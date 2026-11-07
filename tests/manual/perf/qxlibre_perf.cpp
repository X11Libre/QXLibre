// Prints one CSV line of timing and X traffic for a software-rendered window scenario.
// Usage: qxlibre_perf <fullrepaint1080p|fullrepaint4k|scrolltext|scrolllist|partial|resize|update> [frames]
//        qxlibre_perf <rasterwindow|quick> [seconds]
//        qxlibre_perf has-quick

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qfile.h>
#include <QtCore/qeventloop.h>
#include <QtCore/qstringlistmodel.h>
#include <QtCore/qtimer.h>
#include <QtGui/qpainter.h>
#include <QtGui/qrasterwindow.h>
#include <QtGui/qscreen.h>
#include <QtGui/qwindow.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qlistview.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qtextedit.h>
#include <qpa/qplatformnativeinterface.h>
#ifdef QXLIBRE_PERF_HAS_QUICK
#include <QtCore/qmutex.h>
#include <QtQml/qqmlapplicationengine.h>
#include <QtQuick/qquickwindow.h>
#endif

#include <xcb/xcb.h>

#include <sys/resource.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>

using namespace Qt::StringLiterals;

namespace {

class PaintedWidget : public QWidget
{
public:
    int frame = 0;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor::fromHsv(frame % 360, 255, 200));
    }
};

double cpuMilliseconds()
{
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_utime.tv_sec * 1e3 + usage.ru_utime.tv_usec / 1e3
            + usage.ru_stime.tv_sec * 1e3 + usage.ru_stime.tv_usec / 1e3;
}

// CPU time of the process named by QXLIBRE_PERF_SERVER_PID, or -1 without it.
double serverCpuMilliseconds()
{
    const QByteArray pid = qgetenv("QXLIBRE_PERF_SERVER_PID");
    QFile file(QString::fromLatin1("/proc/" + pid + "/stat"));
    if (pid.isEmpty() || !file.open(QIODevice::ReadOnly))
        return -1;
    // The name in parentheses can contain spaces; utime and stime are the 12th and 13th fields after it.
    const QByteArray stat = file.readAll();
    const QList<QByteArray> fields = stat.mid(stat.lastIndexOf(')') + 2).split(' ');
    return (fields.value(11).toLongLong() + fields.value(12).toLongLong()) * 1e3 / sysconf(_SC_CLK_TCK);
}

// Makes each frame include the server's work, not just queueing the requests.
void waitForServer(xcb_connection_t *connection)
{
    free(xcb_get_input_focus_reply(connection, xcb_get_input_focus(connection), nullptr));
}

// A window animating itself with requestUpdate(), as far as the platform lets it.
class AnimatedWindow : public QRasterWindow
{
public:
    QList<double> intervals;

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (m_timer.isValid())
            intervals << m_timer.nsecsElapsed() / 1e6;
        m_timer.start();
        QPainter(this).fillRect(QRect(QPoint(), size()), QColor::fromHsv(intervals.size() % 360, 255, 200));
        update();
    }

private:
    QElapsedTimer m_timer;
};

// Prints updates per second, the standard deviation of their interval in ms, client and server CPU
// in ms per second, and the screen's refresh rate.
int measureAnimation(const char *scenario, QWindow &window, int seconds,
                     const std::function<void()> &clearIntervals,
                     const std::function<QList<double>()> &takeIntervals)
{
    window.setFlag(Qt::WindowDoesNotAcceptFocus);
    window.resize(800, 600);
    window.show();
    // A real event loop: spinning on processEvents() would measure the spinning.
    QEventLoop loop;
    const auto runFor = [&loop](int milliseconds) {
        QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
        loop.exec();
    };
    runFor(500);

    clearIntervals();
    const double cpuBefore = cpuMilliseconds();
    const double serverCpuBefore = serverCpuMilliseconds();
    QElapsedTimer timer;
    timer.start();
    runFor(seconds * 1000);
    const double elapsed = timer.elapsed() / 1e3;
    const double cpu = (cpuMilliseconds() - cpuBefore) / elapsed;
    const double serverCpu = (serverCpuMilliseconds() - serverCpuBefore) / elapsed;

    const QList<double> intervals = takeIntervals();
    double mean = 0;
    for (double interval : intervals)
        mean += interval;
    mean /= std::max<qsizetype>(intervals.size(), 1);
    double variance = 0;
    for (double interval : intervals)
        variance += (interval - mean) * (interval - mean);
    variance /= std::max<qsizetype>(intervals.size(), 1);

    printf("%s,%s,%d,%.1f,%.3f,%.1f,%.1f,%.1f\n", scenario,
           qEnvironmentVariable("QT_QPA_PLATFORM").toUtf8().constData(), seconds,
           intervals.size() / elapsed, std::sqrt(variance), cpu, serverCpu,
           window.screen()->refreshRate());
    return 0;
}

int runRasterWindow(int seconds)
{
    AnimatedWindow window;
    return measureAnimation("rasterwindow", window, seconds, [&window] { window.intervals.clear(); },
                            [&window] { return window.intervals; });
}

#ifdef QXLIBRE_PERF_HAS_QUICK
// Timed from the swaps, which can happen on Qt Quick's render thread. QT_QUICK_BACKEND picks how it
// draws.
int runQuick(int seconds)
{
    // Before the engine, because its render thread can still be swapping while it's destroyed.
    QMutex mutex;
    QList<double> intervals;
    QElapsedTimer swaps;

    QQmlApplicationEngine engine;
    engine.loadData(R"(
        import QtQuick
        Window {
            color: "black"
            Rectangle {
                width: 200; height: 200; y: 200; color: "steelblue"
                NumberAnimation on x { from: 0; to: 600; duration: 2000; loops: Animation.Infinite }
                RotationAnimation on rotation { from: 0; to: 360; duration: 2000; loops: Animation.Infinite }
            }
        }
    )");
    auto *window = engine.rootObjects().isEmpty()
            ? nullptr
            : qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (!window) {
        fprintf(stderr, "qxlibre_perf: the scene didn't load\n");
        return 3;
    }

    QObject::connect(window, &QQuickWindow::frameSwapped, window, [&] {
        const QMutexLocker locker(&mutex);
        if (swaps.isValid())
            intervals << swaps.nsecsElapsed() / 1e6;
        swaps.start();
    }, Qt::DirectConnection);

    return measureAnimation("quick", *window, seconds,
                            [&] {
                                const QMutexLocker locker(&mutex);
                                intervals.clear();
                            },
                            [&] {
                                const QMutexLocker locker(&mutex);
                                return intervals;
                            });
}
#endif

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QString scenario = argc > 1 ? QString::fromLatin1(argv[1]) : QString();
    const int frames = argc > 2 ? atoi(argv[2]) : 600;

    auto *native = QGuiApplication::platformNativeInterface();
    auto *connection = static_cast<xcb_connection_t *>(native->nativeResourceForIntegration("connection"));
    if (!connection) {
        fprintf(stderr, "qxlibre_perf: needs an X11 platform\n");
        return 2;
    }

    std::unique_ptr<QWidget> widget;
    std::function<void(int)> step;

    if (scenario == "rasterwindow"_L1)
        return runRasterWindow(frames);
#ifdef QXLIBRE_PERF_HAS_QUICK
    if (scenario == "has-quick"_L1)
        return 0;
    if (scenario == "quick"_L1)
        return runQuick(frames);
#else
    if (scenario == "has-quick"_L1 || scenario == "quick"_L1) {
        fprintf(stderr, "qxlibre_perf: built without Qt Quick\n");
        return 4;
    }
#endif

    if (scenario == "fullrepaint1080p"_L1 || scenario == "fullrepaint4k"_L1) {
        auto *painted = new PaintedWidget;
        widget.reset(painted);
        widget->resize(scenario == "fullrepaint4k"_L1 ? QSize(3840, 2160) : QSize(1920, 1080));
        step = [painted](int frame) {
            painted->frame = frame;
            painted->repaint();
        };
    } else if (scenario == "update"_L1) {
        auto *painted = new PaintedWidget;
        widget.reset(painted);
        widget->resize(800, 600);
        step = [painted](int frame) {
            painted->frame = frame;
            painted->update();
        };
    } else if (scenario == "partial"_L1) {
        auto *painted = new PaintedWidget;
        widget.reset(painted);
        widget->resize(800, 600);
        step = [painted](int frame) {
            painted->frame = frame;
            painted->repaint(40, 40, 2, 18); // the size of a text cursor
        };
    } else if (scenario == "scrolltext"_L1) {
        QString text;
        for (int i = 0; i < 20000; ++i)
            text += u"Line "_s + QString::number(i) + u" of the document being scrolled\n"_s;
        auto *edit = new QTextEdit;
        edit->setPlainText(text);
        widget.reset(edit);
        widget->resize(800, 600);
        step = [edit](int frame) {
            auto *bar = edit->verticalScrollBar();
            bar->setValue((frame * bar->singleStep()) % bar->maximum());
            edit->viewport()->repaint();
        };
    } else if (scenario == "scrolllist"_L1) {
        QStringList rows;
        for (int i = 0; i < 20000; ++i)
            rows << u"Row "_s + QString::number(i);
        auto *view = new QListView;
        view->setModel(new QStringListModel(rows, view));
        widget.reset(view);
        widget->resize(800, 600);
        step = [view](int frame) {
            auto *bar = view->verticalScrollBar();
            bar->setValue(frame % bar->maximum());
            view->viewport()->repaint();
        };
    } else if (scenario == "resize"_L1) {
        auto *painted = new PaintedWidget;
        widget.reset(painted);
        widget->resize(800, 600);
        step = [painted](int frame) {
            painted->frame = frame;
            painted->resize(800 + frame % 100, 600 + frame % 100);
            QCoreApplication::processEvents();
            painted->repaint();
        };
    } else {
        fprintf(stderr, "qxlibre_perf: unknown scenario \"%s\"\n", qPrintable(scenario));
        return 2;
    }

    // Runs on a desktop in use shouldn't take the keyboard focus.
    widget->setAttribute(Qt::WA_ShowWithoutActivating);
    widget->show();
    // Nothing is drawn until then, and a run that draws nothing looks very fast.
    QElapsedTimer exposure;
    exposure.start();
    while (!widget->windowHandle()->isExposed() && exposure.elapsed() < 5000) {
        QCoreApplication::processEvents();
        waitForServer(connection);
    }
    if (!widget->windowHandle()->isExposed()) {
        fprintf(stderr, "qxlibre_perf: the window was never exposed\n");
        return 3;
    }
    for (int i = 0; i < 50; ++i) {
        QCoreApplication::processEvents();
        waitForServer(connection);
    }

    using SyncCount = void *(*)();
    const auto syncFunction = reinterpret_cast<SyncCount>(
            native->nativeResourceFunctionForIntegration("qxlibresynccount"));
    const quint64 bytesBefore = xcb_total_written(connection);
    const quintptr syncsBefore = syncFunction ? quintptr(syncFunction()) : 0;
    const double serverCpuBefore = serverCpuMilliseconds();
    const double cpuBefore = cpuMilliseconds();

    // Without pacing, a client that throttles to the display would skip most of these frames.
    const int pace = qEnvironmentVariableIntValue("QXLIBRE_PERF_PACE_MS");

    QList<double> times;
    times.reserve(frames);
    QElapsedTimer timer;
    // From an event loop, as in an application, so events from the server are handled between frames.
    QEventLoop loop;
    QTimer ticker;
    ticker.setTimerType(Qt::PreciseTimer);
    ticker.setInterval(pace);
    int frame = 0;
    QObject::connect(&ticker, &QTimer::timeout, &loop, [&] {
        timer.start();
        step(frame);
        QCoreApplication::processEvents();
        waitForServer(connection);
        times << timer.nsecsElapsed() / 1e6;
        if (++frame == frames) {
            ticker.stop();
            loop.quit();
        }
    });
    ticker.start();
    loop.exec();

    const double cpu = cpuMilliseconds() - cpuBefore;
    // Work the server hasn't finished yet belongs to these frames too.
    QElapsedTimer settle;
    settle.start();
    while (settle.elapsed() < 100) {
        QCoreApplication::processEvents();
        waitForServer(connection);
    }
    const double serverCpu = serverCpuMilliseconds() - serverCpuBefore;
    const quint64 bytes = xcb_total_written(connection) - bytesBefore;
    std::sort(times.begin(), times.end());

    // Syncs can only be counted by QXLibre, with QT_XLIBRE_DEBUG set: otherwise "-".
    const QByteArray syncs = syncFunction && qEnvironmentVariableIsSet("QT_XLIBRE_DEBUG")
            ? QByteArray::number((quintptr(syncFunction()) - syncsBefore) / double(frames), 'f', 2)
            : QByteArray("-");

    printf("%s,%s,%d,%.3f,%.3f,%.3f,%s,%.0f,%.3f\n", qPrintable(scenario),
           qEnvironmentVariable("QT_QPA_PLATFORM").toUtf8().constData(), frames,
           times.at(frames / 2), times.at(frames * 99 / 100), cpu / frames, syncs.constData(),
           double(bytes) / frames, serverCpu / frames);
    return 0;
}

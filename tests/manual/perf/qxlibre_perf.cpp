// Prints one CSV line of timing and X traffic for a software-rendered window scenario.
// Usage: qxlibre_perf <fullrepaint1080p|fullrepaint4k|scrolltext|scrolllist|partial|resize> [frames]

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qstringlistmodel.h>
#include <QtGui/qpainter.h>
#include <QtGui/qscreen.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qlistview.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qtextedit.h>
#include <qpa/qplatformnativeinterface.h>

#include <xcb/xcb.h>

#include <sys/resource.h>

#include <algorithm>
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

// Makes each frame include the server's work, not just queueing the requests.
void waitForServer(xcb_connection_t *connection)
{
    free(xcb_get_input_focus_reply(connection, xcb_get_input_focus(connection), nullptr));
}

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

    if (scenario == "fullrepaint1080p"_L1 || scenario == "fullrepaint4k"_L1) {
        auto *painted = new PaintedWidget;
        widget.reset(painted);
        widget->resize(scenario == "fullrepaint4k"_L1 ? QSize(3840, 2160) : QSize(1920, 1080));
        step = [painted](int frame) {
            painted->frame = frame;
            painted->repaint();
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

    widget->show();
    for (int i = 0; i < 50; ++i) {
        QCoreApplication::processEvents();
        waitForServer(connection);
    }

    using SyncCount = void *(*)();
    const auto syncFunction = reinterpret_cast<SyncCount>(
            native->nativeResourceFunctionForIntegration("qxlibresynccount"));
    const quint64 bytesBefore = xcb_total_written(connection);
    const quintptr syncsBefore = syncFunction ? quintptr(syncFunction()) : 0;
    const double cpuBefore = cpuMilliseconds();

    QList<double> times;
    times.reserve(frames);
    QElapsedTimer timer;
    for (int frame = 0; frame < frames; ++frame) {
        timer.start();
        step(frame);
        QCoreApplication::processEvents();
        waitForServer(connection);
        times << timer.nsecsElapsed() / 1e6;
    }

    const double cpu = cpuMilliseconds() - cpuBefore;
    const quint64 bytes = xcb_total_written(connection) - bytesBefore;
    std::sort(times.begin(), times.end());

    // Syncs can only be counted by QXLibre, with QT_XLIBRE_DEBUG set: otherwise "-".
    const QByteArray syncs = syncFunction && qEnvironmentVariableIsSet("QT_XLIBRE_DEBUG")
            ? QByteArray::number((quintptr(syncFunction()) - syncsBefore) / double(frames), 'f', 2)
            : QByteArray("-");

    printf("%s,%s,%d,%.3f,%.3f,%.3f,%s,%.0f\n", qPrintable(scenario),
           qEnvironmentVariable("QT_QPA_PLATFORM").toUtf8().constData(), frames,
           times.at(frames / 2), times.at(frames * 99 / 100), cpu / frames, syncs.constData(),
           double(bytes) / frames);
    return 0;
}

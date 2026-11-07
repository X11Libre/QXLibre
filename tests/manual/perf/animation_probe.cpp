// Diagnostic only. Logs each animation tick and each frame of a software-backend Qt Quick scene, and
// can change the animation timer's interval, to test whether that timer causes the late frames.
// Usage: probe <animation timer ms> <seconds>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/private/qabstractanimation_p.h>
#include <QtCore/qtimer.h>
#include <QtGui/qguiapplication.h>
#include <QtQml/qqmlapplicationengine.h>
#include <QtQuick/qquickitem.h>
#include <QtQuick/qquickwindow.h>
#include <sys/resource.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

static double cpuMs()
{
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_utime.tv_sec * 1e3 + usage.ru_utime.tv_usec / 1e3
            + usage.ru_stime.tv_sec * 1e3 + usage.ru_stime.tv_usec / 1e3;
}

// Prototype: animations advance once per frame, instead of from their own timer. Advancing dirties
// the scene, which requests the next update, so a frame is what schedules the next frame.
class FrameDriver : public QAnimationDriver
{
public:
    using QAnimationDriver::QAnimationDriver;
};

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const int interval = argc > 1 ? atoi(argv[1]) : 16;
    const int seconds = argc > 2 ? atoi(argv[2]) : 5;
    const bool frameDriven = argc > 3 && !strcmp(argv[3], "framedriver");
    QUnifiedTimer::instance()->setTimingInterval(interval);
    FrameDriver driver;
    if (frameDriven)
        driver.install();

    QElapsedTimer clock;
    clock.start();
    QList<double> ticks, frames;
    bool recording = false;

    QQmlApplicationEngine engine;
    engine.loadData(R"(
        import QtQuick
        Window {
            color: "black"
            Rectangle {
                objectName: "mover"
                width: 200; height: 200; y: 200; color: "steelblue"
                NumberAnimation on x { from: 0; to: 600; duration: 2000; loops: Animation.Infinite }
                RotationAnimation on rotation { from: 0; to: 360; duration: 2000; loops: Animation.Infinite }
            }
        }
    )");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    auto *mover = window->findChild<QQuickItem *>("mover");
    QObject::connect(mover, &QQuickItem::xChanged, mover, [&] {
        if (recording)
            ticks << clock.nsecsElapsed() / 1e6;
    });
    QObject::connect(window, &QQuickWindow::frameSwapped, window, [&] {
        if (recording)
            frames << clock.nsecsElapsed() / 1e6;
        if (frameDriven && driver.isRunning()) {
            driver.advance();
            // An advance with no time passed changes nothing, and the next frame must still come.
            window->update();
        }
    }, Qt::DirectConnection);
    if (frameDriven) {
        QObject::connect(&driver, &QAnimationDriver::started, window, &QQuickWindow::update);
        QObject::connect(window, &QWindow::visibleChanged, window, &QQuickWindow::update);
    }

    window->setFlag(Qt::WindowDoesNotAcceptFocus);
    window->resize(800, 600);
    window->show();

    double cpuBefore = 0;
    double recordingStart = 0;
    QTimer::singleShot(500, &app, [&] {
        recording = true;
        recordingStart = clock.nsecsElapsed() / 1e6;
        cpuBefore = cpuMs();
    });
    QTimer::singleShot(500 + seconds * 1000, &app, &QCoreApplication::quit);
    app.exec();
    // From the clock: these single-shot timers are coarse, so the run isn't exactly as long as asked.
    const double elapsed = (clock.nsecsElapsed() / 1e6 - recordingStart) / 1e3;
    const double cpu = (cpuMs() - cpuBefore) / elapsed;

    double mean = 0;
    QList<double> gaps;
    for (qsizetype i = 1; i < frames.size(); ++i) {
        gaps << frames[i] - frames[i - 1];
        mean += gaps.last();
    }
    mean /= std::max<qsizetype>(gaps.size(), 1);
    double variance = 0;
    for (double gap : gaps)
        variance += (gap - mean) * (gap - mean);
    variance /= std::max<qsizetype>(gaps.size(), 1);

    // A late frame is a gap of about two refreshes. Count the ticks inside each one.
    int late = 0, noTick = 0, oneTick = 0, more = 0;
    QByteArray samples;
    for (qsizetype i = 0; i < gaps.size(); ++i) {
        if (gaps[i] < 25)
            continue;
        ++late;
        int inside = 0;
        QByteArray offsets;
        for (double tick : ticks) {
            if (tick > frames[i] && tick <= frames[i + 1]) {
                ++inside;
                offsets += ' ' + QByteArray::number(tick - frames[i], 'f', 1);
            }
        }
        inside == 0 ? ++noTick : inside == 1 ? ++oneTick : ++more;
        if (late <= 4)
            samples += " [gap " + QByteArray::number(gaps[i], 'f', 1) + " ticks at" + offsets + "]";
    }

    printf("%s", frameDriven ? "frame-driven " : "");
    printf("timer %2d ms: frames/s %5.1f sd %6.3f cpu %5.1f ms/s ticks/s %6.1f late %3d (0 ticks %d, 1 tick %d, 2+ %d)%s\n",
           interval, gaps.size() / elapsed, std::sqrt(variance), cpu,
           ticks.size() / elapsed, late, noTick, oneTick, more, samples.constData());
    return 0;
}

// A client for tst_present. It runs one scenario and prints what the test checks.

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qthread.h>
#include <QtCore/qtimer.h>
#include <QtGui/qevent.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qopenglcontext.h>
#include <QtGui/qpainter.h>
#include <QtGui/qrasterwindow.h>
#include <QtGui/qwindow.h>
#include <qpa/qplatformnativeinterface.h>

#include <xcb/present.h>
#include <xcb/xcb.h>

#include <cstdio>
#include <cstdlib>

static void spin(int milliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// The plugin's own connection.
static xcb_connection_t *connection()
{
    return static_cast<xcb_connection_t *>(
            QGuiApplication::platformNativeInterface()->nativeResourceForIntegration("connection"));
}

class Animated : public QRasterWindow
{
public:
    int paints = 0;

protected:
    void paintEvent(QPaintEvent *) override
    {
        ++paints;
        QPainter(this).fillRect(QRect(QPoint(), size()), Qt::blue);
        update();
    }
};

class UpdateWatcher : public QWindow
{
public:
    bool updated = false;

protected:
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::UpdateRequest)
            updated = true;
        return QWindow::event(event);
    }
};

static int runAnimation()
{
    Animated window;
    window.resize(100, 100);
    window.show();
    spin(300);
    const int before = window.paints;
    QElapsedTimer timer;
    timer.start();
    spin(1000);
    printf("updates-per-second: %d\n",
           qRound((window.paints - before) * 1000.0 / timer.elapsed()));
    return 0;
}

// Exits from the paint because a starved event loop would never run a timer to end the run.
class SlowWindow : public QRasterWindow
{
public:
    int timerFires = 0;

protected:
    void paintEvent(QPaintEvent *) override
    {
        update();
        QPainter(this).fillRect(QRect(QPoint(), size()), Qt::blue);
        QThread::msleep(40);
        if (!m_clock.isValid())
            m_clock.start();
        if (m_clock.elapsed() > 1500) {
            printf("timer-fires: %d\n", timerFires);
            fflush(stdout);
            std::_Exit(0);
        }
    }

private:
    QElapsedTimer m_clock;
};

static int runSlowFrames()
{
    SlowWindow window;
    window.resize(100, 100);
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, &window, [&window] { ++window.timerFires; });
    timer.start(100);
    window.show();
    return QCoreApplication::exec();
}

// A window that isn't on a CRTC can wait for a fake vblank, as slow as 1 Hz.
static void printDelivery(const UpdateWatcher &window)
{
    QElapsedTimer timer;
    timer.start();
    while (!window.updated && timer.elapsed() < 3000)
        spin(10);
    printf("update-delivered: %s\n", window.updated ? "yes" : "no");
}

static int runUpdateWhileHidden()
{
    UpdateWatcher window;
    window.resize(100, 100);
    window.create();
    spin(100);
    window.updated = false;
    window.requestUpdate();
    printDelivery(window);
    return 0;
}

static int runUpdateAfterError()
{
    UpdateWatcher window;
    window.resize(100, 100);
    window.show();
    spin(100);
    window.updated = false;
    window.requestUpdate();

    // A remainder without a divisor is a BadValue, on a window that is still there.
    xcb_present_notify_msc(connection(), window.winId(), 0, 0, 0, 1);
    xcb_flush(connection());

    printDelivery(window);
    return runAnimation();
}

static int runUpdateAfterWindowLoss()
{
    UpdateWatcher window;
    window.resize(100, 100);
    window.show();
    spin(100);
    window.updated = false;

    xcb_destroy_window(connection(), window.winId());
    window.requestUpdate();

    printDelivery(window);
    return runAnimation();
}

static int runUpdateAcrossRecreation()
{
    UpdateWatcher window;
    window.resize(100, 100);
    window.show();
    spin(100);
    window.hide();
    // The next show() replaces the native window, while the update is waiting on the old one.
    window.setFlags(window.flags() ^ Qt::WindowStaysOnTopHint);
    window.updated = false;
    window.requestUpdate();
    const WId before = window.winId();
    window.show();
    if (window.winId() == before)
        return 3;

    printDelivery(window);
    return 0;
}

// Notifications asked for by anyone else reach our selection too, and these come right away.
static int runOtherNotifications()
{
    QTimer noise;
    QObject::connect(&noise, &QTimer::timeout, &noise, [] {
        const auto windows = QGuiApplication::allWindows();
        for (QWindow *window : windows) {
            if (window->handle())
                xcb_present_notify_msc(connection(), window->winId(), 0, 0, 0, 0);
        }
        xcb_flush(connection());
    });
    noise.start(1);
    return runAnimation();
}

class OpenGLSurfaceWindow : public QWindow
{
public:
    int updates = 0;

    OpenGLSurfaceWindow() { setSurfaceType(OpenGLSurface); }

protected:
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::UpdateRequest) {
            ++updates;
            requestUpdate();
        }
        return QWindow::event(event);
    }
};

static int runOpenGLSurface()
{
    QOpenGLContext context;
    if (!context.create()) {
        printf("no-opengl: yes\n");
        return 0;
    }

    OpenGLSurfaceWindow window;
    window.resize(100, 100);
    window.show();
    window.requestUpdate();
    spin(300);
    const int before = window.updates;
    QElapsedTimer timer;
    timer.start();
    spin(1000);
    printf("updates-per-second: %d\n",
           qRound((window.updates - before) * 1000.0 / timer.elapsed()));
    return 0;
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const QByteArray scenario = argc > 1 ? argv[1] : "";

    if (scenario == "animation")
        return runAnimation();
    if (scenario == "slow-frames")
        return runSlowFrames();
    if (scenario == "update-hidden")
        return runUpdateWhileHidden();
    if (scenario == "update-error")
        return runUpdateAfterError();
    if (scenario == "update-window-lost")
        return runUpdateAfterWindowLoss();
    if (scenario == "update-recreated")
        return runUpdateAcrossRecreation();
    if (scenario == "other-notifications")
        return runOtherNotifications();
    if (scenario == "opengl-surface")
        return runOpenGLSurface();
    return 2;
}

// A client for tst_present. It runs one scenario and prints what the test checks.

#include <QtCore/qelapsedtimer.h>
#include <QtGui/qevent.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qpainter.h>
#include <QtGui/qrasterwindow.h>
#include <QtGui/qwindow.h>
#include <qpa/qplatformnativeinterface.h>

#include <xcb/present.h>
#include <xcb/xcb.h>

#include <cstdio>

static void spin(int milliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// A request the server refuses, on the plugin's own connection.
static void sendBadPresent()
{
    auto *connection = static_cast<xcb_connection_t *>(
            QGuiApplication::platformNativeInterface()->nativeResourceForIntegration("connection"));
    xcb_present_pixmap(connection, 0x1fffffff, 0x1fffffff, 1, XCB_NONE, XCB_NONE, 0, 0, XCB_NONE,
                       XCB_NONE, XCB_NONE, XCB_PRESENT_OPTION_NONE, 0, 0, 0, 0, nullptr);
    xcb_flush(connection);
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

static int runUpdateRequest(bool shown, bool withError)
{
    UpdateWatcher window;
    window.resize(100, 100);
    if (shown)
        window.show();
    else
        window.create();
    spin(100);
    window.updated = false;
    window.requestUpdate();
    if (withError)
        sendBadPresent();
    // A window that isn't on a CRTC can wait for a fake vblank, as slow as 1 Hz.
    QElapsedTimer timer;
    timer.start();
    while (!window.updated && timer.elapsed() < 3000)
        spin(10);
    printf("update-delivered: %s\n", window.updated ? "yes" : "no");
    return 0;
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const QByteArray scenario = argc > 1 ? argv[1] : "";

    if (scenario == "animation")
        return runAnimation();
    if (scenario == "update-hidden")
        return runUpdateRequest(false, false);
    if (scenario == "update-error")
        return runUpdateRequest(true, true);
    return 2;
}

// A client for tst_xnamespace. Each mode prints its result to stdout.

#include <QtCore/qelapsedtimer.h>
#include <QtGui/qclipboard.h>
#include <QtGui/qimage.h>
#include <QtGui/qpainter.h>
#include <QtGui/qscreen.h>
#include <QtQml/qqmlapplicationengine.h>
#include <QtQuick/qquickwindow.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qlineedit.h>

#include <xcb/xcb.h>
#include <xcb/xtest.h>

#include <unistd.h>

#include <cstdio>

using namespace Qt::StringLiterals;

static void spin(int milliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
}

class ColourWidget : public QWidget
{
public:
    bool painted = false;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter(this).fillRect(rect(), Qt::green);
        painted = true;
    }
};

static int runWindow()
{
    ColourWidget widget;
    widget.resize(100, 100);
    widget.show();
    spin(500);
    printf("window: %s\n", widget.painted ? "painted" : "not-painted");
    return 0;
}

static int runTranslucent()
{
    ColourWidget widget;
    widget.setAttribute(Qt::WA_TranslucentBackground);
    widget.resize(100, 100);
    widget.show();
    spin(500);
    printf("translucent: %s\n", widget.painted ? "painted" : "not-painted");
    return 0;
}

static int runQuick()
{
    QQmlApplicationEngine engine;
    engine.loadData(R"(
import QtQuick
Window { width: 100; height: 100; visible: true; color: "#ff0000" }
)");
    if (engine.rootObjects().isEmpty())
        return 3;
    spin(500);
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    const QImage image = window->grabWindow();
    printf("quick: %s\n", image.isNull() ? "null" : qPrintable(image.pixelColor(50, 50).name()));
    return 0;
}

static int runOwn(const char *text)
{
    QApplication::clipboard()->setText(QString::fromLatin1(text));
    printf("owning\n");
    fflush(stdout);
    return QCoreApplication::exec();
}

static int runRead()
{
    printf("clipboard: [%s]\n", qPrintable(QApplication::clipboard()->text()));
    return 0;
}

static int runType()
{
    QLineEdit edit;
    edit.move(0, 0);
    edit.show();
    edit.setFocus();
    spin(300);
    printf("ready %llu\n", static_cast<unsigned long long>(edit.winId()));
    fflush(stdout);
    QElapsedTimer timer;
    timer.start();
    while (edit.text().isEmpty() && timer.elapsed() < 5000)
        spin(50);
    printf("typed: [%s]\n", qPrintable(edit.text()));
    return 0;
}

static int runScreen()
{
    const QSize size = QGuiApplication::primaryScreen()->size();
    printf("screen: %dx%d\n", size.width(), size.height());
    return 0;
}

// A control for runType, without Qt: does this server deliver keys into a namespace at all?
static int runRawType()
{
    xcb_connection_t *connection = xcb_connect(nullptr, nullptr);
    if (xcb_connection_has_error(connection))
        return 4;
    const xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    const xcb_window_t window = xcb_generate_id(connection);
    const uint32_t eventMask = XCB_EVENT_MASK_KEY_PRESS;
    xcb_create_window(connection, XCB_COPY_FROM_PARENT, window, screen->root, 0, 0, 100, 100, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, XCB_CW_EVENT_MASK,
                      &eventMask);
    xcb_map_window(connection, window);
    free(xcb_get_input_focus_reply(connection, xcb_get_input_focus(connection), nullptr));
    printf("ready %u\n", window);
    fflush(stdout);

    bool typed = false;
    QElapsedTimer timer;
    timer.start();
    while (!typed && timer.elapsed() < 3000) {
        xcb_generic_event_t *event = xcb_poll_for_event(connection);
        if (!event) {
            usleep(20000);
            continue;
        }
        typed = (event->response_type & 0x7f) == XCB_KEY_PRESS;
        free(event);
    }
    printf("rawtyped: %s\n", typed ? "yes" : "no");
    xcb_disconnect(connection);
    return 0;
}

// Runs as a client outside the namespace, without Qt, because tests can't inject keys from inside.
// It gives the window focus first, as a window manager would.
static int runSendKey(xcb_window_t window, int keycode)
{
    xcb_connection_t *connection = xcb_connect(nullptr, nullptr);
    if (xcb_connection_has_error(connection))
        return 4;
    xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT, window, XCB_CURRENT_TIME);
    free(xcb_get_input_focus_reply(connection, xcb_get_input_focus(connection), nullptr));
    xcb_test_fake_input(connection, XCB_KEY_PRESS, keycode, XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0);
    xcb_test_fake_input(connection, XCB_KEY_RELEASE, keycode, XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0);
    free(xcb_get_input_focus_reply(connection, xcb_get_input_focus(connection), nullptr));
    xcb_disconnect(connection);
    return 0;
}

int main(int argc, char **argv)
{
    const QByteArray mode = argc > 1 ? argv[1] : "";
    if (mode == "sendkey")
        return runSendKey(argc > 2 ? xcb_window_t(strtoul(argv[2], nullptr, 10)) : 0,
                          argc > 3 ? atoi(argv[3]) : 38);
    if (mode == "rawtype")
        return runRawType();

    QApplication app(argc, argv);
    if (mode == "window")
        return runWindow();
    if (mode == "translucent")
        return runTranslucent();
    if (mode == "quick")
        return runQuick();
    if (mode == "own")
        return runOwn(argc > 2 ? argv[2] : "");
    if (mode == "read")
        return runRead();
    if (mode == "type")
        return runType();
    if (mode == "screen")
        return runScreen();
    return 2;
}

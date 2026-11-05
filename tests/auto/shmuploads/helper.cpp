// A client for tst_shmuploads. It runs one scenario and prints what the test compares.

#include <QtCore/qcryptographichash.h>
#include <QtCore/qelapsedtimer.h>
#include <QtGui/qevent.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qwidget.h>
#include <qpa/qplatformnativeinterface.h>

#include <xcb/xcb.h>

#include <cstdio>
#include <cstdlib>

static void spin(int milliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
}

static quintptr syncCount()
{
    using Function = void *(*)();
    auto function = reinterpret_cast<Function>(
            QGuiApplication::platformNativeInterface()->nativeResourceFunctionForIntegration(
                    "qxlibresynccount"));
    return function ? quintptr(function()) : 0;
}

// Paints bands that depend on position and on a value, so partial updates and scrolling show up.
class Pattern : public QWidget
{
public:
    int value = 0;
    int scrolled = 0;
    bool translucent = false;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPainter painter(this);
        if (translucent)
            painter.setCompositionMode(QPainter::CompositionMode_Source);
        for (const QRect &rect : event->region()) {
            for (int y = rect.top(); y <= rect.bottom(); ++y) {
                const int band = (y - scrolled) / 8;
                QColor colour = QColor::fromHsv((band * 37 + value * 11) & 255, 200, 200);
                if (translucent)
                    colour.setAlpha(128);
                painter.fillRect(rect.left(), y, rect.width(), 1, colour);
            }
        }
    }
};

static QByteArray windowHash(xcb_window_t window, const QSize &size)
{
    xcb_connection_t *connection = xcb_connect(nullptr, nullptr);
    auto *reply = xcb_get_image_reply(
            connection,
            xcb_get_image(connection, XCB_IMAGE_FORMAT_Z_PIXMAP, window, 0, 0, size.width(),
                          size.height(), ~0u),
            nullptr);
    QByteArray hash = "no-image";
    if (reply) {
        const QByteArrayView data(reinterpret_cast<const char *>(xcb_get_image_data(reply)),
                                  xcb_get_image_data_length(reply));
        hash = QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex();
        free(reply);
    }
    xcb_disconnect(connection);
    return hash;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const QByteArray scenario = argc > 1 ? argv[1] : "";

    Pattern pattern;
    pattern.translucent = scenario == "translucent";
    if (pattern.translucent)
        pattern.setAttribute(Qt::WA_TranslucentBackground);
    pattern.setAttribute(Qt::WA_OpaquePaintEvent, !pattern.translucent);
    pattern.resize(320, 240);
    pattern.move(0, 0);
    pattern.show();
    spin(100);

    constexpr int frames = 40;
    const quintptr before = syncCount();

    if (scenario == "idle-frames") {
        // Events are handled between frames, so each upload's completion has arrived.
        for (int i = 1; i <= frames; ++i) {
            pattern.value = i;
            pattern.repaint();
            spin(5);
        }
        printf("syncs-per-frame: %.2f\n", (syncCount() - before) / double(frames));
    } else if (scenario == "rapid") {
        // No events are handled between frames, so nothing says the server has finished reading.
        for (int i = 1; i <= frames; ++i) {
            pattern.value = i;
            pattern.repaint();
        }
        printf("syncs-per-frame: %.2f\n", (syncCount() - before) / double(frames));
    } else if (scenario == "two-windows") {
        // The other window's completion is older than the first repaint here, so only the
        // second repaint has to wait.
        Pattern other;
        other.resize(320, 240);
        other.move(400, 0);
        other.show();
        spin(100);
        const quintptr start = syncCount();
        for (int i = 1; i <= frames; ++i) {
            other.value = i;
            other.repaint();
            spin(5);
            pattern.value = i;
            pattern.repaint();
            pattern.value = -i;
            pattern.repaint();
        }
        printf("syncs-per-frame: %.2f\n", (syncCount() - start) / double(frames));
    } else if (scenario == "partial") {
        for (int i = 1; i <= frames; ++i) {
            pattern.value = i;
            pattern.repaint(i * 5, i * 3, 40, 30);
            if (i % 2)
                spin(5);
        }
    } else if (scenario == "scroll") {
        for (int i = 1; i <= 12; ++i) {
            pattern.scrolled += 16;
            pattern.scroll(0, 16);
            if (i % 2)
                spin(5);
        }
    } else if (scenario == "resize") {
        for (const QSize &size : { QSize(400, 300), QSize(200, 120), QSize(1000, 800), QSize(360, 260) }) {
            pattern.resize(size);
            spin(30);
            pattern.value += 1;
            pattern.repaint();
        }
    } else if (scenario == "hide") {
        pattern.value = 3;
        pattern.repaint();
        pattern.hide();
        spin(30);
        pattern.show();
    } else if (scenario == "translucent") {
        for (int i = 1; i <= 10; ++i) {
            pattern.value = i;
            pattern.repaint();
            spin(5);
        }
        printf("syncs-per-frame: %.2f\n", (syncCount() - before) / 10.0);
    }

    // Long enough for anything still queued to reach the window.
    spin(100);
    printf("hash: %s\n", windowHash(pattern.winId(), pattern.size()).constData());
    return 0;
}

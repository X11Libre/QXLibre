// Diagnostic only: a window that asks for updates from a timer, as Qt's animation driver does,
// rather than from inside its paint event.
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qmap.h>
#include <QtCore/qtimer.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qpainter.h>
#include <QtGui/qrasterwindow.h>
#include <cstdio>

class Window : public QRasterWindow
{
public:
    QElapsedTimer clock;
    qint64 last = 0;
    qint64 requested = 0;
    int paints = 0;
    QMap<int, int> gaps;
    QMap<int, int> latency;

    void tick()
    {
        requested = clock.nsecsElapsed();
        requestUpdate();
    }

protected:
    bool event(QEvent *e) override
    {
        if (e->type() == QEvent::UpdateRequest) {
            const qint64 now = clock.nsecsElapsed();
            if (last) {
                ++paints;
                ++gaps[int((now - last) / 1000000)];
                ++latency[int((now - requested) / 1000000)];
            }
            last = now;
        }
        return QRasterWindow::event(e);
    }
    void paintEvent(QPaintEvent *) override { }
};

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    Window window;
    window.resize(800, 600);
    window.show();
    window.clock.start();
    QTimer ticker;
    ticker.setTimerType(Qt::PreciseTimer);
    QObject::connect(&ticker, &QTimer::timeout, &window, &Window::tick);
    ticker.start(argc > 1 ? atoi(argv[1]) : 16);
    QTimer::singleShot(3000, &app, &QCoreApplication::quit);
    app.exec();
    printf("updates/s %.1f gaps(ms:count)", window.paints / 3.0);
    for (auto it = window.gaps.cbegin(); it != window.gaps.cend(); ++it)
        printf(" %d:%d", it.key(), it.value());
    printf(" | request-to-delivery(ms:count)");
    for (auto it = window.latency.cbegin(); it != window.latency.cend(); ++it)
        printf(" %d:%d", it.key(), it.value());
    printf("\n");
    return 0;
}

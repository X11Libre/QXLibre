// Prints the _NET_WM_WINDOW_TYPE of each Qt Quick popup window, and the colour a window renders.

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qset.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qimage.h>
#include <QtGui/qwindow.h>
#include <QtQml/qqmlapplicationengine.h>
#include <QtQuick/qquickwindow.h>
#include <qpa/qplatformnativeinterface.h>

#include <xcb/xcb.h>

#include <cstdio>
#include <cstdlib>

using namespace Qt::StringLiterals;

static const char *const qmlSource = R"(
import QtQuick
import QtQuick.Controls
ApplicationWindow {
    width: 200; height: 200; visible: true; color: "#ff0000"
    Menu { objectName: "menu"; popupType: Popup.Window; MenuItem { text: "item" } }
    ToolTip { objectName: "tooltip"; popupType: Popup.Window; text: "tip" }
    Dialog { objectName: "dialog"; popupType: Popup.Window; width: 50; height: 50 }
    ComboBox {
        objectName: "combobox"; model: ["a", "b"]
        Component.onCompleted: popup.popupType = Popup.Window
    }
}
)";

static xcb_atom_t internAtom(xcb_connection_t *c, const char *name)
{
    auto cookie = xcb_intern_atom(c, false, uint16_t(qstrlen(name)), name);
    auto *reply = xcb_intern_atom_reply(c, cookie, nullptr);
    const xcb_atom_t atom = reply ? reply->atom : xcb_atom_t(XCB_ATOM_NONE);
    free(reply);
    return atom;
}

static QByteArray windowTypes(xcb_connection_t *c, xcb_window_t window)
{
    const xcb_atom_t property = internAtom(c, "_NET_WM_WINDOW_TYPE");
    auto cookie = xcb_get_property(c, false, window, property, XCB_ATOM_ATOM, 0, 32);
    auto *reply = xcb_get_property_reply(c, cookie, nullptr);
    if (!reply)
        return "none";

    QByteArrayList names;
    const auto *atoms = static_cast<const xcb_atom_t *>(xcb_get_property_value(reply));
    const int count = xcb_get_property_value_length(reply) / int(sizeof(xcb_atom_t));
    for (int i = 0; i < count; ++i) {
        auto *name = xcb_get_atom_name_reply(c, xcb_get_atom_name(c, atoms[i]), nullptr);
        if (name) {
            names << QByteArray(xcb_get_atom_name_name(name), xcb_get_atom_name_name_length(name));
            free(name);
        }
    }
    free(reply);
    return names.isEmpty() ? "none" : names.join(',');
}

static void spin(int milliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
}

int main(int argc, char **argv)
{
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    QGuiApplication app(argc, argv);

    auto *connection = static_cast<xcb_connection_t *>(
            QGuiApplication::platformNativeInterface()->nativeResourceForIntegration("connection"));
    if (!connection)
        return 2;

    QQmlApplicationEngine engine;
    engine.loadData(qmlSource);
    if (engine.rootObjects().isEmpty())
        return 3;

    auto *root = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    spin(300);

    QSet<QWindow *> seen;
    for (QWindow *window : QGuiApplication::allWindows())
        seen.insert(window);

    for (const char *name : { "menu", "tooltip", "dialog", "combobox" }) {
        QObject *popup = root->findChild<QObject *>(QLatin1StringView(name));
        if (popup && qstrcmp(name, "combobox") == 0)
            popup = popup->property("popup").value<QObject *>();
        if (!popup || !QMetaObject::invokeMethod(popup, "open")) {
            printf("%s: unavailable\n", name);
            continue;
        }
        spin(300);

        QByteArray result = "no-window";
        for (QWindow *window : QGuiApplication::allWindows()) {
            if (!seen.contains(window) && window->isVisible()) {
                result = windowTypes(connection, xcb_window_t(window->winId()));
                seen.insert(window);
            }
        }
        printf("%s: %s\n", name, result.constData());
        QMetaObject::invokeMethod(popup, "close");
        spin(100);
    }

    const QImage image = root->grabWindow();
    printf("render: %s\n", image.isNull() ? "null"
            : qPrintable(image.pixelColor(190, 190).name()));
    return 0;
}

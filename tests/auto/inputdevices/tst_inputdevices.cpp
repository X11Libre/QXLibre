// A device hierarchy change only removes the input devices the plugin created, not ones the app registered.

#include <QtCore/qpointer.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qpointingdevice.h>
#include <QtTest/qtest.h>

#include <qpa/qplatformnativeinterface.h>
#include <qpa/qwindowsysteminterface.h>

#include <xcb/xcb.h>
#include <xcb/xinput.h>

#include <algorithm>
#include <cstring>

using namespace Qt::StringLiterals;

class tst_InputDevices : public QObject
{
    Q_OBJECT
private slots:
    void hierarchyChangeKeepsForeignDevices();
};

// Adds a master pointer and keyboard pair, as plugging in a device does.
static void addMasterDevice(xcb_connection_t *connection, const QByteArray &name)
{
    const int nameBytes = (name.size() + 3) & ~3;
    QByteArray change(8 + nameBytes, '\0');
    auto *add = reinterpret_cast<xcb_input_add_master_t *>(change.data());
    add->type = XCB_INPUT_HIERARCHY_CHANGE_TYPE_ADD_MASTER;
    add->len = change.size() / 4;
    add->name_len = name.size();
    add->send_core = 1;
    add->enable = 1;
    std::memcpy(change.data() + 8, name.constData(), name.size());
    xcb_input_xi_change_hierarchy(connection, 1,
                                  reinterpret_cast<const xcb_input_hierarchy_change_t *>(change.constData()));
    xcb_flush(connection);
}

// Removes a master pair again, as unplugging does; its slave devices float.
static void removeMasterDevice(xcb_connection_t *connection, xcb_input_device_id_t masterPointer)
{
    xcb_input_remove_master_t remove = {};
    remove.type = XCB_INPUT_HIERARCHY_CHANGE_TYPE_REMOVE_MASTER;
    remove.len = sizeof(remove) / 4;
    remove.deviceid = masterPointer;
    remove.return_mode = XCB_INPUT_CHANGE_MODE_FLOAT;
    xcb_input_xi_change_hierarchy(connection, 1, reinterpret_cast<const xcb_input_hierarchy_change_t *>(&remove));
    xcb_flush(connection);
}

static QByteArray deviceNames()
{
    QStringList names;
    for (const QInputDevice *dev : QInputDevice::devices())
        names << dev->name();
    return names.join(u", "_s).toUtf8();
}

static const QInputDevice *deviceNamed(const QString &name)
{
    const auto devices = QInputDevice::devices();
    const auto it = std::find_if(devices.cbegin(), devices.cend(),
                                 [&name](const QInputDevice *dev) { return dev->name().startsWith(name); });
    return it == devices.cend() ? nullptr : *it;
}

void tst_InputDevices::hierarchyChangeKeepsForeignDevices()
{
    auto *connection = static_cast<xcb_connection_t *>(
            QGuiApplication::platformNativeInterface()->nativeResourceForIntegration("connection"));
    QVERIFY(connection);

    // Like sonic-win's virtual touch device for its own windows: no X device behind it.
    QPointer<QPointingDevice> foreign = new QPointingDevice(
            u"foreign touchscreen"_s, 0, QInputDevice::DeviceType::TouchScreen,
            QPointingDevice::PointerType::Finger, QInputDevice::Capability::Position, 10, 0);
    QWindowSystemInterface::registerInputDevice(foreign);

    const QByteArray name = "qxlibre test " + QByteArray::number(QCoreApplication::applicationPid());
    const QString pointerName = QString::fromLatin1(name) + u" pointer"_s;
    addMasterDevice(connection, name);
    // The plugin has handled the hierarchy event once it lists the new master.
    QTRY_VERIFY2(deviceNamed(pointerName), deviceNames().constData());

    QVERIFY2(foreign, "the plugin deleted an input device it didn't create");
    QVERIFY(QInputDevice::devices().contains(foreign.data()));

    // The removed masters and their slaves go together; the slaves are the masters' children.
    removeMasterDevice(connection, deviceNamed(pointerName)->systemId());
    QTRY_VERIFY(!deviceNamed(pointerName));
    QVERIFY(std::none_of(QInputDevice::devices().cbegin(), QInputDevice::devices().cend(),
                         [&name](const QInputDevice *dev) { return dev->name().startsWith(QString::fromLatin1(name)); }));
    QVERIFY2(foreign, "the plugin deleted an input device it didn't create");
    delete foreign.data();
}

int main(int argc, char **argv)
{
    if (!qEnvironmentVariableIsSet("DISPLAY")) {
        qInfo("tst_InputDevices: skipped, needs an X server (DISPLAY isn't set)");
        return 0;
    }

    // Set before QGuiApplication so that Qt loads this build's plugin.
    qputenv("QT_QPA_PLATFORM", "xlibre");
    qputenv("QT_PLUGIN_PATH", QXLIBRE_PLUGIN_DIR);

    QGuiApplication app(argc, argv);
    tst_InputDevices test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_inputdevices.moc"

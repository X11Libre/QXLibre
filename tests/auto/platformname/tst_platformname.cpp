#include <QtCore/qfile.h>
#include <QtCore/qdir.h>
#include <QtGui/qguiapplication.h>
#include <QtTest/qtest.h>

#include <qpa/qplatformnativeinterface.h>

using namespace Qt::StringLiterals;

class tst_PlatformName : public QObject
{
    Q_OBJECT
private slots:
    void loadedThisPlugin();
    void reportsXcb();
    void reportsXcbDuringThemeCreation();
    void nativeResourcesAvailable();
};

void tst_PlatformName::loadedThisPlugin()
{
    QFile maps(u"/proc/self/maps"_s);
    QVERIFY(maps.open(QIODevice::ReadOnly));
    QVERIFY(maps.readAll().contains("/libqxlibre.so"));
}

void tst_PlatformName::reportsXcb()
{
    QCOMPARE(QGuiApplication::platformName(), u"xcb"_s);
}

// Catches Qt changing the startup order that the name override relies on.
void tst_PlatformName::reportsXcbDuringThemeCreation()
{
    QCOMPARE(qEnvironmentVariable("QXLIBRE_TEST_THEME_PLATFORM"), u"xcb"_s);
}

void tst_PlatformName::nativeResourcesAvailable()
{
    auto *native = QGuiApplication::platformNativeInterface();
    QVERIFY(native);
    QVERIFY(native->nativeResourceForIntegration("connection"));
    QVERIFY(native->nativeResourceForIntegration("display"));
}

int main(int argc, char **argv)
{
    if (!qEnvironmentVariableIsSet("DISPLAY")) {
        qInfo("tst_PlatformName: skipped, needs an X server (DISPLAY isn't set)");
        return 0;
    }

    // Set before QGuiApplication so that Qt loads this build's plugin and the fixture theme.
    qputenv("QT_QPA_PLATFORM", "xlibre");
    qputenv("QT_QPA_PLATFORMTHEME", "qxlibretesttheme");
    const QByteArray pluginPath =
            QByteArray(QXLIBRE_PLUGIN_DIR) + QDir::listSeparator().toLatin1()
            + QXLIBRE_FIXTURE_PLUGIN_DIR;
    qputenv("QT_PLUGIN_PATH", pluginPath);

    QGuiApplication app(argc, argv);
    tst_PlatformName test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_platformname.moc"

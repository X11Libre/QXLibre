#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>
#include <QtGui/qguiapplication.h>
#include <qpa/qplatformthemeplugin.h>

class QXLibreTestThemePlugin : public QPlatformThemePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformThemeFactoryInterface_iid FILE "qxlibretesttheme.json")
public:
    QPlatformTheme *create(const QString &, const QStringList &) override
    {
        qputenv("QXLIBRE_TEST_THEME_PLATFORM", QGuiApplication::platformName().toUtf8());
        return nullptr; // Qt moves on to its next theme
    }
};

#include "qxlibretesttheme.moc"

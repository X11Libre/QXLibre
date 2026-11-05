#include <QtCore/qprocess.h>
#include <QtGui/qtguiglobal.h>
#include <QtTest/qtest.h>

using namespace Qt::StringLiterals;

static QByteArray runHelper(const QString &platform)
{
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(u"QT_QPA_PLATFORM"_s, platform);
    environment.insert(u"QT_PLUGIN_PATH"_s, QStringLiteral(QXLIBRE_PLUGIN_DIR));
    process.setProcessEnvironment(environment);
    process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    process.start(QStringLiteral(QXLIBRE_HELPER));
    if (!process.waitForFinished(60000) || process.exitCode() != 0)
        return "helper failed: " + QByteArray::number(process.exitCode());
    return process.readAllStandardOutput();
}

class tst_QuickPopupTypes : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void rendersThroughPlugin();
    void popupWindowTypes();
};

void tst_QuickPopupTypes::initTestCase()
{
    if (!qEnvironmentVariableIsSet("DISPLAY"))
        QSKIP("Needs an X server (DISPLAY isn't set)");
}

void tst_QuickPopupTypes::rendersThroughPlugin()
{
    QVERIFY2(runHelper(u"xlibre"_s).contains("render: #ff0000"), "Window didn't render its colour");
}

void tst_QuickPopupTypes::popupWindowTypes()
{
    const QByteArray output = runHelper(u"xlibre"_s);
    qInfo().noquote() << output;
    QVERIFY(!output.contains("unavailable") && !output.contains("no-window"));

#if QT_CONFIG(xcb)
    // Stock Qt's own plugin is the reference: this one must label popups the same way.
    QCOMPARE(output, runHelper(u"xcb"_s));
#else
    QVERIFY(output.contains("menu: _NET_WM_WINDOW_TYPE_POPUP_MENU"));
    QVERIFY(output.contains("combobox: _NET_WM_WINDOW_TYPE_POPUP_MENU"));
    QVERIFY(output.contains("tooltip: _NET_WM_WINDOW_TYPE_TOOLTIP"));
    QVERIFY(output.contains("dialog: _NET_WM_WINDOW_TYPE_DIALOG"));
#endif
}

QTEST_GUILESS_MAIN(tst_QuickPopupTypes)
#include "tst_quickpopuptypes.moc"

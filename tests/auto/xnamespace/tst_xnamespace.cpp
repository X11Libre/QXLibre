// Runs clients inside Xnamespaces on a private X server. Skipped on servers without the extension.

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qprocess.h>
#include <QtCore/qstandardpaths.h>
#include <QtCore/qtemporarydir.h>
#include <QtTest/qtest.h>

using namespace Qt::StringLiterals;

static const auto rootCookie = "56f8e62b78e58962de0ceefc05ad90b0"_L1;
static const auto plainCookie = "46f8e62b78e58962de0ceefc05ad90b0"_L1;
static const auto allowedCookie = "56f8e62b78e58962de0ceefc05ad90b1"_L1;
static const auto otherCookie = "46f8e62b78e58962de0ceefc05ad90b8"_L1;

class tst_XNamespace : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void clientRuns_data();
    void clientRuns();
    void clipboardWithinNamespace();
    void clipboardIsolatedBetweenNamespaces();
    void typing();

private:
    QString authFile(QLatin1StringView cookie) const;
    void configure(QProcess &process, QLatin1StringView cookie) const;
    QByteArray run(QLatin1StringView cookie, const QStringList &arguments) const;
    QByteArray typeInto(const QString &mode) const;

    QTemporaryDir m_dir;
    QProcess m_server;
    QString m_display;
};

QString tst_XNamespace::authFile(QLatin1StringView cookie) const
{
    return m_dir.filePath(cookie + ".auth"_L1);
}

void tst_XNamespace::configure(QProcess &process, QLatin1StringView cookie) const
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(u"DISPLAY"_s, m_display);
    environment.insert(u"XAUTHORITY"_s, authFile(cookie));
    environment.insert(u"QT_QPA_PLATFORM"_s, u"xlibre"_s);
    environment.insert(u"QT_PLUGIN_PATH"_s, QStringLiteral(QXLIBRE_PLUGIN_DIR));
    process.setProcessEnvironment(environment);
    process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
}

QByteArray tst_XNamespace::run(QLatin1StringView cookie, const QStringList &arguments) const
{
    QProcess process;
    configure(process, cookie);
    process.start(QStringLiteral(QXLIBRE_HELPER), arguments);
    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished();
        return "timed out";
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return "failed: " + QByteArray::number(process.exitCode());
    return process.readAllStandardOutput();
}

void tst_XNamespace::initTestCase()
{
    const QString xvfb = QStandardPaths::findExecutable(u"Xvfb"_s);
    const QString xauth = QStandardPaths::findExecutable(u"xauth"_s);
    if (xvfb.isEmpty() || xauth.isEmpty())
        QSKIP("Needs Xvfb and xauth");

    QProcess help;
    help.setProcessChannelMode(QProcess::MergedChannels);
    help.start(xvfb, { u"-help"_s });
    help.waitForFinished();
    if (!help.readAll().contains("-namespace"))
        QSKIP("This X server has no Xnamespace extension");

    QVERIFY(m_dir.isValid());
    QFile config(m_dir.filePath(u"ns.conf"_s));
    QVERIFY(config.open(QIODevice::WriteOnly));
    // auth lines before the first namespace statement belong to the root namespace
    config.write(("auth MIT-MAGIC-COOKIE-1 "_L1 + rootCookie
                  + "\n\nnamespace plain root\n  auth MIT-MAGIC-COOKIE-1 "_L1 + plainCookie
                  + "\n\nnamespace other root\n  auth MIT-MAGIC-COOKIE-1 "_L1 + otherCookie
                  + "\n\nnamespace allowed root\n  auth MIT-MAGIC-COOKIE-1 "_L1 + allowedCookie
                  + "\n  allow mouse-motion\n  allow shape\n  allow transparency\n"
                    "  allow xinput\n  allow xkeyboard\n"_L1).toLatin1());
    config.close();

    // The server reads its cookie at startup, whichever display name the entry carries.
    const QString serverAuth = m_dir.filePath(u"server.auth"_s);
    QVERIFY(QFile(serverAuth).open(QIODevice::WriteOnly));
    QCOMPARE(QProcess::execute(xauth, { u"-f"_s, serverAuth, u"add"_s, u":0"_s,
                                        u"MIT-MAGIC-COOKIE-1"_s, QString(rootCookie) }), 0);

    // Not -displayfd: it can pick the number of a server that holds only an abstract socket, such
    // as a login greeter, and then clients reach that server instead.
    int display = 100;
    while (display < 200 && (QFile::exists(u"/tmp/.X11-unix/X%1"_s.arg(display))
                             || QFile::exists(u"/tmp/.X%1-lock"_s.arg(display)))) {
        ++display;
    }
    QVERIFY(display < 200);
    m_display = u":%1"_s.arg(display);

    // Without -noreset the server restarts whenever its last client leaves, which breaks the next
    // one to connect. Its namespace log goes to stdout, so that can't be a pipe that fills up.
    m_server.setStandardOutputFile(m_dir.filePath(u"server.out"_s));
    m_server.setStandardErrorFile(QProcess::nullDevice());
    m_server.start(xvfb, { m_display, u"-noreset"_s, u"-screen"_s, u"0"_s, u"1280x1024x24"_s,
                           u"-auth"_s, serverAuth, u"-namespace"_s, config.fileName() });
    QVERIFY(m_server.waitForStarted());
    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(u"/tmp/.X11-unix/X%1"_s.arg(display)), 10000);

    for (auto cookie : { rootCookie, plainCookie, allowedCookie, otherCookie }) {
        QVERIFY(QFile(authFile(cookie)).open(QIODevice::WriteOnly));
        const int result = QProcess::execute(xauth, { u"-f"_s, authFile(cookie), u"add"_s, m_display,
                                                      u"MIT-MAGIC-COOKIE-1"_s, QString(cookie) });
        QCOMPARE(result, 0);
    }
}

void tst_XNamespace::cleanupTestCase()
{
    m_server.kill();
    m_server.waitForFinished();
}

void tst_XNamespace::clientRuns_data()
{
    QTest::addColumn<QString>("cookie");
    QTest::addColumn<QString>("mode");
    QTest::addColumn<QByteArray>("expected");
    const std::pair<const char *, QLatin1StringView> namespaces[] = {
        { "plain", plainCookie }, { "allowed", allowedCookie }
    };
    for (const auto &[name, cookie] : namespaces) {
        QTest::addRow("%s, window", name) << QString(cookie) << u"window"_s << QByteArray("window: painted");
        QTest::addRow("%s, translucent", name) << QString(cookie) << u"translucent"_s
                                                << QByteArray("translucent: painted");
        QTest::addRow("%s, Qt Quick", name) << QString(cookie) << u"quick"_s << QByteArray("quick: #ff0000");
        QTest::addRow("%s, screen", name) << QString(cookie) << u"screen"_s
                                           << QByteArray("screen: 1280x1024");
    }
}

void tst_XNamespace::clientRuns()
{
    QFETCH(QString, cookie);
    QFETCH(QString, mode);
    QFETCH(QByteArray, expected);
    const QByteArray output = run(QLatin1StringView(cookie.toLatin1()), { mode });
    QVERIFY2(output.contains(expected), output.constData());
}

void tst_XNamespace::clipboardWithinNamespace()
{
    QProcess owner;
    configure(owner, allowedCookie);
    owner.start(QStringLiteral(QXLIBRE_HELPER), { u"own"_s, u"shared-text"_s });
    QVERIFY(owner.waitForReadyRead(15000));

    const QByteArray output = run(allowedCookie, { u"read"_s });
    owner.kill();
    owner.waitForFinished();
    QVERIFY2(output.contains("clipboard: [shared-text]"), output.constData());
}

void tst_XNamespace::clipboardIsolatedBetweenNamespaces()
{
    QProcess owner;
    configure(owner, allowedCookie);
    owner.start(QStringLiteral(QXLIBRE_HELPER), { u"own"_s, u"private-text"_s });
    QVERIFY(owner.waitForReadyRead(15000));

    const QByteArray output = run(otherCookie, { u"read"_s });
    owner.kill();
    owner.waitForFinished();
    QVERIFY2(output.contains("clipboard:"), output.constData());
    QVERIFY2(!output.contains("private-text"), output.constData());
}

QByteArray tst_XNamespace::typeInto(const QString &mode) const
{
    QProcess typist;
    configure(typist, allowedCookie);
    typist.start(QStringLiteral(QXLIBRE_HELPER), { mode });
    if (!typist.waitForReadyRead(15000))
        return "no window";
    const QString window = QString::fromLatin1(typist.readLine().trimmed().split(' ').value(1));

    QProcess key;
    configure(key, rootCookie);
    key.start(QStringLiteral(QXLIBRE_HELPER), { u"sendkey"_s, window, u"38"_s });
    if (!key.waitForFinished(10000) || key.exitCode() != 0)
        return "no key sent";

    if (!typist.waitForFinished(15000))
        return "timed out";
    return typist.readAllStandardOutput();
}

void tst_XNamespace::typing()
{
    if (!typeInto(u"rawtype"_s).contains("rawtyped: yes"))
        QSKIP("This server doesn't deliver keys to any client in a namespace");

    const QByteArray output = typeInto(u"type"_s);
    QVERIFY2(output.contains("typed: [a]"), output.constData());
}

QTEST_GUILESS_MAIN(tst_XNamespace)
#include "tst_xnamespace.moc"

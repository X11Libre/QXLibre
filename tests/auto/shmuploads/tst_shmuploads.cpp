// Skipping round trips with MIT-SHM completion events must never change what reaches the window.

#include <QtCore/qfile.h>
#include <QtCore/qprocess.h>
#include <QtCore/qstandardpaths.h>
#include <QtTest/qtest.h>

using namespace Qt::StringLiterals;

class tst_ShmUploads : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void samePixels_data();
    void samePixels();
    void samePixelsWithoutShm_data() { samePixels_data(); }
    void samePixelsWithoutShm();
    void noRoundTripOnceFinished_data();
    void noRoundTripOnceFinished();
    void offSwitchUsesRoundTrips();
    void roundTripWithoutCompletion();
    void earlierCompletionDoesntFinishLaterUpload();

private:
    bool startServer(QProcess &server, QString *display, const QStringList &extraArguments);
    QByteArray run(const QByteArray &scenario, bool offSwitch = false) const;
    QByteArray run(const QString &display, const QByteArray &scenario, bool offSwitch) const;

    QProcess m_server;
    QProcess m_serverWithoutShm;
    QString m_display;
    QString m_displayWithoutShm;
};

static QByteArray field(const QByteArray &output, const QByteArray &name)
{
    const auto lines = output.split('\n');
    for (const QByteArray &line : lines) {
        if (line.startsWith(name + ": "))
            return line.mid(name.size() + 2);
    }
    return {};
}

QByteArray tst_ShmUploads::run(const QByteArray &scenario, bool offSwitch) const
{
    return run(m_display, scenario, offSwitch);
}

QByteArray tst_ShmUploads::run(const QString &display, const QByteArray &scenario, bool offSwitch) const
{
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(u"DISPLAY"_s, display);
    environment.insert(u"QT_QPA_PLATFORM"_s, u"xlibre"_s);
    environment.insert(u"QT_PLUGIN_PATH"_s, QStringLiteral(QXLIBRE_PLUGIN_DIR));
    environment.insert(u"QT_XLIBRE_DEBUG"_s, u"1"_s);
    environment.remove(u"QT_XLIBRE_NO_SHM_COMPLETION"_s);
    if (offSwitch)
        environment.insert(u"QT_XLIBRE_NO_SHM_COMPLETION"_s, u"1"_s);
    process.setProcessEnvironment(environment);
    process.setStandardErrorFile(QProcess::nullDevice());
    process.start(QStringLiteral(QXLIBRE_HELPER), { QString::fromLatin1(scenario) });
    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished();
        return "timed out";
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return "failed: " + QByteArray::number(process.exitCode());
    return process.readAllStandardOutput();
}

bool tst_ShmUploads::startServer(QProcess &server, QString *display, const QStringList &extraArguments)
{
    int number = 100;
    while (number < 200 && (QFile::exists(u"/tmp/.X11-unix/X%1"_s.arg(number))
                            || QFile::exists(u"/tmp/.X%1-lock"_s.arg(number)))) {
        ++number;
    }
    if (number == 200)
        return false;
    *display = u":%1"_s.arg(number);

    server.setStandardOutputFile(QProcess::nullDevice());
    server.setStandardErrorFile(QProcess::nullDevice());
    server.start(QStandardPaths::findExecutable(u"Xvfb"_s),
                 QStringList{ *display, u"-noreset"_s, u"-screen"_s, u"0"_s, u"1280x1024x24"_s }
                         + extraArguments);
    if (!server.waitForStarted())
        return false;
    for (int i = 0; i < 100 && !QFile::exists(u"/tmp/.X11-unix/X%1"_s.arg(number)); ++i)
        QTest::qWait(100);
    return QFile::exists(u"/tmp/.X11-unix/X%1"_s.arg(number));
}

void tst_ShmUploads::initTestCase()
{
    if (QStandardPaths::findExecutable(u"Xvfb"_s).isEmpty())
        QSKIP("Needs Xvfb");
    QVERIFY(startServer(m_server, &m_display, {}));
    QVERIFY(startServer(m_serverWithoutShm, &m_displayWithoutShm,
                        { u"-extension"_s, u"MIT-SHM"_s }));
}

void tst_ShmUploads::cleanupTestCase()
{
    for (QProcess *server : { &m_server, &m_serverWithoutShm }) {
        server->kill();
        server->waitForFinished();
    }
}

void tst_ShmUploads::samePixels_data()
{
    QTest::addColumn<QByteArray>("scenario");
    for (const char *scenario : { "idle-frames", "rapid", "partial", "scroll", "resize", "hide", "translucent" })
        QTest::newRow(scenario) << QByteArray(scenario);
}

void tst_ShmUploads::samePixels()
{
    QFETCH(QByteArray, scenario);
    const QByteArray withRoundTrips = run(scenario, true);
    const QByteArray withEvents = run(scenario);

    QVERIFY2(field(withRoundTrips, "hash").size() == 40, withRoundTrips.constData());
    QCOMPARE(field(withEvents, "hash"), field(withRoundTrips, "hash"));
}

void tst_ShmUploads::samePixelsWithoutShm()
{
    QFETCH(QByteArray, scenario);
    const QByteArray withSwitch = run(m_displayWithoutShm, scenario, true);
    const QByteArray withoutSwitch = run(m_displayWithoutShm, scenario, false);

    QVERIFY2(field(withSwitch, "hash").size() == 40, withSwitch.constData());
    QCOMPARE(field(withoutSwitch, "hash"), field(withSwitch, "hash"));
}

void tst_ShmUploads::noRoundTripOnceFinished_data()
{
    QTest::addColumn<QByteArray>("scenario");
    QTest::newRow("opaque") << QByteArray("idle-frames");
    QTest::newRow("alpha") << QByteArray("translucent");
}

void tst_ShmUploads::noRoundTripOnceFinished()
{
    QFETCH(QByteArray, scenario);
    const QByteArray output = run(scenario);
    QVERIFY2(field(output, "syncs-per-frame").toDouble() < 0.25, output.constData());
}

void tst_ShmUploads::offSwitchUsesRoundTrips()
{
    const QByteArray output = run("idle-frames", true);
    QVERIFY2(field(output, "syncs-per-frame").toDouble() > 0.75, output.constData());
}

void tst_ShmUploads::roundTripWithoutCompletion()
{
    // Without a completion, painting over memory the server may be reading has to wait for it.
    const QByteArray output = run("rapid");
    QVERIFY2(field(output, "syncs-per-frame").toDouble() > 0.75, output.constData());
}

void tst_ShmUploads::earlierCompletionDoesntFinishLaterUpload()
{
    const QByteArray output = run("two-windows");
    const double syncs = field(output, "syncs-per-frame").toDouble();
    QVERIFY2(syncs > 0.75 && syncs < 1.5, output.constData());
}

QTEST_GUILESS_MAIN(tst_ShmUploads)
#include "tst_shmuploads.moc"

// Update requests follow the vblank through the Present extension, and always get delivered.

#include <QtCore/qfile.h>
#include <QtCore/qprocess.h>
#include <QtCore/qstandardpaths.h>
#include <QtTest/qtest.h>

using namespace Qt::StringLiterals;

class tst_Present : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();
    void updatesFollowTheVblank();
    void offSwitchUsesTheTimer();
    void slowFramesLeaveRoomForTimers();
    void hiddenWindowGetsItsUpdate();
    void updateSurvivesError();
    void updateSurvivesRecreation();
    void lostWindowKeepsVblankUpdates();
    void otherNotificationsDontDeliver();
    void otherSurfacesUseTheTimer();
    void softwareQuickFollowsTheVblank();
    void softwareQuickAfterError();
    void softwareQuickAnimatesWithoutFrames();

private:
    QByteArray run(const QByteArray &scenario, bool offSwitch = false) const;

    QProcess m_server;
    QString m_display;
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

QByteArray tst_Present::run(const QByteArray &scenario, bool offSwitch) const
{
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(u"DISPLAY"_s, m_display);
    environment.insert(u"QT_QPA_PLATFORM"_s, u"xlibre"_s);
    environment.insert(u"QT_PLUGIN_PATH"_s, QStringLiteral(QXLIBRE_PLUGIN_DIR));
    environment.remove(u"QT_XLIBRE_NO_VBLANK_UPDATES"_s);
    if (offSwitch)
        environment.insert(u"QT_XLIBRE_NO_VBLANK_UPDATES"_s, u"1"_s);
    process.setProcessEnvironment(environment);
    process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
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

void tst_Present::initTestCase()
{
    const QString xvfb = QStandardPaths::findExecutable(u"Xvfb"_s);
    if (xvfb.isEmpty())
        QSKIP("Needs Xvfb");

    int number = 100;
    while (number < 200 && (QFile::exists(u"/tmp/.X11-unix/X%1"_s.arg(number))
                            || QFile::exists(u"/tmp/.X%1-lock"_s.arg(number)))) {
        ++number;
    }
    QVERIFY(number < 200);
    m_display = u":%1"_s.arg(number);

    m_server.setStandardOutputFile(QProcess::nullDevice());
    m_server.setStandardErrorFile(QProcess::nullDevice());
    m_server.start(xvfb, { m_display, u"-noreset"_s, u"-screen"_s, u"0"_s, u"1280x1024x24"_s });
    QVERIFY(m_server.waitForStarted());
    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(u"/tmp/.X11-unix/X%1"_s.arg(number)), 10000);
}

void tst_Present::cleanupTestCase()
{
    m_server.kill();
    m_server.waitForFinished();
}

void tst_Present::updatesFollowTheVblank()
{
    // Without a display, the server's fake vblank runs at 60 Hz.
    const QByteArray output = run("animation");
    const int rate = field(output, "updates-per-second").toInt();
    QVERIFY2(rate >= 50 && rate <= 70, output.constData());
}

void tst_Present::offSwitchUsesTheTimer()
{
    // Qt's timer waits at most 5 ms, so it runs well above the vblank.
    const QByteArray output = run("animation", true);
    const int rate = field(output, "updates-per-second").toInt();
    QVERIFY2(rate > 100, output.constData());
}

void tst_Present::slowFramesLeaveRoomForTimers()
{
    const QByteArray output = run("slow-frames");
    QVERIFY2(field(output, "timer-fires").toInt() >= 5, output.constData());
}

void tst_Present::hiddenWindowGetsItsUpdate()
{
    const QByteArray output = run("update-hidden");
    QCOMPARE(field(output, "update-delivered"), "yes");
}

void tst_Present::updateSurvivesError()
{
    const QByteArray output = run("update-error");
    QCOMPARE(field(output, "update-delivered"), "yes");
    // After the error, the timer is what delivers.
    QVERIFY2(field(output, "updates-per-second").toInt() > 100, output.constData());
}

void tst_Present::updateSurvivesRecreation()
{
    const QByteArray output = run("update-recreated");
    QCOMPARE(field(output, "update-delivered"), "yes");
}

void tst_Present::lostWindowKeepsVblankUpdates()
{
    const QByteArray output = run("update-window-lost");
    QCOMPARE(field(output, "update-delivered"), "yes");
    const int rate = field(output, "updates-per-second").toInt();
    QVERIFY2(rate >= 50 && rate <= 70, output.constData());
}

void tst_Present::otherNotificationsDontDeliver()
{
    const QByteArray output = run("other-notifications");
    const int rate = field(output, "updates-per-second").toInt();
    QVERIFY2(rate >= 50 && rate <= 70, output.constData());
}

void tst_Present::otherSurfacesUseTheTimer()
{
    const QByteArray output = run("opengl-surface");
    if (field(output, "no-opengl") == "yes")
        QSKIP("Needs OpenGL");
    QVERIFY2(field(output, "updates-per-second").toInt() > 100, output.constData());
}

void tst_Present::softwareQuickFollowsTheVblank()
{
    if (run("has-quick").startsWith("failed"))
        QSKIP("Needs Qt Quick");
    const QByteArray output = run("quick-software");
    const int rate = field(output, "frames-per-second").toInt();
    QVERIFY2(rate >= 50 && rate <= 70, output.constData());
    // A frame that misses a vblank shows up here, at about 8 ms on Xvfb.
    QVERIFY2(field(output, "interval-deviation-tenths").toInt() < 30, output.constData());
}

void tst_Present::softwareQuickAfterError()
{
    if (run("has-quick").startsWith("failed"))
        QSKIP("Needs Qt Quick");
    const QByteArray output = run("quick-software-error");
    QVERIFY2(field(output, "frames-per-second").toInt() > 30, output.constData());
}

void tst_Present::softwareQuickAnimatesWithoutFrames()
{
    if (run("has-quick").startsWith("failed"))
        QSKIP("Needs Qt Quick");
    QCOMPARE(field(run("quick-software-hidden"), "animation-finished"), "yes");
}

QTEST_GUILESS_MAIN(tst_Present)
#include "tst_present.moc"

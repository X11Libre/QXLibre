// Diagnostic only: a session-managed client that logs what the session manager asks of it.
#include <QtCore/qfile.h>
#include <QtCore/qtimer.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qsessionmanager.h>
#include <QtGui/qwindow.h>
#include <cstdio>
#include <cstdlib>

static void note(const QByteArray &text)
{
    QFile file(QString::fromLocal8Bit(qgetenv("SMTEST_LOG")));
    if (file.open(QIODevice::Append))
        file.write(text + '\n');
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    note("start platform=" + QGuiApplication::platformName().toUtf8() + " restored="
         + (app.isSessionRestored() ? "yes" : "no") + " id="
         + (app.sessionId().isEmpty() ? "empty" : "set"));
    // The id is compared across a restart, so it goes in a separate line that the summary can check.
    note("id " + app.sessionId().toUtf8());

    int commits = 0, saves = 0;
    QObject::connect(&app, &QGuiApplication::commitDataRequest, &app,
                     [&](QSessionManager &manager) {
                         note("commitData " + QByteArray::number(++commits) + " interact="
                              + (manager.allowsInteraction() ? "yes" : "no"));
                         // The second request is the one with a shutdown.
                         if (commits == 2 && qEnvironmentVariableIsSet("SMTEST_CANCEL")) {
                             note("cancelling the shutdown");
                             manager.cancel();
                         }
                         manager.release();
                     }, Qt::DirectConnection);
    QObject::connect(&app, &QGuiApplication::saveStateRequest, &app,
                     [&](QSessionManager &manager) {
                         note("saveState " + QByteArray::number(++saves) + " key="
                              + (manager.sessionKey().isEmpty() ? "empty" : "set") + " restart="
                              + (manager.restartCommand().isEmpty() ? "empty" : "set"));
                     }, Qt::DirectConnection);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [] { note("aboutToQuit"); });

    QWindow window;
    window.resize(200, 200);
    window.show();
    const int result = app.exec();
    note("exit " + QByteArray::number(result));
    return result;
}

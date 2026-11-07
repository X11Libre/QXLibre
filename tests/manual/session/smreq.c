/* Diagnostic only: a session client that asks the manager for a global save, as a logout does.
 * Usage: smreq <0|1 shutdown>, with SESSION_MANAGER set. It answers its own save straight away. */
#include <X11/ICE/ICElib.h>
#include <X11/SM/SMlib.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int done;

static void save(SmcConn c, SmPointer d, int t, Bool s, int i, Bool f)
{
    (void)d; (void)t; (void)s; (void)i; (void)f;
    SmcSaveYourselfDone(c, True);
}
static void die(SmcConn c, SmPointer d) { (void)c; (void)d; done = 1; }
static void complete(SmcConn c, SmPointer d) { (void)c; (void)d; done = 1; }
static void cancelled(SmcConn c, SmPointer d) { (void)c; (void)d; done = 1; }

int main(int argc, char **argv)
{
    const Bool shutdown = argc > 1 && argv[1][0] == '1';
    SmcCallbacks callbacks = {
        { save, NULL }, { die, NULL }, { complete, NULL }, { cancelled, NULL }
    };
    char error[256] = "";
    char *id = NULL;
    SmcConn conn = SmcOpenConnection(NULL, NULL, SmProtoMajor, SmProtoMinor,
                                     SmcSaveYourselfProcMask | SmcDieProcMask
                                             | SmcSaveCompleteProcMask | SmcShutdownCancelledProcMask,
                                     &callbacks, NULL, &id, sizeof error, error);
    if (!conn) {
        fprintf(stderr, "smreq: no connection: %s\n", error);
        return 1;
    }
    SmcRequestSaveYourself(conn, SmSaveBoth, shutdown, SmInteractStyleAny, False, True);
    IceConn ice = SmcGetIceConnection(conn);
    IceFlush(ice);

    const time_t end = time(NULL) + 6;
    while (!done && time(NULL) < end) {
        struct pollfd fd = { IceConnectionNumber(ice), POLLIN, 0 };
        if (poll(&fd, 1, 200) > 0 && IceProcessMessages(ice, NULL, NULL) != IceProcessMessagesSuccess)
            break;
    }
    if (!shutdown)
        SmcCloseConnection(conn, 0, NULL);
    printf("smreq: %s\n", done ? "finished" : "timed out or connection closed");
    return 0;
}

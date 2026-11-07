/* Diagnostic only: the smallest session manager that drives one client through a global save and
 * then a global save with shutdown, as a logout does. Usage: smsrv <address file> <cancel 0|1> */
#include <X11/ICE/ICElib.h>
#include <X11/ICE/ICEutil.h>
#include <X11/SM/SMlib.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static SmsConn client;
static IceConn clientIce;
static int stage; /* 0 waiting, 1 first save running, 2 second save running, 3 finished */
static time_t nextSave;
static int cancelled_;
static time_t dieAt;

static Status registerClient(SmsConn c, SmPointer d, char *previous)
{
    (void)d;
    client = c;
    char *id = SmsGenerateClientID(c);
    printf("smsrv: client registered, previous id %s\n", previous ? "given" : "none");
    free(previous);
    SmsRegisterClientReply(c, id);
    /* The protocol has a new client save its state once. */
    SmsSaveYourself(c, SmSaveLocal, False, SmInteractStyleNone, False);
    return 1;
}
static void interactRequest(SmsConn c, SmPointer d, int type)
{
    (void)d; (void)type;
    printf("smsrv: client asks to interact\n");
    SmsInteract(c);
}
static void interactDone(SmsConn c, SmPointer d, Bool cancel)
{
    (void)d;
    printf("smsrv: interaction done, cancel %d\n", cancel);
    if (cancel) {
        printf("smsrv: sending shutdown cancelled\n");
        SmsShutdownCancelled(c);
        cancelled_ = 1;
        dieAt = time(NULL) + 3;
    }
}
static void saveRequest(SmsConn c, SmPointer d, int t, Bool s, int i, Bool f, Bool g)
{
    (void)c; (void)d; (void)t; (void)s; (void)i; (void)f; (void)g;
}
static void phase2(SmsConn c, SmPointer d) { (void)d; SmsSaveYourselfPhase2(c); }
static void saveDone(SmsConn c, SmPointer d, Bool success)
{
    (void)d;
    printf("smsrv: save done, success %d, stage %d\n", success, stage);
    if (cancelled_)
        return;
    if (stage == 2) {
        printf("smsrv: sending die\n");
        SmsDie(c);
        stage = 3;
        return;
    }
    SmsSaveComplete(c);
    nextSave = time(NULL) + 1;
}
static void closed(SmsConn c, SmPointer d, int n, char **reasons)
{
    (void)d;
    printf("smsrv: client closed the connection\n");
    SmFreeReasons(n, reasons);
    SmsCleanUp(c);
    client = NULL;
    clientIce = NULL;
    stage = 3;
}
static void setProperties(SmsConn c, SmPointer d, int n, SmProp **props)
{
    (void)c; (void)d;
    for (int i = 0; i < n; ++i) {
        printf("smsrv: property %s\n", props[i]->name);
        SmFreeProperty(props[i]);
    }
    free(props);
}
static void deleteProperties(SmsConn c, SmPointer d, int n, char **names)
{
    (void)c; (void)d; (void)n; (void)names;
}
static void getProperties(SmsConn c, SmPointer d) { (void)d; SmsReturnProperties(c, 0, NULL); }

static Status newClient(SmsConn c, SmPointer d, unsigned long *mask, SmsCallbacks *cb, char **failure)
{
    (void)c; (void)d; (void)failure;
    cb->register_client.callback = registerClient;
    cb->interact_request.callback = interactRequest;
    cb->interact_done.callback = interactDone;
    cb->save_yourself_request.callback = saveRequest;
    cb->save_yourself_phase2_request.callback = phase2;
    cb->save_yourself_done.callback = saveDone;
    cb->close_connection.callback = closed;
    cb->set_properties.callback = setProperties;
    cb->delete_properties.callback = deleteProperties;
    cb->get_properties.callback = getProperties;
    *mask = SmsRegisterClientProcMask | SmsInteractRequestProcMask | SmsInteractDoneProcMask
            | SmsSaveYourselfRequestProcMask | SmsSaveYourselfP2RequestProcMask
            | SmsSaveYourselfDoneProcMask | SmsCloseConnectionProcMask | SmsSetPropertiesProcMask
            | SmsDeletePropertiesProcMask | SmsGetPropertiesProcMask;
    return 1;
}
static Bool allowHost(char *host) { (void)host; return True; }

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    char error[256] = "";
    int count = 0;
    IceListenObj *listeners;
    if (!SmsInitialize("smsrv", "1", newClient, NULL, allowHost, sizeof error, error)
        || !IceListenForConnections(&count, &listeners, sizeof error, error)) {
        fprintf(stderr, "smsrv: %s\n", error);
        return 1;
    }
    for (int i = 0; i < count; ++i)
        IceSetHostBasedAuthProc(listeners[i], allowHost);
    FILE *address = fopen(argv[1], "w");
    fprintf(address, "%s\n", IceComposeNetworkIdList(count, listeners));
    fclose(address);

    const time_t end = time(NULL) + 25;
    while (time(NULL) < end && stage < 3) {
        struct pollfd fds[32];
        int n = 0;
        for (int i = 0; i < count && n < 31; ++i)
            fds[n++] = (struct pollfd){ IceGetListenConnectionNumber(listeners[i]), POLLIN, 0 };
        const int listening = n;
        if (clientIce)
            fds[n++] = (struct pollfd){ IceConnectionNumber(clientIce), POLLIN, 0 };
        poll(fds, n, 200);
        for (int i = 0; i < listening; ++i) {
            if (fds[i].revents & POLLIN) {
                IceAcceptStatus status;
                IceConn ice = IceAcceptConnection(listeners[i], &status);
                while (ice && IceConnectionStatus(ice) == IceConnectPending)
                    IceProcessMessages(ice, NULL, NULL);
                if (ice && IceConnectionStatus(ice) == IceConnectAccepted)
                    clientIce = ice;
            }
        }
        if (clientIce && n > listening && (fds[listening].revents & (POLLIN | POLLHUP))
            && IceProcessMessages(clientIce, NULL, NULL) != IceProcessMessagesSuccess) {
            printf("smsrv: connection ended\n");
            clientIce = NULL;
            client = NULL;
            stage = 3;
        }
        if (client && dieAt && time(NULL) >= dieAt) {
            printf("smsrv: client still connected 3 s after the cancel, sending die\n");
            dieAt = 0;
            SmsDie(client);
            IceFlush(clientIce);
            stage = 3;
        }
        if (client && nextSave && time(NULL) >= nextSave) {
            nextSave = 0;
            ++stage;
            printf("smsrv: sending global save, shutdown %d\n", stage == 2);
            SmsSaveYourself(client, SmSaveBoth, stage == 2, SmInteractStyleAny, False);
            IceFlush(clientIce);
        }
    }
    /* Give the client a moment to act on die. */
    struct timespec pause = { 1, 0 };
    nanosleep(&pause, NULL);
    printf("smsrv: finished, stage %d\n", stage);
    return stage == 3 ? 0 : 1;
}

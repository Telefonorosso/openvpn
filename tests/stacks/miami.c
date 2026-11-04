/* Isolated MiamiDX interface lifecycle and real UDP/SANA-II integration. */
#define AOVPN_GUEST_LOG "RunProg:miami-guest.log"
#include "../emulator/dos-entry.h"
#include <dos/dostags.h>
#include <exec/libraries.h>
#include <rexx/rxslib.h>
#include <rexx/storage.h>
#include <proto/rexxsyslib.h>
struct Library *SocketBase;
struct RxsLib *RexxSysBase;
#include "udp-probe.h"

static BOOL assign_dir(const char *name, const char *path)
{
    BPTR lock = Lock((STRPTR)path, ACCESS_READ);
    if (!lock) return FALSE;
    if (!AssignLock((STRPTR)name, lock)) { UnLock(lock); return FALSE; }
    return TRUE;
}

static LONG command(const char *text)
{
    struct MsgPort *reply = CreateMsgPort(), *host;
    struct RexxMsg *message;
    LONG code;
    ULONG i, length = 0;
    if (!reply) return 20;
    message = CreateRexxMsg(reply, (STRPTR)"rexx", (STRPTR)"MIAMI.1");
    if (!message) { DeleteMsgPort(reply); return 20; }
    while (text[length]) ++length;
    message->rm_Args[0] = CreateArgstring((STRPTR)text, length);
    if (!message->rm_Args[0]) { DeleteRexxMsg(message); DeleteMsgPort(reply); return 20; }
    message->rm_Action = RXCOMM | RXFF_RESULT;
    Forbid();
    host = FindPort((STRPTR)"MIAMI.1");
    if (host) PutMsg(host, (struct Message *)message);
    Permit();
    if (!host) { code = 20; goto done; }
    for (i = 0; i < 500; ++i) {
        if (GetMsg(reply)) break;
        Delay(1);
    }
    if (i == 500) {
        puts("FAIL: Miami command timeout; retaining live reply target until VM deadline");
        Flush(Output());
        for (;;) Delay(50); /* The host still owns this message. Never free it early. */
    }
    code = message->rm_Result1;
    if (!code && message->rm_Result2) DeleteArgstring((STRPTR)message->rm_Result2);
done:
    DeleteArgstring(message->rm_Args[0]);
    DeleteRexxMsg(message);
    DeleteMsgPort(reply);
    printf("Miami command %s returned %ld\n", text, code);
    return code;
}

static BOOL wait_online(LONG expected)
{
    ULONG i;
    LONG state;
    for (i = 0; i < 100; ++i) {
        state = command("ISONLINE ptp0");
        if (state == expected) return TRUE;
        if (state != 0 && state != 1) return FALSE;
        Delay(1);
    }
    return FALSE;
}

int aovpn_probe_main(void)
{
    struct TagItem launch[] = {{SYS_Asynch, TRUE}, {SYS_Input, 0}, {SYS_Output, 0}, {TAG_DONE, 0}};
    ULONG i, cycle;
    LONG rc, result = 20;
    BOOL started = FALSE;
    if (!assign_dir("ENV", "RunProg:Env") || !assign_dir("ENVARC", "RunProg:Env") ||
        !assign_dir("DEVS", "RunProg:Devs") || !assign_dir("S", "RunProg:Scripts") ||
        !assign_dir("MIAMI", "RunProg:") || !assign_dir("LIBS", "RunProg:Runtime/Libs") ||
        !assign_dir("FONTS", "RunProg:Runtime/Fonts")) {
        puts("FAIL: Miami isolated runtime assigns"); goto done;
    }
    if (!assign_dir("MUI", "RunProg:Runtime/System/MUI"))
        assign_dir("MUI", "RunProg:Runtime");
    assign_dir("CLASSES", "RunProg:Runtime/Classes");
    RexxSysBase = (struct RxsLib *)OpenLibrary((STRPTR)"rexxsyslib.library", 0);
    if (!RexxSysBase) { puts("FAIL: rexxsyslib.library"); goto done; }
    launch[1].ti_Data = Open((STRPTR)"NIL:", MODE_OLDFILE);
    launch[2].ti_Data = Open((STRPTR)"RunProg:miami-child.log", MODE_NEWFILE);
    if (!launch[1].ti_Data || !launch[2].ti_Data) {
        if (launch[1].ti_Data) Close(launch[1].ti_Data);
        if (launch[2].ti_Data) Close(launch[2].ti_Data);
        puts("FAIL: Miami child handles"); goto done;
    }
    rc = SystemTagList((STRPTR)"RunProg:MiamiDx NOGUI DONTCONNECT SETTINGS=RunProg:openvpn.miami", launch);
    if (rc == -1) {
        Close(launch[1].ti_Data); Close(launch[2].ti_Data);
        puts("FAIL: Miami process creation"); goto done;
    }
    started = TRUE;
    for (i = 0; i < 500 && !FindPort((STRPTR)"MIAMI.1"); ++i) Delay(1);
    if (!FindPort((STRPTR)"MIAMI.1")) { puts("FAIL: Miami startup timeout"); goto done; }
    for (cycle = 0; cycle < 2; ++cycle) {
        rc = command("ONLINE ptp0");
        if ((rc != 0 && rc != 5) || !wait_online(1)) { puts("FAIL: Miami ONLINE"); goto done; }
        SocketBase = OpenLibrary((STRPTR)"bsdsocket.library", 4);
        if (!SocketBase) { puts("FAIL: Miami bsdsocket.library"); goto done; }
        printf("Stack: %s\n", SocketBase->lib_IdString);
        rc = SystemTagList((STRPTR)"RunProg:MiamiIfConfig ptp0 inet 10.8.0.15 10.8.0.1", NULL);
        if (rc) { printf("FAIL: MiamiIfConfig returned %ld\n", rc); goto done; }
        if (udp_roundtrip()) goto done;
        CloseLibrary(SocketBase); SocketBase = NULL;
        rc = command("OFFLINE ptp0");
        if ((rc != 0 && rc != 5) || !wait_online(0)) { puts("FAIL: Miami OFFLINE"); goto done; }
    }
    result = 0;
done:
    if (SocketBase) { CloseLibrary(SocketBase); SocketBase = NULL; }
    if (started && FindPort((STRPTR)"MIAMI.1")) {
        rc = command("QUIT");
        if (rc) result = 20;
        for (i = 0; i < 500 && FindPort((STRPTR)"MIAMI.1"); ++i) Delay(1);
        if (FindPort((STRPTR)"MIAMI.1")) { puts("FAIL: Miami shutdown timeout"); result = 20; }
    }
    if (RexxSysBase) CloseLibrary((struct Library *)RexxSysBase);
    if (!result) puts("PASS: Miami stack roundtrip and reconnect complete");
    return result;
}

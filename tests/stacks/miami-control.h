/* Isolated MiamiDX interface lifecycle and real UDP/SANA-II integration. */
#ifndef AOVPN_GUEST_LOG
#define AOVPN_GUEST_LOG "RunProg:miami-guest.log"
#endif
#include "../emulator/dos-entry.h"
#include <dos/dostags.h>
#include <exec/libraries.h>
#include <rexx/rxslib.h>
#include <rexx/storage.h>
#include <proto/rexxsyslib.h>
struct Library *SocketBase;
struct RxsLib *RexxSysBase;

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


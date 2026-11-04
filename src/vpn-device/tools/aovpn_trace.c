/* Read-only, chronological resident lifecycle diagnostics for vpn.device. */
#include <exec/types.h>
#include <exec/io.h>
#include <devices/sana2.h>
#include <proto/exec.h>
#include <stdio.h>
#include "../src/vpn_trace.h"

/* A fixed BSS buffer avoids exceeding the Amiga shell's default stack. */
static struct AovpnTraceDump result;

static const char *event_name(ULONG e)
{
    switch (e) {
    case AOVPN_T_OPEN: return "OPEN";
    case AOVPN_T_CLOSE_IN: return "CLOSE-IN";
    case AOVPN_T_CLOSE_OUT: return "CLOSE-OUT";
    case AOVPN_T_BEGIN: return "BEGIN";
    case AOVPN_T_DONE: return "DONE";
    case AOVPN_T_READ_QUEUED: return "READ-QUEUED";
    case AOVPN_T_EVENT_QUEUED: return "EVENT-QUEUED";
    case AOVPN_T_ABORT_IN: return "ABORT-IN";
    case AOVPN_T_ABORT_OK: return "ABORT-OK";
    case AOVPN_T_ABORT_MISS: return "ABORT-MISS";
    case AOVPN_T_ASYNC_READ_REPLY: return "READ-REPLIED";
    case AOVPN_T_ASYNC_ABORT_REPLY: return "CLOSE-ABORT";
    case AOVPN_T_ASYNC_EVENT_REPLY: return "EVENT-REPLIED";
    }
    return "UNKNOWN";
}

static const char *command_name(ULONG command)
{
    switch (command) {
    case CMD_READ: return "CMD_READ";
    case CMD_WRITE: return "CMD_WRITE";
    case CMD_FLUSH: return "CMD_FLUSH";
    case CMD_RESET: return "CMD_RESET";
    case S2_READORPHAN: return "S2_READORPHAN";
    case S2_DEVICEQUERY: return "S2_DEVICEQUERY";
    case S2_GETSTATIONADDRESS: return "S2_GETSTATIONADDRESS";
    case S2_CONFIGINTERFACE: return "S2_CONFIGINTERFACE";
    case S2_ONLINE: return "S2_ONLINE";
    case S2_OFFLINE: return "S2_OFFLINE";
    case S2_ONEVENT: return "S2_ONEVENT";
    case S2_TRACKTYPE: return "S2_TRACKTYPE";
    case S2_UNTRACKTYPE: return "S2_UNTRACKTYPE";
    case S2_GETGLOBALSTATS: return "S2_GETGLOBALSTATS";
    case S2_GETTYPESTATS: return "S2_GETTYPESTATS";
    case S2_GETSPECIALSTATS: return "S2_GETSPECIALSTATS";
    case CMD_CLEAR: return "CMD_CLEAR";
    case CMD_START: return "CMD_START";
    case CMD_STOP: return "CMD_STOP";
    }
    return "OTHER";
}

static void inspect(ULONG unit)
{
    struct MsgPort *port = CreateMsgPort();
    struct IOSana2Req *io;
    ULONG i;
    if (!port) { puts("CreateMsgPort failed"); return; }
    io = (struct IOSana2Req *)CreateIORequest(port, sizeof(*io));
    if (!io) { puts("CreateIORequest failed"); DeleteMsgPort(port); return; }
    io->ios2_BufferManagement = NULL;
    if (OpenDevice((STRPTR)"vpn.device", unit, (struct IORequest *)io, 0)) {
        printf("unit %lu: OpenDevice failed (error %ld)\n",
               (unsigned long)unit, (long)io->ios2_Req.io_Error);
    } else {
        io->ios2_Req.io_Command = AOVPN_TRACE_CMD;
        io->ios2_StatData = &result;
        io->ios2_DataLength = sizeof(result);
        if (DoIO((struct IORequest *)io) || result.magic != AOVPN_TRACE_MAGIC ||
            result.version != AOVPN_TRACE_VERSION || result.size != sizeof(result) ||
            result.count > AOVPN_TRACE_CAP) {
            printf("unit %lu: unsupported TRACE1 command / error %ld\n",
                   (unsigned long)unit, (long)io->ios2_Req.io_Error);
        } else {
            printf("\nUnit %lu: events=%lu, total sequence=%lu (last %lu only)\n",
                   (unsigned long)result.unit, (unsigned long)result.count,
                   (unsigned long)result.next_seq, (unsigned long)AOVPN_TRACE_CAP);
            puts(" SEQ  EVENT           COMMAND              REQ       TYPE     FLAGS NT ERR WIRE     UP OP RQ OQ EQ FIFO");
            for (i = 0; i < result.count; ++i) {
                const struct AovpnTraceEntry *e = &result.entries[i];
                printf("%4lu  %-14s  %-20s  %08lx  %04lx     %02lx    %lu  %3lu %08lx %lu  %lu  %lu  %lu  %lu  %lu\n",
                    (unsigned long)e->seq, event_name(e->event), command_name(e->command),
                    (unsigned long)e->request, (unsigned long)e->type,
                    (unsigned long)e->flags, (unsigned long)e->node_type,
                    (unsigned long)e->error, (unsigned long)e->wire_error,
                    (unsigned long)e->online, (unsigned long)e->opens,
                    (unsigned long)e->reads, (unsigned long)e->orphans,
                    (unsigned long)e->events, (unsigned long)e->fifo);
            }
        }
        CloseDevice((struct IORequest *)io);
    }
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(port);
}

int main(void)
{
    puts("AOVPN-Trace TRACE1 (read-only chronological I/O lifecycle)");
    inspect(0);
    inspect(1);
    return 0;
}

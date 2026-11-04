/* Read-only live diagnostics for vpn.device POC2 FIX2. */
#include <exec/types.h>
#include <exec/io.h>
#include <devices/sana2.h>
#include <proto/exec.h>
#include <stdio.h>
#include "../src/vpn_diag.h"
static void inspect(ULONG unit)
{
    struct MsgPort *port = CreateMsgPort();
    struct IOSana2Req *io = NULL;
    struct AovpnDiag d;
    if (!port) { puts("CreateMsgPort failed"); return; }
    io = (struct IOSana2Req *)CreateIORequest(port, sizeof(*io));
    if (!io) { puts("CreateIORequest failed"); DeleteMsgPort(port); return; }
    io->ios2_BufferManagement = NULL; /* no copy callbacks needed for private query */
    if (OpenDevice((STRPTR)"vpn.device", unit, (struct IORequest *)io, 0)) {
        printf("unit %lu: OpenDevice failed (%ld)\n", (unsigned long)unit, (long)io->ios2_Req.io_Error);
    } else {
        io->ios2_Req.io_Command = AOVPN_DIAG_CMD;
        io->ios2_StatData = &d;
        io->ios2_DataLength = sizeof(d);
        if (DoIO((struct IORequest *)io) || d.magic != AOVPN_DIAG_MAGIC || d.version != AOVPN_DIAG_VERSION || d.size != sizeof(d)) {
            printf("unit %lu: diagnostic command unsupported or failed (error %ld); install FIX2 driver\n", (unsigned long)unit, (long)io->ios2_Req.io_Error);
        } else {
            printf("\nUnit %lu: online=%lu opens=%lu fifo=%lu\n", (unsigned long)d.unit, (unsigned long)d.online, (unsigned long)d.open_count, (unsigned long)d.fifo_count);
            printf("  WRITE received=%lu accepted=%lu rejected=%lu\n", (unsigned long)d.write_received, (unsigned long)d.write_accepted, (unsigned long)d.write_rejected);
            printf("  Last WRITE type=0x%08lx flags=0x%08lx error=%lu wireerror=%lu\n", (unsigned long)d.last_write_type, (unsigned long)d.last_write_flags, (unsigned long)d.last_write_error, (unsigned long)d.last_write_wire_error);
            printf("  Reject: offline=%lu type/raw=%lu length=%lu ring=%lu copy=%lu\n", (unsigned long)d.reject_offline, (unsigned long)d.reject_type_or_raw, (unsigned long)d.reject_length, (unsigned long)d.reject_ring, (unsigned long)d.reject_copy);
            printf("  READ submitted=%lu queued=%lu completed=%lu errors=%lu\n", (unsigned long)d.read_submitted, (unsigned long)d.read_queued, (unsigned long)d.read_completed, (unsigned long)d.read_errors);
            printf("  FIFO enqueued=%lu delivered=%lu\n", (unsigned long)d.fifo_enqueued, (unsigned long)d.fifo_delivered);
        }
        CloseDevice((struct IORequest *)io);
    }
    DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(port);
}
int main(void)
{
    puts("AOVPN-Diag POC2 FIX2 (read-only, live counters)");
    inspect(0); inspect(1);
    return 0;
}

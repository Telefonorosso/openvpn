/* SPDX-License-Identifier: BSD-2-Clause
 * AOVPN-VPNProbe (shell executable): no argv use, as newlib argv is broken
 * on the validated AmigaOS environment. Tests unit 0 <-> unit 1 without
 * MiamiDX so we can isolate the Exec/SANA-II implementation first.
 */
#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <devices/sana2.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <stdio.h>

struct Channel {
    struct MsgPort *port;
    struct IOSana2Req *io;
    struct TagItem tags[3];
};

static BOOL hookcopy(APTR to asm("a0"), APTR from asm("a1"), ULONG len asm("d0"))
{
    CopyMem(from, to, len);
    return TRUE;
}

static BOOL start_channel(struct Channel *ch, ULONG unit)
{
    ch->port = CreateMsgPort();
    ch->io = NULL;
    if (!ch->port) return FALSE;
    ch->io = (struct IOSana2Req *)CreateIORequest(ch->port, sizeof(struct IOSana2Req));
    if (!ch->io) return FALSE;
    ch->tags[0].ti_Tag = S2_CopyToBuff;
    ch->tags[0].ti_Data = (ULONG)hookcopy;
    ch->tags[1].ti_Tag = S2_CopyFromBuff;
    ch->tags[1].ti_Data = (ULONG)hookcopy;
    ch->tags[2].ti_Tag = TAG_DONE;
    ch->tags[2].ti_Data = 0;
    ch->io->ios2_BufferManagement = ch->tags;
    if (OpenDevice((STRPTR)"vpn.device", unit, (struct IORequest *)ch->io, 0)) {
        ch->io->ios2_Req.io_Device = NULL;
        return FALSE;
    }
    return TRUE;
}

static void stop_channel(struct Channel *ch)
{
    if (ch->io) {
        if (ch->io->ios2_Req.io_Device)
            CloseDevice((struct IORequest *)ch->io);
        DeleteIORequest((struct IORequest *)ch->io);
        ch->io = NULL;
    }
    if (ch->port) { DeleteMsgPort(ch->port); ch->port = NULL; }
}

static BOOL online(struct Channel *ch)
{
    ch->io->ios2_Req.io_Command = S2_ONLINE;
    return DoIO((struct IORequest *)ch->io) == 0;
}

static BOOL query(struct Channel *ch)
{
    struct Sana2DeviceQuery q;
    q.SizeAvailable = sizeof(q);
    ch->io->ios2_Req.io_Command = S2_DEVICEQUERY;
    ch->io->ios2_StatData = &q;
    if (DoIO((struct IORequest *)ch->io)) return FALSE;
    return q.MTU == 1400 && q.HardwareType == S2WireType_PPP && q.AddrFieldSize == 32;
}

static BOOL one_way(struct Channel *tx, struct Channel *rx, int dir)
{
    UBYTE out[96];
    UBYTE in[96];
    ULONG i;
    LONG write_rc;
    for (i = 0; i < sizeof(out); ++i) {
        out[i] = (UBYTE)((i + dir * 13) & 0xff);
        in[i] = 0;
    }
    out[0] = 0x45; /* recognizable IPv4 first nibble; no network inject */
    rx->io->ios2_Req.io_Command = CMD_READ;
    rx->io->ios2_PacketType = 0x0800;
    rx->io->ios2_Data = in;
    rx->io->ios2_DataLength = sizeof(in);
    SendIO((struct IORequest *)rx->io);

    tx->io->ios2_Req.io_Command = CMD_WRITE;
    tx->io->ios2_PacketType = 0x0800;
    tx->io->ios2_Data = out;
    tx->io->ios2_DataLength = sizeof(out);
    tx->io->ios2_SrcAddr[0] = 10;
    tx->io->ios2_SrcAddr[1] = 77;
    tx->io->ios2_SrcAddr[2] = 0;
    tx->io->ios2_SrcAddr[3] = (UBYTE)(dir + 1);
    write_rc = DoIO((struct IORequest *)tx->io);
    if (write_rc != 0) {
        AbortIO((struct IORequest *)rx->io);
        WaitIO((struct IORequest *)rx->io);
        printf("FAIL: write direction %d, error %ld\n", dir, (long)write_rc);
        return FALSE;
    }
    if (WaitIO((struct IORequest *)rx->io)) {
        puts("FAIL: queued read failed");
        return FALSE;
    }
    if (rx->io->ios2_DataLength != sizeof(out) ||
        rx->io->ios2_PacketType != 0x0800) {
        puts("FAIL: packet metadata mismatch");
        return FALSE;
    }
    for (i = 0; i < sizeof(out); ++i)
        if (out[i] != in[i]) { puts("FAIL: payload mismatch"); return FALSE; }
    printf("PASS: unit %d -> unit %d (%lu IPv4 bytes)\n", dir, 1-dir, (unsigned long)sizeof(out));
    return TRUE;
}

/* Exercises the bounded packet queue when the reader is not armed yet. */
static BOOL buffered_one_way(struct Channel *tx, struct Channel *rx)
{
    UBYTE out[32], in[32];
    ULONG i;
    for (i = 0; i < sizeof(out); ++i) { out[i] = (UBYTE)(0x60 + i); in[i] = 0; }
    out[0] = 0x45;
    tx->io->ios2_Req.io_Command = CMD_WRITE;
    tx->io->ios2_PacketType = 0x0800;
    tx->io->ios2_Data = out;
    tx->io->ios2_DataLength = sizeof(out);
    if (DoIO((struct IORequest *)tx->io)) { puts("FAIL: buffered write"); return FALSE; }
    rx->io->ios2_Req.io_Command = CMD_READ;
    rx->io->ios2_PacketType = 0x0800;
    rx->io->ios2_Data = in;
    rx->io->ios2_DataLength = sizeof(in);
    if (DoIO((struct IORequest *)rx->io)) { puts("FAIL: buffered read"); return FALSE; }
    for (i = 0; i < sizeof(out); ++i)
        if (out[i] != in[i]) { puts("FAIL: buffered payload"); return FALSE; }
    puts("PASS: buffered write-before-read FIFO");
    return TRUE;
}

static BOOL abort_pending(struct Channel *rx)
{
    UBYTE in[32];
    LONG result;
    rx->io->ios2_Req.io_Command = CMD_READ;
    rx->io->ios2_PacketType = 0x0800;
    rx->io->ios2_Data = in;
    rx->io->ios2_DataLength = sizeof(in);
    SendIO((struct IORequest *)rx->io);
    AbortIO((struct IORequest *)rx->io);
    result = WaitIO((struct IORequest *)rx->io);
    if (result != IOERR_ABORTED) {
        printf("FAIL: AbortIO/WaitIO returned %ld\n", (long)result);
        return FALSE;
    }
    puts("PASS: queued CMD_READ aborted and replied once");
    return TRUE;
}

int main(void)
{
    struct Channel u0 = {0};
    struct Channel u1 = {0};
    int ok = 0;
    puts("AOVPN-VPNProbe: vpn.device POC1 (no MiamiDX, no network)");
    if (!start_channel(&u0, 0) || !start_channel(&u1, 1)) {
        puts("FAIL: OpenDevice(vpn.device) -- install it in DEVS:");
        goto done;
    }
    if (!query(&u0) || !query(&u1)) {
        puts("FAIL: S2_DEVICEQUERY"); goto done;
    }
    puts("PASS: S2_DEVICEQUERY, PPP wire, MTU 1400");
    if (!online(&u0) || !online(&u1)) {
        puts("FAIL: S2_ONLINE"); goto done;
    }
    if (!one_way(&u0, &u1, 0) || !one_way(&u1, &u0, 1))
        goto done;
    if (!buffered_one_way(&u0, &u1) || !abort_pending(&u1))
        goto done;
    ok = 1;
    puts("PASS: vpn.device SANA-II bidirectional IP transport");
done:
    stop_channel(&u1);
    stop_channel(&u0);
    return ok ? 0 : 20;
}

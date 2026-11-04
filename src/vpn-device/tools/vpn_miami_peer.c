/* SPDX-License-Identifier: BSD-2-Clause
 * POC2 test tool: isolated MiamiDX -> vpn.device -> ICMP Echo responder.
 * Uses only vpn.device UNIT ONE, never opens/modifies UNIT ZERO.
 * No TCP/IP libraries, no AmiSSL, no changes to the POC1 device driver.
 */
#include <exec/types.h>
#include <exec/io.h>
#include <exec/tasks.h>
#include <devices/sana2.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <stdio.h>
#include "../src/icmp_echo_core.h"

#define TEST_DEVICE "vpn.device"
#define TEST_UNIT 1UL
#define TEST_MTU 1400U
#define IPV4_TYPE 0x0800UL

struct Peer {
    struct MsgPort *ctl_port;
    struct MsgPort *rx_port;
    struct MsgPort *tx_port;
    struct IOSana2Req *ctl;
    struct IOSana2Req *rx;
    struct IOSana2Req *tx;
    struct TagItem tags[3];
    int opened;
    int pending;
    unsigned long seen;
    unsigned long answered;
    unsigned long rejected;
    UBYTE packet[TEST_MTU];
};

/* Identical 68k callback ABI used by hardware-validated AOVPN-VPNProbe. */
static BOOL hook_copy(APTR to asm("a0"), APTR from asm("a1"), ULONG len asm("d0"))
{
    CopyMem(from, to, len);
    return TRUE;
}
static void clone_open(struct IOSana2Req *dst, const struct IOSana2Req *src)
{
    dst->ios2_Req.io_Device = src->ios2_Req.io_Device;
    dst->ios2_Req.io_Unit = src->ios2_Req.io_Unit;
    dst->ios2_BufferManagement = src->ios2_BufferManagement;
}
static int peer_open(struct Peer *p)
{
    struct Sana2DeviceQuery query;
    LONG rc;
    p->ctl_port = CreateMsgPort();
    p->rx_port = CreateMsgPort();
    p->tx_port = CreateMsgPort();
    if (!p->ctl_port || !p->rx_port || !p->tx_port) return 0;
    p->ctl = (struct IOSana2Req *)CreateIORequest(p->ctl_port, sizeof(*p->ctl));
    p->rx = (struct IOSana2Req *)CreateIORequest(p->rx_port, sizeof(*p->rx));
    p->tx = (struct IOSana2Req *)CreateIORequest(p->tx_port, sizeof(*p->tx));
    if (!p->ctl || !p->rx || !p->tx) return 0;
    p->tags[0].ti_Tag = S2_CopyToBuff;
    p->tags[0].ti_Data = (ULONG)hook_copy;
    p->tags[1].ti_Tag = S2_CopyFromBuff;
    p->tags[1].ti_Data = (ULONG)hook_copy;
    p->tags[2].ti_Tag = TAG_DONE;
    p->tags[2].ti_Data = 0;
    p->ctl->ios2_BufferManagement = p->tags;
    rc = OpenDevice((STRPTR)TEST_DEVICE, TEST_UNIT, (struct IORequest *)p->ctl, 0);
    if (rc) {
        printf("FAIL: OpenDevice(vpn.device, unit 1) rc=%ld\n", (long)rc);
        return 0;
    }
    p->opened = 1;
    clone_open(p->rx, p->ctl);
    clone_open(p->tx, p->ctl);

    query.SizeAvailable = sizeof(query);
    p->ctl->ios2_Req.io_Command = S2_DEVICEQUERY;
    p->ctl->ios2_StatData = &query;
    rc = DoIO((struct IORequest *)p->ctl);
    p->ctl->ios2_StatData = NULL;
    if (rc || query.MTU != TEST_MTU || query.HardwareType != S2WireType_PPP) {
        printf("FAIL: S2_DEVICEQUERY rc=%ld MTU=%lu wire=%lu\n",
               (long)rc, (unsigned long)query.MTU,
               (unsigned long)query.HardwareType);
        return 0;
    }
    p->ctl->ios2_Req.io_Command = S2_CONFIGINTERFACE;
    p->ctl->ios2_SrcAddr[0] = 10;
    p->ctl->ios2_SrcAddr[1] = 77;
    p->ctl->ios2_SrcAddr[2] = 0;
    p->ctl->ios2_SrcAddr[3] = 2;
    rc = DoIO((struct IORequest *)p->ctl);
    if (rc) {
        printf("FAIL: S2_CONFIGINTERFACE unit1 rc=%ld\n", (long)rc);
        return 0;
    }
    p->ctl->ios2_Req.io_Command = S2_ONLINE;
    rc = DoIO((struct IORequest *)p->ctl);
    if (rc) {
        printf("FAIL: S2_ONLINE unit1 rc=%ld\n", (long)rc);
        return 0;
    }
    return 1;
}
static void peer_shutdown(struct Peer *p)
{
    if (p->pending) {
        AbortIO((struct IORequest *)p->rx);
        (void)WaitIO((struct IORequest *)p->rx);
        p->pending = 0;
    }
    if (p->opened) {
        p->ctl->ios2_Req.io_Command = S2_OFFLINE;
        (void)DoIO((struct IORequest *)p->ctl);
        CloseDevice((struct IORequest *)p->ctl);
        p->opened = 0;
    }
    if (p->tx) DeleteIORequest((struct IORequest *)p->tx);
    if (p->rx) DeleteIORequest((struct IORequest *)p->rx);
    if (p->ctl) DeleteIORequest((struct IORequest *)p->ctl);
    if (p->tx_port) DeleteMsgPort(p->tx_port);
    if (p->rx_port) DeleteMsgPort(p->rx_port);
    if (p->ctl_port) DeleteMsgPort(p->ctl_port);
}
static void arm_read(struct Peer *p)
{
    p->rx->ios2_Req.io_Command = CMD_READ;
    p->rx->ios2_PacketType = IPV4_TYPE;
    p->rx->ios2_DataLength = sizeof(p->packet);
    p->rx->ios2_Data = p->packet;
    p->rx->ios2_WireError = 0;
    SendIO((struct IORequest *)p->rx);
    p->pending = 1;
}
static void handle_read(struct Peer *p)
{
    ULONG n;
    LONG rc;
    p->pending = 0;
    rc = WaitIO((struct IORequest *)p->rx);
    if (rc) {
        printf("WARN: CMD_READ rc=%ld wire=%lu\n", (long)rc,
               (unsigned long)p->rx->ios2_WireError);
        return;
    }
    n = p->rx->ios2_DataLength;
    p->seen++;
    if (n > TEST_MTU || !aovpn_make_icmp_reply(p->packet, (unsigned int)n)) {
        p->rejected++;
        printf("RX: ignored IPv4 datagram length=%lu type=%lu\n",
               (unsigned long)n, (unsigned long)p->rx->ios2_PacketType);
        return;
    }
    p->tx->ios2_Req.io_Command = CMD_WRITE;
    p->tx->ios2_PacketType = IPV4_TYPE;
    p->tx->ios2_Data = p->packet;
    p->tx->ios2_DataLength = n;
    p->tx->ios2_SrcAddr[0] = 10; p->tx->ios2_SrcAddr[1] = 77;
    p->tx->ios2_SrcAddr[2] = 0;  p->tx->ios2_SrcAddr[3] = 2;
    p->tx->ios2_DstAddr[0] = 10; p->tx->ios2_DstAddr[1] = 77;
    p->tx->ios2_DstAddr[2] = 0;  p->tx->ios2_DstAddr[3] = 1;
    rc = DoIO((struct IORequest *)p->tx);
    if (rc) {
        printf("FAIL: CMD_WRITE reply rc=%ld wire=%lu\n", (long)rc,
               (unsigned long)p->tx->ios2_WireError);
        return;
    }
    p->answered++;
    printf("PASS: ICMP echo reply %lu (%lu IPv4 bytes)\n",
           p->answered, (unsigned long)n);
}
int main(void)
{
    static struct Peer p; /* avoid large task stack; initially zeroed */
    ULONG mask, signals;
    int ok = 0;
    puts("AOVPN-MiamiPeer POC2: vpn.device unit 1 only");
    puts("IPv4 peer 10.77.0.2; responds only to ICMP echo from 10.77.0.1");
    if (!peer_open(&p)) goto done;
    puts("READY: configure MiamiDX SANA-II unit 0, local 10.77.0.1, peer 10.77.0.2");
    puts("Use IP packet type 2048, then ping 10.77.0.2. Ctrl+C to finish.");
    mask = (1UL << p.rx_port->mp_SigBit) | SIGBREAKF_CTRL_C;
    arm_read(&p);
    ok = 1;
    for (;;) {
        signals = Wait(mask);
        if (signals & (1UL << p.rx_port->mp_SigBit)) {
            handle_read(&p);
            if (!(signals & SIGBREAKF_CTRL_C)) arm_read(&p);
        }
        if (signals & SIGBREAKF_CTRL_C) break;
    }
    printf("SUMMARY: observed=%lu replied=%lu ignored=%lu\n",
           p.seen, p.answered, p.rejected);
done:
    peer_shutdown(&p);
    puts("AOVPN-MiamiPeer: resources released");
    return ok ? 0 : 20;
}

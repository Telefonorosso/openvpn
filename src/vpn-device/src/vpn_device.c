/* SPDX-License-Identifier: BSD-2-Clause
 * vpn.device POC1: future-release SANA-II user-space IP link.
 * Unit 0 is the MiamiDX end; unit 1 is the AmiOpenVPN end.
 * A CMD_WRITE on one unit becomes one inbound IPv4 packet on its peer.
 * NO echo, replay, simulated faults, internal networking, or OpenVPN TLS.
 *
 * SANA-II core helpers included verbatim from Simon Dick's sana2loop
 * under its BSD-2-Clause license (see third_party/sana2loop/LICENSE).
 * The bounded packet ring is source-identical in POC and intended RC.
 * This is a freestanding 68000 / Kickstart-compatible device, *not*
 * an AmigaDOS command and *not* a dynamically loaded shared library.
 *
 * FIX4 STATUS: REAL TWO-WAY SANA-II DATA PATH, HOST-TESTED ONLY.
 * Still needs MiamiDX/hardware validation; no OpenVPN/TLS engine connected.
 */
#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <exec/io.h>
#include <exec/errors.h>
#include <exec/resident.h>
#include <exec/devices.h>
#include <exec/memory.h>
#include <devices/sana2.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include "vpn_ring.h"
#include "vpn_diag.h"
#include "vpn_trace.h"

#define STR2(x) #x
#define STR(x) STR2(x)
#define VPN_VERSION 0
#define VPN_REVISION 1
#define VPN_MAGIC 0x564E504F /* VNPO */
#define VPN_BPS 10000000UL
#define VPN_UNITS 2
#define VPN_NAME "vpn.device"
#define VPN_ID "$VER: vpn.device 0.1 (21.10.2026)"

struct ExecBase *SysBase;

typedef BOOL (*Sana2CopyFunc)(APTR to asm("a0"), APTR from asm("a1"),
                              ULONG size asm("d0"));

struct VpnOpen {
    struct Node node;
    ULONG magic;
    ULONG flags;
    Sana2CopyFunc copy_to;
    Sana2CopyFunc copy_from;
};

struct VpnUnit {
    struct Unit exec_unit; /* offset zero, usable as io_Unit */
    struct List opens;
    struct List reads;
    struct List orphans;
    struct List events;
    struct VpnRing pending;
    struct AovpnDiag diag;
    struct AovpnTraceState trace;
    struct Sana2DeviceStats stats;
    struct Sana2PacketTypeStats type_stats;
    UBYTE station[4];
    UBYTE factory[4];
    UBYTE configured;
    UBYTE online;
    UBYTE exclusive;
    UBYTE tracked;
};

struct VpnDevice {
    struct Device device; /* Exec allocations include all unit/ring storage */
    BPTR seglist;
    struct VpnUnit units[VPN_UNITS];
};

/* Freestanding resident; layout mirrors upstream sana2loop's known-good
 * Resident/auto-init strategy. _start must remain first in the text hunk. */
#ifndef VPN_HOST_MODEL
int __attribute__((no_reorder)) _start(void) { return -1; }
asm("romtag:                                \n"
    "       dc.w    " STR(RTC_MATCHWORD) "   \n"
    "       dc.l    romtag                  \n"
    "       dc.l    endcode                 \n"
    "       dc.b    " STR(RTF_AUTOINIT) "    \n"
    "       dc.b    " STR(VPN_VERSION) "     \n"
    "       dc.b    " STR(NT_DEVICE) "       \n"
    "       dc.b    0                       \n"
    "       dc.l    _vpn_name               \n"
    "       dc.l    _vpn_id                 \n"
    "       dc.l    _vpn_autoinit          \n"
    "endcode:                               \n");
#endif /* !VPN_HOST_MODEL: m68k Resident assembly */

char vpn_name[] = VPN_NAME;
char vpn_id[] = VPN_ID;

/* From sana2loop: no changes to their function bodies. */
#include "sana2_core.inc"

/* Unmodified __mulsi3 source from sana2loop. Needed by -nostdlib when GCC
 * expands 32-bit multiplication. Keep AFTER _start/Resident, as upstream. */
#ifndef VPN_HOST_MODEL
ULONG __mulsi3(ULONG a, ULONG b)
{
    ULONG low    = a & 0xFFFF;         /* al */
    ULONG cross1 = a & 0xFFFF;         /* al (second copy, for al*bh) */
    ULONG cross2 = a >> 16;            /* ah */

    __asm__("mulu.w %1,%0" : "+d"(low)    : "d"((UWORD)b));         /* al*bl */
    __asm__("mulu.w %1,%0" : "+d"(cross1) : "d"((UWORD)(b >> 16))); /* al*bh */
    __asm__("mulu.w %1,%0" : "+d"(cross2) : "d"((UWORD)b));         /* ah*bl */

    return low + ((cross1 + cross2) << 16);
}
#endif

static BOOL has_open(struct VpnUnit *u, const APTR cookie)
{
    struct Node *n;
    for (n = u->opens.lh_Head; n->ln_Succ; n = n->ln_Succ)
        if ((APTR)n == cookie)
            return TRUE;
    return FALSE;
}

static BOOL capture_tags(struct VpnOpen *op, struct TagItem *tag)
{
    unsigned n = 0;
    op->copy_to = default_copy;
    op->copy_from = default_copy;
    while (tag && n++ < 128) {
        switch (tag->ti_Tag) {
        case TAG_DONE: return TRUE;
        case TAG_IGNORE: break;
        case TAG_MORE: tag = (struct TagItem *)tag->ti_Data; continue;
        case TAG_SKIP: tag += tag->ti_Data; break;
        case S2_CopyToBuff: op->copy_to = (Sana2CopyFunc)tag->ti_Data; break;
        case S2_CopyFromBuff: op->copy_from = (Sana2CopyFunc)tag->ti_Data; break;
        default: break; /* Optional hooks may be declined under SANA-II */
        }
        ++tag;
    }
    return tag == NULL; /* no tags: defaults; unterminated tag chain: reject */
}

/* TRACE1: bounded, in-memory event history, no DOS, no allocations.
 * We log the real production request fields and queue depths. Trace is
 * protected by Exec Disable/Enable, like the existing device queues. */
static ULONG trace_queue_depth(const struct List *q)
{
    const struct Node *n;
    ULONG count = 0;
    for (n = q->lh_Head; n->ln_Succ && count < 1024; n = n->ln_Succ)
        ++count;
    return count;
}

static void trace_event(struct VpnUnit *u, ULONG what, const struct IORequest *io)
{
    struct AovpnTraceState *t;
    struct AovpnTraceEntry *e;
    ULONG slot;
    if (!u) return;
    Disable();
    t = &u->trace;
    slot = t->next_seq % AOVPN_TRACE_CAP;
    e = &t->entries[slot];
    e->seq = ++t->next_seq;
    e->event = what;
    e->request = (ULONG)io;
    e->command = io ? io->io_Command : 0;
    e->type = io ? ((const struct IOSana2Req *)io)->ios2_PacketType : 0;
    e->flags = io ? io->io_Flags : 0;
    e->node_type = io ? io->io_Message.mn_Node.ln_Type : 0;
    e->error = io ? (ULONG)(UBYTE)io->io_Error : 0;
    e->wire_error = io ? ((const struct IOSana2Req *)io)->ios2_WireError : 0;
    e->online = u->online;
    e->opens = u->exec_unit.unit_OpenCnt;
    e->reads = trace_queue_depth(&u->reads);
    e->orphans = trace_queue_depth(&u->orphans);
    e->events = trace_queue_depth(&u->events);
    e->fifo = u->pending.count;
    if (t->count < AOVPN_TRACE_CAP) ++t->count;
    Enable();
}

static void answer(struct IORequest *io)
{
    if (!(io->io_Flags & IOF_QUICK))
        ReplyMsg(&io->io_Message);
}

static void abort_waiters(struct List *q)
{
    struct Node *n;
    while ((n = safe_rem_head(q)) != NULL) {
        struct IORequest *io = (struct IORequest *)n;
        io->io_Error = IOERR_ABORTED;
        ReplyMsg(&io->io_Message);
    }
}

static void abort_one_open(struct VpnUnit *u, struct VpnOpen *op)
{
    struct List *q[3];
    unsigned k;
    q[0] = &u->reads;
    q[1] = &u->orphans;
    q[2] = &u->events;
    for (k = 0; k < 3; ++k) {
        struct Node *n, *next;
        Disable();
        for (n = q[k]->lh_Head; n->ln_Succ; n = next) {
            struct IOSana2Req *req = (struct IOSana2Req *)n;
            next = n->ln_Succ;
            if (req->ios2_BufferManagement == (APTR)op) {
                Remove(n);
                req->ios2_Req.io_Error = IOERR_ABORTED;
                trace_event(u, AOVPN_T_ASYNC_ABORT_REPLY, &req->ios2_Req);
                ReplyMsg(&req->ios2_Req.io_Message);
            }
        }
        Enable();
    }
}

static void fire_event(struct VpnUnit *u, ULONG event)
{
    struct Node *n, *next;
    Disable();
    for (n = u->events.lh_Head; n->ln_Succ; n = next) {
        struct IOSana2Req *req = (struct IOSana2Req *)n;
        next = n->ln_Succ;
        if (req->ios2_WireError & event) {
            Remove(n);
            req->ios2_WireError = event;
            req->ios2_Req.io_Error = 0;
            trace_event(u, AOVPN_T_ASYNC_EVENT_REPLY, &req->ios2_Req);
            ReplyMsg(&req->ios2_Req.io_Message);
        }
    }
    Enable();
}

/* This function is called WITH task switching disabled. The copy hook is
 * required by SANA-II to be IRQ safe. POC1 copies at most VPN_MTU bytes
 * under Disable(): bounded latency, but measure it before declaring RC. */
/* FIX4: PPP IPv4 0x0021 is accepted for both CMD_READ and CMD_WRITE.
 * Tracking/statistics acceptance remains at FIX3B/FIX3C behavior. */
static BOOL ipv4_type(ULONG type)
{
    return type == VPN_IPV4_PACKET_TYPE || type == VPN_PPP_IPV4_PACKET_TYPE ||
           type == VPN_PPP_IP_PROTOCOL;
}

/* Only control-plane tracking/statistics preserve FIX2 type acceptance. */
static BOOL ipv4_tracking_type(ULONG type)
{
    return type == VPN_IPV4_PACKET_TYPE || type == VPN_PPP_IPV4_PACKET_TYPE;
}

static void complete_read(struct VpnUnit *u, struct IOSana2Req *req, struct VpnFrame *frame)
{
    struct VpnOpen *op = (struct VpnOpen *)req->ios2_BufferManagement;
    struct IORequest *io = &req->ios2_Req;
    unsigned i;
    for (i = 0; i < 4; ++i) {
        req->ios2_SrcAddr[i] = frame->src[i];
        req->ios2_DstAddr[i] = frame->dst[i];
    }
    if (req->ios2_Req.io_Command == S2_READORPHAN)
        req->ios2_PacketType = frame->packet_type;
    req->ios2_DataLength = frame->length;
    if (!req->ios2_Data || !op->copy_to(req->ios2_Data, frame->payload, frame->length)) {
        io->io_Error = S2ERR_NO_RESOURCES;
        req->ios2_WireError = S2WERR_BUFF_ERROR;
        u->diag.read_errors++;
    } else {
        u->diag.read_completed++;
        io->io_Error = 0;
        req->ios2_WireError = 0;
    }
    trace_event(u, AOVPN_T_ASYNC_READ_REPLY, io);
    ReplyMsg(&io->io_Message);
}

/* Caller holds Disable(). A read takes one full IP datagram; the frame is
 * removed from the FIFO only after its CopyToBuff attempt. */
static void deliver_pending(struct VpnUnit *u)
{
    while (u->pending.count) {
        struct Node *n;
        struct VpnFrame *f = vpn_ring_peek(&u->pending);
        if (!f) break;
        n = RemHead(&u->reads);
        if (!n) n = RemHead(&u->orphans);
        if (!n) break;
        complete_read(u, (struct IOSana2Req *)n, f);
        u->diag.fifo_delivered++;
        vpn_ring_pop(&u->pending);
        u->stats.PacketsReceived++;
        if (u->tracked) {
            u->type_stats.PacketsReceived++;
            u->type_stats.BytesReceived += f->length;
        }
    }
}

static void do_write(struct VpnDevice *d, struct VpnUnit *u,
                     struct IOSana2Req *req)
{
    struct VpnUnit *to = (u == &d->units[0]) ? &d->units[1] : &d->units[0];
    struct VpnOpen *op = (struct VpnOpen *)req->ios2_BufferManagement;
    struct VpnFrame *f;
    ULONG len = req->ios2_DataLength;
    unsigned i;
    u->diag.write_received++;
    u->diag.last_write_type = req->ios2_PacketType;
    u->diag.last_write_flags = req->ios2_Req.io_Flags;
    u->diag.last_write_error = 0;
    u->diag.last_write_wire_error = 0;

    if (!u->online || !to->online || !to->exec_unit.unit_OpenCnt) {
        req->ios2_Req.io_Error = S2ERR_OUTOFSERVICE;
        u->diag.reject_offline++;
        return;
    }
    if (req->ios2_Req.io_Flags & SANA2IOF_RAW ||
        !ipv4_type(req->ios2_PacketType)) {
        req->ios2_Req.io_Error = S2ERR_NOT_SUPPORTED;
        u->diag.reject_type_or_raw++;
        return;
    }
    if (!req->ios2_Data || !len || len > VPN_MTU) {
        req->ios2_Req.io_Error = len > VPN_MTU ? S2ERR_MTU_EXCEEDED : S2ERR_BAD_ARGUMENT;
        u->diag.reject_length++;
        return;
    }
    Disable();
    f = vpn_ring_reserve(&to->pending);
    if (!f) {
        to->stats.Overruns++; /* bounded receive queue saturation */
        Enable();
        req->ios2_Req.io_Error = S2ERR_NO_RESOURCES;
        u->diag.reject_ring++;
        return;
    }
    f->length = (unsigned short)len;
    f->packet_type = VPN_IPV4_PACKET_TYPE;
    for (i = 0; i < 4; ++i) {
        f->src[i] = req->ios2_SrcAddr[i];
        f->dst[i] = req->ios2_DstAddr[i];
    }
    if (!op->copy_from(f->payload, req->ios2_Data, len)) {
        /* The new frame is the last one; roll back the reservation. */
        to->pending.count--;
        Enable();
        req->ios2_Req.io_Error = S2ERR_NO_RESOURCES;
        u->diag.reject_copy++;
        req->ios2_WireError = S2WERR_BUFF_ERROR;
        return;
    }
    u->diag.write_accepted++;
    to->diag.fifo_enqueued++;
    u->stats.PacketsSent++;
    if (u->tracked) {
        u->type_stats.PacketsSent++;
        u->type_stats.BytesSent += len;
    }
    deliver_pending(to);
    Enable();
}

/* Init is called by Exec while forbidden. No DOS or other libraries. */
static struct Library * __attribute__((used))
vpn_init(BPTR seg asm("a0"), struct Library *lib asm("d0"))
{
    struct VpnDevice *d = (struct VpnDevice *)lib;
    unsigned i;
#ifndef VPN_HOST_MODEL
    SysBase = *(struct ExecBase **)4UL;
#endif
    lib->lib_Node.ln_Type = NT_DEVICE;
    lib->lib_Node.ln_Name = vpn_name;
    lib->lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
    lib->lib_Version = VPN_VERSION;
    lib->lib_Revision = VPN_REVISION;
    lib->lib_IdString = (APTR)vpn_id;
    lib->lib_OpenCnt = 0;
    d->seglist = seg;
    for (i = 0; i < VPN_UNITS; ++i) {
        struct VpnUnit *u = &d->units[i];
        u->exec_unit.unit_OpenCnt = 0;
        list_init(&u->opens);
        list_init(&u->reads);
        list_init(&u->orphans);
        list_init(&u->events);
        vpn_ring_clear(&u->pending);
        { UBYTE *b=(UBYTE *)&u->diag; unsigned j; for(j=0;j<sizeof(u->diag);++j)b[j]=0; }
        u->factory[0] = 10; u->factory[1] = 77;
        u->factory[2] = 0; u->factory[3] = (UBYTE)(i + 1);
        CopyMem(u->factory, u->station, 4);
        u->online = FALSE;
        u->trace.next_seq = 0;
        u->trace.count = 0;
    }
    return lib;
}

static BPTR __attribute__((used)) vpn_expunge(struct Library *lib asm("a6"))
{
    struct VpnDevice *d = (struct VpnDevice *)lib;
    BPTR seg;
    if (lib->lib_OpenCnt) {
        lib->lib_Flags |= LIBF_DELEXP;
        return 0;
    }
    seg = d->seglist;
    Remove(&lib->lib_Node);
    FreeMem((UBYTE *)lib - lib->lib_NegSize, lib->lib_NegSize + lib->lib_PosSize);
    return seg;
}

static void __attribute__((used))
vpn_open(struct Library *lib asm("a6"), struct IORequest *io asm("a1"),
         ULONG index asm("d0"), ULONG flags asm("d1"))
{
    struct VpnDevice *d = (struct VpnDevice *)lib;
    struct IOSana2Req *req = (struct IOSana2Req *)io;
    struct VpnUnit *u;
    struct VpnOpen *op;
    io->io_Error = S2ERR_BAD_ARGUMENT;
    io->io_Message.mn_Node.ln_Type = NT_REPLYMSG;
    if (index >= VPN_UNITS) return;
    u = &d->units[index];
    if ((u->exclusive || ((flags & SANA2OPF_MINE) && u->exec_unit.unit_OpenCnt))) {
        io->io_Error = IOERR_UNITBUSY;
        return;
    }
    op = (struct VpnOpen *)AllocMem(sizeof(*op), MEMF_CLEAR);
    if (!op) return;
    if (!capture_tags(op, (struct TagItem *)req->ios2_BufferManagement)) {
        FreeMem(op, sizeof(*op));
        return;
    }
    op->magic = VPN_MAGIC;
    op->flags = flags;
    AddTail(&u->opens, &op->node);
    if (flags & SANA2OPF_MINE) u->exclusive = TRUE;
    u->exec_unit.unit_OpenCnt++;
    lib->lib_OpenCnt++;
    lib->lib_Flags &= ~LIBF_DELEXP;
    io->io_Device = (struct Device *)lib;
    io->io_Unit = (struct Unit *)u;
    req->ios2_BufferManagement = (APTR)op;
    io->io_Error = 0;
}

static BPTR __attribute__((used))
vpn_close(struct Library *lib asm("a6"), struct IORequest *io asm("a1"))
{
    struct VpnUnit *u = (struct VpnUnit *)io->io_Unit;
    struct IOSana2Req *req = (struct IOSana2Req *)io;
    struct VpnOpen *op = (struct VpnOpen *)req->ios2_BufferManagement;
    if (!u || !op) return 0;
    trace_event(u, AOVPN_T_CLOSE_IN, io);
    abort_one_open(u, op);
    Disable();
    Remove(&op->node);
    Enable();
    op->magic = 0;
    if (op->flags & SANA2OPF_MINE) u->exclusive = FALSE;
    FreeMem(op, sizeof(*op));
    req->ios2_BufferManagement = NULL;
    io->io_Unit = NULL;
    io->io_Device = NULL;
    u->exec_unit.unit_OpenCnt--;
    lib->lib_OpenCnt--;
    if (!u->exec_unit.unit_OpenCnt) {
        u->online = FALSE;
        u->configured = FALSE;
        u->tracked = FALSE;
        vpn_ring_clear(&u->pending);
        abort_waiters(&u->reads);
        abort_waiters(&u->orphans);
        abort_waiters(&u->events);
    }
    trace_event(u, AOVPN_T_CLOSE_OUT, io);
    if (!lib->lib_OpenCnt && (lib->lib_Flags & LIBF_DELEXP))
        return vpn_expunge(lib);
    return 0;
}

static void __attribute__((used))
vpn_beginio(struct Library *lib asm("a6"), struct IORequest *io asm("a1"))
{
    struct VpnDevice *d = (struct VpnDevice *)lib;
    struct IOSana2Req *req = (struct IOSana2Req *)io;
    struct VpnUnit *u = (struct VpnUnit *)io->io_Unit;
    ULONG event_mask = req->ios2_WireError;
    BOOL trace_enabled = io->io_Command != AOVPN_DIAG_CMD && io->io_Command != AOVPN_TRACE_CMD;
    struct VpnFrame *f;
    ULONG avail;
    unsigned i;
    /* FIX3D: mark an IORequest as in-flight on every BeginIO, including
     * requests previously completed/replied and subsequently reused.
     * Exec CheckIO() uses mn_Node.ln_Type to distinguish pending I/O from
     * NT_REPLYMSG; mandatory for asynchronously queued CMD_READ requests.
     * Keep the existing IOF_QUICK and ReplyMsg handling unchanged. */
    if (u && (u == &d->units[0] || u == &d->units[1]) && trace_enabled)
        trace_event(u, AOVPN_T_BEGIN, io);
    io->io_Message.mn_Node.ln_Type = NT_MESSAGE;
    io->io_Error = 0;
    req->ios2_WireError = 0;
    if (!u || (u != &d->units[0] && u != &d->units[1]) ||
        !has_open(u, req->ios2_BufferManagement)) {
        io->io_Error = S2ERR_BAD_ARGUMENT;
        answer(io);
        return;
    }

    switch (io->io_Command) {
    case CMD_READ:
    case S2_READORPHAN:
        u->diag.read_submitted++;
        if (io->io_Command == CMD_READ && !ipv4_type(req->ios2_PacketType)) {
            io->io_Error = S2ERR_NOT_SUPPORTED;
            break;
        }
        if (!u->online) { io->io_Error = S2ERR_OUTOFSERVICE; break; }
        Disable();
        f = vpn_ring_peek(&u->pending);
        if (f) {
            struct VpnOpen *op = (struct VpnOpen *)req->ios2_BufferManagement;
            for (i = 0; i < 4; ++i) {
                req->ios2_SrcAddr[i] = f->src[i];
                req->ios2_DstAddr[i] = f->dst[i];
            }
            if (io->io_Command == S2_READORPHAN)
                req->ios2_PacketType = f->packet_type;
            req->ios2_DataLength = f->length;
            if (!req->ios2_Data || !op->copy_to(req->ios2_Data, f->payload, f->length)) {
                u->diag.read_errors++;
                io->io_Error = S2ERR_NO_RESOURCES;
                req->ios2_WireError = S2WERR_BUFF_ERROR;
            } else {
                u->diag.read_completed++;
                u->stats.PacketsReceived++;
                if (u->tracked) {
                    u->type_stats.PacketsReceived++;
                    u->type_stats.BytesReceived += f->length;
                }
            }
            vpn_ring_pop(&u->pending);
            u->diag.fifo_delivered++;
        } else {
            u->diag.read_queued++;
            io->io_Flags &= ~IOF_QUICK;
            /* Calling shared helper in the Disable region is safe:
             * Exec's Disable nesting is balanced inside the helper. */
            safe_add_tail(io->io_Command == CMD_READ ? &u->reads : &u->orphans,
                          (struct Node *)io);
            Enable();
            if (trace_enabled) trace_event(u, AOVPN_T_READ_QUEUED, io);
            return;
        }
        Enable();
        break;

    case CMD_WRITE:
        do_write(d, u, req);
        u->diag.write_rejected += (io->io_Error != 0);
        u->diag.last_write_error = (ULONG)(UBYTE)io->io_Error;
        u->diag.last_write_wire_error = req->ios2_WireError;
        break;

    case CMD_FLUSH:
        /* SANA-II: flush data-plane I/O, NOT S2_ONEVENT subscriptions.
         * MiamiDX leaves S2_ONEVENT(S2EVENT_OFFLINE) outstanding while
         * flushing receive requests.  Replying IOERR_ABORTED to that
         * subscription during CMD_FLUSH is not an OFFLINE notification.
         * Match the proven sana2loop/loopback.device CMD_FLUSH semantics.
         */
        abort_waiters(&u->reads);
        abort_waiters(&u->orphans);
        Disable();
        vpn_ring_clear(&u->pending);
        Enable();
        break;

    case CMD_RESET:
        /* Preserve TRACE1's reset behavior. Only CMD_FLUSH semantics
         * change in FIX4, to isolate the effect of event lifetime. */
        abort_waiters(&u->reads);
        abort_waiters(&u->orphans);
        abort_waiters(&u->events);
        Disable();
        vpn_ring_clear(&u->pending);
        Enable();
        break;

    case AOVPN_TRACE_CMD: {
        struct AovpnTraceDump *dump = (struct AovpnTraceDump *)req->ios2_StatData;
        struct AovpnTraceState *t = &u->trace;
        ULONG k, first;
        if (!dump || req->ios2_DataLength < sizeof(*dump)) {
            io->io_Error = S2ERR_BAD_ARGUMENT;
            break;
        }
        Disable();
        dump->magic = AOVPN_TRACE_MAGIC;
        dump->version = AOVPN_TRACE_VERSION;
        dump->size = sizeof(*dump);
        dump->unit = u == &d->units[0] ? 0 : 1;
        dump->count = t->count;
        dump->next_seq = t->next_seq;
        first = (t->next_seq - t->count) % AOVPN_TRACE_CAP;
        for (k = 0; k < t->count; ++k)
            CopyMem(&t->entries[(first + k) % AOVPN_TRACE_CAP],
                    &dump->entries[k], sizeof(dump->entries[k]));
        Enable();
        break;
    }

    case AOVPN_DIAG_CMD: {
        struct AovpnDiag snapshot;
        if (!req->ios2_StatData || req->ios2_DataLength < sizeof(snapshot)) {
            io->io_Error = S2ERR_BAD_ARGUMENT;
            break;
        }
        Disable();
        CopyMem(&u->diag, &snapshot, sizeof(snapshot));
        snapshot.magic = AOVPN_DIAG_MAGIC;
        snapshot.version = AOVPN_DIAG_VERSION;
        snapshot.size = sizeof(snapshot);
        snapshot.unit = u == &d->units[0] ? 0 : 1;
        snapshot.online = u->online;
        snapshot.open_count = u->exec_unit.unit_OpenCnt;
        snapshot.fifo_count = u->pending.count;
        Enable();
        CopyMem(&snapshot, req->ios2_StatData, sizeof(snapshot));
        break;
    }

    case S2_DEVICEQUERY: {
        struct Sana2DeviceQuery *q = (struct Sana2DeviceQuery *)req->ios2_StatData;
        if (!q || q->SizeAvailable < 8) {
            io->io_Error = S2ERR_BAD_ARGUMENT;
            break;
        }
        avail = q->SizeAvailable;
#define VPN_FITS(field) (avail >= (ULONG)((UBYTE *)&q->field - (UBYTE *)q) + sizeof(q->field))
        if (VPN_FITS(DevQueryFormat)) q->DevQueryFormat = 0;
        if (VPN_FITS(DeviceLevel)) q->DeviceLevel = 0;
        if (VPN_FITS(AddrFieldSize)) q->AddrFieldSize = 32;
        if (VPN_FITS(MTU)) q->MTU = VPN_MTU;
        if (VPN_FITS(BPS)) q->BPS = VPN_BPS;
        if (VPN_FITS(HardwareType)) q->HardwareType = S2WireType_PPP;
#undef VPN_FITS
        q->SizeSupplied = avail < sizeof(*q) ? avail : sizeof(*q);
        q->SizeAvailable = sizeof(*q);
        break;
    }

    case S2_GETSTATIONADDRESS:
        for (i = 0; i < SANA2_MAX_ADDR_BYTES; ++i) {
            req->ios2_SrcAddr[i] = i < 4 ? u->station[i] : 0;
            req->ios2_DstAddr[i] = i < 4 ? u->factory[i] : 0;
        }
        break;

    case S2_CONFIGINTERFACE:
        if (u->configured) {
            io->io_Error = S2ERR_BAD_STATE;
            req->ios2_WireError = S2WERR_IS_CONFIGURED;
        } else {
            for (i = 0; i < 4; ++i) u->station[i] = req->ios2_SrcAddr[i];
            u->configured = TRUE;
            u->stats.Reconfigurations++;
        }
        break;

    case S2_ONLINE:
    case S2_OFFLINE: {
        UBYTE new_state = io->io_Command == S2_ONLINE ? TRUE : FALSE;
        if (new_state != u->online) {
            u->online = new_state;
            if (!new_state) {
                abort_waiters(&u->reads);
                abort_waiters(&u->orphans);
                vpn_ring_clear(&u->pending);
            }
            fire_event(u, new_state ? S2EVENT_ONLINE : S2EVENT_OFFLINE);
        }
        break;
    }

    case S2_ONEVENT:
        if (!(event_mask & (S2EVENT_ONLINE | S2EVENT_OFFLINE))) {
            io->io_Error = S2ERR_NOT_SUPPORTED;
            break;
        }
        if (event_mask & (u->online ? S2EVENT_ONLINE : S2EVENT_OFFLINE)) {
            req->ios2_WireError = u->online ? S2EVENT_ONLINE : S2EVENT_OFFLINE;
        } else {
            req->ios2_WireError = event_mask;
            io->io_Flags &= ~IOF_QUICK;
            safe_add_tail(&u->events, (struct Node *)io);
            if (trace_enabled) trace_event(u, AOVPN_T_EVENT_QUEUED, io);
            return;
        }
        break;

    case S2_TRACKTYPE:
        if (!ipv4_tracking_type(req->ios2_PacketType)) {
            io->io_Error = S2ERR_NOT_SUPPORTED;
        } else if (u->tracked) {
            io->io_Error = S2ERR_BAD_STATE;
            req->ios2_WireError = S2WERR_ALREADY_TRACKED;
        } else {
            u->tracked = TRUE;
        }
        break;

    case S2_UNTRACKTYPE:
        if (!ipv4_tracking_type(req->ios2_PacketType) || !u->tracked) {
            io->io_Error = S2ERR_BAD_STATE;
            req->ios2_WireError = S2WERR_NOT_TRACKED;
        } else {
            u->tracked = FALSE;
        }
        break;

    case S2_GETGLOBALSTATS:
        if (!req->ios2_StatData) io->io_Error = S2ERR_BAD_ARGUMENT;
        else CopyMem(&u->stats, req->ios2_StatData, sizeof(u->stats));
        break;

    case S2_GETTYPESTATS:
        if (!ipv4_tracking_type(req->ios2_PacketType) || !req->ios2_StatData)
            io->io_Error = S2ERR_BAD_ARGUMENT;
        else CopyMem(&u->type_stats, req->ios2_StatData, sizeof(u->type_stats));
        break;

    case S2_GETSPECIALSTATS:
        if (!req->ios2_StatData) io->io_Error = S2ERR_BAD_ARGUMENT;
        else ((struct Sana2SpecialStatHeader *)req->ios2_StatData)->RecordCountSupplied = 0;
        break;

    case CMD_CLEAR:
        /* Avoid struct assignment, which can emit libc memcpy in -nostdlib. */
        { UBYTE *s = (UBYTE *)&u->stats, *t = (UBYTE *)&u->type_stats;
          for (i = 0; i < sizeof(u->stats); ++i) s[i] = 0;
          for (i = 0; i < sizeof(u->type_stats); ++i) t[i] = 0; }
        break;

    case CMD_START:
    case CMD_STOP:
        break;

    default:
        io->io_Error = IOERR_NOCMD;
        break;
    }
    if (trace_enabled) trace_event(u, AOVPN_T_DONE, io);
    answer(io);
}

static ULONG __attribute__((used))
vpn_abortio(struct Library *lib asm("a6"), struct IORequest *io asm("a1"))
{
    struct VpnDevice *d = (struct VpnDevice *)lib;
    struct VpnUnit *u = (struct VpnUnit *)io->io_Unit;
    if (u != &d->units[0] && u != &d->units[1]) return (ULONG)-1;
    trace_event(u, AOVPN_T_ABORT_IN, io);
    if (safe_unlink_if_queued(&u->reads, (struct Node *)io) ||
        safe_unlink_if_queued(&u->orphans, (struct Node *)io) ||
        safe_unlink_if_queued(&u->events, (struct Node *)io)) {
        io->io_Error = IOERR_ABORTED;
        trace_event(u, AOVPN_T_ABORT_OK, io);
        ReplyMsg(&io->io_Message);
        return 0;
    }
    trace_event(u, AOVPN_T_ABORT_MISS, io);
    return (ULONG)-1;
}

static ULONG vpn_vectors[] = {
    (ULONG)vpn_open, (ULONG)vpn_close, (ULONG)vpn_expunge,
    0, (ULONG)vpn_beginio, (ULONG)vpn_abortio, (ULONG)-1
};

const ULONG vpn_autoinit[4] = {
    sizeof(struct VpnDevice), (ULONG)vpn_vectors, 0, (ULONG)vpn_init
};

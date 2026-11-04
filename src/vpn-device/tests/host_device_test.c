/* Host functional test compiling the actual vpn.device source, with Exec
 * API in-memory mocks. This validates dispatch and queue behavior, NOT ABI. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define VPN_HOST_MODEL 1
#include "../src/vpn_device.c"

static unsigned g_replies, g_disable_nest;
void Disable(void) { ++g_disable_nest; }
void Enable(void) { assert(g_disable_nest); --g_disable_nest; }
void AddTail(struct List *l, struct Node *n)
{
    struct Node *pred = l->lh_TailPred;
    n->ln_Succ = (struct Node *)&l->lh_Tail;
    n->ln_Pred = pred;
    pred->ln_Succ = n;
    l->lh_TailPred = n;
}
struct Node *RemHead(struct List *l)
{
    struct Node *n = l->lh_Head;
    if (!n->ln_Succ) return NULL;
    Remove(n);
    return n;
}
void Remove(struct Node *n)
{
    n->ln_Pred->ln_Succ = n->ln_Succ;
    n->ln_Succ->ln_Pred = n->ln_Pred;
    n->ln_Succ = n->ln_Pred = NULL;
}
void CopyMem(const void *src, void *dst, ULONG len) { memcpy(dst,src,(size_t)len); }
/* Reflect the Exec state transition used by CheckIO()/WaitIO().
 * Previous mocks only incremented g_replies, masking missing NT_MESSAGE. */
void ReplyMsg(struct Message *m)
{
    assert(m->mn_Node.ln_Type == NT_MESSAGE);
    m->mn_Node.ln_Type = NT_REPLYMSG;
    ++g_replies;
}
static int checkio_model(const struct IORequest *io)
{
    return io->io_Message.mn_Node.ln_Type == NT_REPLYMSG;
}
APTR AllocMem(ULONG n, ULONG flags) { return flags ? calloc(1,(size_t)n):malloc((size_t)n); }
void FreeMem(APTR p, ULONG n) { (void)n; free(p); }

static struct VpnDevice d;
static struct IOSana2Req io0,io1,rd,wr;
static void open_unit(struct IOSana2Req *io, int index)
{
    memset(io,0,sizeof(*io));
    vpn_open(&d.device.dd_Library, &io->ios2_Req, (ULONG)index, 0);
    assert(io->ios2_Req.io_Error == 0);
    assert(io->ios2_BufferManagement != NULL);
}
static void submit(struct IOSana2Req *io)
{
    vpn_beginio(&d.device.dd_Library, &io->ios2_Req);
    assert(g_disable_nest == 0);
}
static void init_request(struct IOSana2Req *r, struct IOSana2Req *op, UWORD cmd)
{
    memset(r,0,sizeof(*r));
    r->ios2_Req.io_Device = op->ios2_Req.io_Device;
    r->ios2_Req.io_Unit = op->ios2_Req.io_Unit;
    r->ios2_Req.io_Command = cmd;
    r->ios2_BufferManagement = op->ios2_BufferManagement;
}
static void set_online(struct IOSana2Req *op)
{
    struct IOSana2Req r;
    init_request(&r,op,S2_ONLINE);
    submit(&r);
    assert(!r.ios2_Req.io_Error);
}
static void write_packet(struct IOSana2Req *op, UBYTE *payload, ULONG len)
{
    init_request(&wr,op,CMD_WRITE);
    wr.ios2_Data = payload;
    wr.ios2_DataLength = len;
    wr.ios2_PacketType = VPN_IPV4_PACKET_TYPE;
    submit(&wr);
}
static void read_packet(struct IOSana2Req *op, UBYTE *payload)
{
    init_request(&rd,op,CMD_READ);
    rd.ios2_Data = payload;
    rd.ios2_PacketType = VPN_IPV4_PACKET_TYPE;
    rd.ios2_DataLength = VPN_MTU;
    submit(&rd);
}
int main(void)
{
    UBYTE in[64]={0},out[64]={0};
    ULONG i;
    unsigned pre;
    struct IOSana2Req q;
    struct Sana2DeviceQuery dq;
    vpn_init(0,&d.device.dd_Library);
    open_unit(&io0,0);
    open_unit(&io1,1);
    set_online(&io0);
    set_online(&io1);
    init_request(&q,&io0,S2_DEVICEQUERY);
    dq.SizeAvailable=sizeof(dq);
    q.ios2_StatData=&dq;
    submit(&q);
    assert(q.ios2_Req.io_Error == 0 && dq.MTU==VPN_MTU && dq.HardwareType==S2WireType_PPP);
    out[0]=0x45; out[3]=0x42;

    pre=g_replies;
    read_packet(&io1,in);
    assert(g_replies==pre); /* pending, must not reply yet */
    write_packet(&io0,out,64);
    assert(!wr.ios2_Req.io_Error);
    assert(g_replies==pre+2); /* read reply + write reply */
    assert(memcmp(in,out,64)==0 && rd.ios2_DataLength==64);
    puts("PASS: async unit0->unit1 IPv4 packet");

    memset(in,0,sizeof(in));
    write_packet(&io1,out,64);
    assert(!wr.ios2_Req.io_Error);
    assert(d.units[0].pending.count==1);
    read_packet(&io0,in);
    assert(!rd.ios2_Req.io_Error && !d.units[0].pending.count);
    assert(memcmp(in,out,64)==0);
    puts("PASS: write-before-read unit1->unit0 buffering");

    /* PPP IPv4 0x001f and legacy 0x0800 must interoperate. */
    init_request(&rd,&io1,CMD_READ);
    rd.ios2_Data = in;
    rd.ios2_DataLength = sizeof(in);
    rd.ios2_PacketType = VPN_IPV4_PACKET_TYPE;
    submit(&rd);
    init_request(&wr,&io0,CMD_WRITE);
    wr.ios2_Data = out;
    wr.ios2_DataLength = sizeof(out);
    wr.ios2_PacketType = VPN_PPP_IPV4_PACKET_TYPE;
    submit(&wr);
    assert(!wr.ios2_Req.io_Error && !rd.ios2_Req.io_Error);
    assert(rd.ios2_PacketType == VPN_IPV4_PACKET_TYPE);
    assert(memcmp(in,out,sizeof(out)) == 0);
    puts("PASS: PPP 0x001f write -> legacy 0x0800 read");

    init_request(&rd,&io0,CMD_READ);
    rd.ios2_Data = in;
    rd.ios2_DataLength = sizeof(in);
    rd.ios2_PacketType = VPN_PPP_IPV4_PACKET_TYPE;
    submit(&rd);
    init_request(&wr,&io1,CMD_WRITE);
    wr.ios2_Data = out;
    wr.ios2_DataLength = sizeof(out);
    wr.ios2_PacketType = VPN_IPV4_PACKET_TYPE;
    submit(&wr);
    assert(!wr.ios2_Req.io_Error && !rd.ios2_Req.io_Error);
    assert(rd.ios2_PacketType == VPN_PPP_IPV4_PACKET_TYPE);
    assert(memcmp(in,out,sizeof(out)) == 0);
    puts("PASS: legacy 0x0800 write -> PPP 0x001f read");


    /* MiamiDX uses the RFC 1662 PPP IPv4 protocol value 0x0021. */
    init_request(&rd,&io1,CMD_READ);
    rd.ios2_Data = in;
    rd.ios2_DataLength = sizeof(in);
    rd.ios2_PacketType = VPN_IPV4_PACKET_TYPE;
    submit(&rd);
    init_request(&wr,&io0,CMD_WRITE);
    wr.ios2_Data = out;
    wr.ios2_DataLength = sizeof(out);
    wr.ios2_PacketType = VPN_PPP_IP_PROTOCOL;
    submit(&wr);
    assert(!wr.ios2_Req.io_Error && !rd.ios2_Req.io_Error);
    assert(d.units[0].diag.last_write_type == VPN_PPP_IP_PROTOCOL);
    assert(memcmp(in,out,sizeof(out)) == 0);
    puts("PASS: MiamiDX PPP 0x0021 write -> peer 0x0800 read");

    /* FIX3D: the peer's reply can complete a MiamiDX 0x0021 read.
     * Simulate an IORequest reused after a previous ReplyMsg: BeginIO must
     * reset NT_REPLYMSG -> NT_MESSAGE, and completion must restore it. */
    pre = g_replies;
    init_request(&rd, &io0, CMD_READ);
    rd.ios2_Data = in;
    rd.ios2_DataLength = sizeof(in);
    rd.ios2_PacketType = VPN_PPP_IP_PROTOCOL;
    rd.ios2_Req.io_Message.mn_Node.ln_Type = NT_REPLYMSG;
    submit(&rd);
    assert(rd.ios2_Req.io_Error == 0);
    assert(rd.ios2_Req.io_Message.mn_Node.ln_Type == NT_MESSAGE);
    assert(!checkio_model(&rd.ios2_Req));
    assert(g_replies == pre);
    assert(d.units[0].diag.read_queued > 0);
    init_request(&wr, &io1, CMD_WRITE);
    wr.ios2_Data = out;
    wr.ios2_DataLength = sizeof(out);
    wr.ios2_PacketType = VPN_IPV4_PACKET_TYPE;
    submit(&wr);
    assert(!wr.ios2_Req.io_Error && !rd.ios2_Req.io_Error);
    assert(checkio_model(&rd.ios2_Req));
    assert(g_replies == pre + 2);
    assert(d.units[0].pending.count == 0);
    assert(memcmp(in,out,sizeof(out)) == 0);
    puts("PASS: FIX3D MiamiDX 0x0021 READ queued NT_MESSAGE -> delivered NT_REPLYMSG");

    /* FIX3D: 0x0021 remains data-plane-only; tracking/statistics preserve
     * their FIX2 semantics as in FIX3B and FIX3C. */
    {
        struct Sana2PacketTypeStats ts;
        assert(d.units[0].tracked == FALSE);
        init_request(&q, &io0, S2_TRACKTYPE);
        q.ios2_PacketType = VPN_PPP_IP_PROTOCOL;
        submit(&q);
        assert(q.ios2_Req.io_Error == S2ERR_NOT_SUPPORTED);
        assert(d.units[0].tracked == FALSE);

        init_request(&q, &io0, S2_GETTYPESTATS);
        q.ios2_PacketType = VPN_PPP_IP_PROTOCOL;
        q.ios2_StatData = &ts;
        submit(&q);
        assert(q.ios2_Req.io_Error == S2ERR_BAD_ARGUMENT);

        init_request(&q, &io0, S2_UNTRACKTYPE);
        q.ios2_PacketType = VPN_PPP_IP_PROTOCOL;
        submit(&q);
        assert(q.ios2_Req.io_Error == S2ERR_BAD_STATE);
        assert(q.ios2_WireError == S2WERR_NOT_TRACKED);

        /* Pre-existing 0x0800 tracking path remains available. */
        init_request(&q, &io0, S2_TRACKTYPE);
        q.ios2_PacketType = VPN_IPV4_PACKET_TYPE;
        submit(&q);
        assert(q.ios2_Req.io_Error == 0 && d.units[0].tracked);
        init_request(&q, &io0, S2_GETTYPESTATS);
        q.ios2_PacketType = VPN_IPV4_PACKET_TYPE;
        q.ios2_StatData = &ts;
        submit(&q);
        assert(q.ios2_Req.io_Error == 0);
        init_request(&q, &io0, S2_UNTRACKTYPE);
        q.ios2_PacketType = VPN_IPV4_PACKET_TYPE;
        submit(&q);
        assert(q.ios2_Req.io_Error == 0 && !d.units[0].tracked);
        puts("PASS: FIX3D control plane preserves FIX2 TRACK/UNTRACK/GETTYPESTATS");
    }

    pre=g_replies;
    read_packet(&io1,in);
    assert(g_replies==pre);
    vpn_abortio(&d.device.dd_Library, &rd.ios2_Req);
    assert(g_replies==pre+1 && rd.ios2_Req.io_Error==IOERR_ABORTED);
    assert(!d.units[1].reads.lh_Head->ln_Succ);
    puts("PASS: AbortIO pending read, single reply");

    for (i=0;i<VPN_RING_CAPACITY;i++) {
        write_packet(&io0,out,64);
        assert(!wr.ios2_Req.io_Error);
    }
    assert(d.units[1].pending.count==VPN_RING_CAPACITY);
    write_packet(&io0,out,64);
    assert(wr.ios2_Req.io_Error==S2ERR_NO_RESOURCES && d.units[1].stats.Overruns==1);
    init_request(&q,&io1,CMD_FLUSH);
    submit(&q);
    assert(!d.units[1].pending.count);
    puts("PASS: FIFO capacity 16 / overrun / flush");

    /* Private diagnostics snapshot is read-only and does not disturb queues. */
    {
        struct AovpnDiag snap;
        struct IOSana2Req di;
        init_request(&di, &io0, AOVPN_DIAG_CMD);
        di.ios2_StatData = &snap;
        di.ios2_DataLength = sizeof(snap);
        submit(&di);
        assert(!di.ios2_Req.io_Error);
        assert(snap.magic == AOVPN_DIAG_MAGIC && snap.version == AOVPN_DIAG_VERSION);
        assert(snap.unit == 0 && snap.online == 1 && snap.open_count == 1);
        assert(snap.write_received == 20 && snap.write_accepted == 19 && snap.write_rejected == 1);
        assert(snap.reject_ring == 1 && snap.fifo_count == 0);
        assert(snap.read_completed == 3 && snap.fifo_delivered == 3);
        init_request(&di, &io1, AOVPN_DIAG_CMD);
        di.ios2_StatData = &snap;
        di.ios2_DataLength = sizeof(snap);
        submit(&di);
        assert(!di.ios2_Req.io_Error && snap.unit == 1);
        assert(snap.read_submitted == 4 && snap.read_completed == 3);
        puts("PASS: read-only live diagnostics / unit counters / ring rejection");
    }

    /* FIX3D: MiamiDX-style burst of 15 pending reads. Make their initial
     * type NT_REPLYMSG to emulate reused completed IORequests; otherwise
     * the pre-FIX3D bug would remain invisible to this host-model test. */
    {
        struct IOSana2Req pending[15];
        UBYTE sink[15][64];
        struct IOSana2Req off;
        ULONG queued_before = d.units[0].diag.read_queued;
        unsigned k;
        pre = g_replies;
        for (k = 0; k < 15; ++k) {
            init_request(&pending[k], &io0, CMD_READ);
            pending[k].ios2_PacketType = VPN_PPP_IP_PROTOCOL;
            pending[k].ios2_Data = sink[k];
            pending[k].ios2_DataLength = sizeof(sink[k]);
            pending[k].ios2_Req.io_Message.mn_Node.ln_Type = NT_REPLYMSG;
            submit(&pending[k]);
            assert(g_replies == pre);
            assert(pending[k].ios2_Req.io_Error == 0);
            assert(!checkio_model(&pending[k].ios2_Req));
            assert(!(pending[k].ios2_Req.io_Flags & IOF_QUICK));
        }
        assert(d.units[0].diag.read_queued == queued_before + 15);
        init_request(&off, &io0, S2_OFFLINE);
        submit(&off);
        assert(off.ios2_Req.io_Error == 0);
        assert(g_replies == pre + 16); /* 15 aborts + S2_OFFLINE */
        for (k = 0; k < 15; ++k) {
            assert(pending[k].ios2_Req.io_Error == IOERR_ABORTED);
            assert(checkio_model(&pending[k].ios2_Req));
        }
        assert(!d.units[0].reads.lh_Head->ln_Succ);
        set_online(&io0);

        /* Reuse one just-completed request and confirm it becomes pending
         * again rather than reporting a stale completion to CheckIO(). */
        pre = g_replies;
        pending[0].ios2_Req.io_Error = 0;
        submit(&pending[0]);
        assert(g_replies == pre);
        assert(!checkio_model(&pending[0].ios2_Req));
        init_request(&wr, &io1, CMD_WRITE);
        wr.ios2_Data = out;
        wr.ios2_DataLength = sizeof(out);
        wr.ios2_PacketType = VPN_IPV4_PACKET_TYPE;
        submit(&wr);
        assert(g_replies == pre + 2);
        assert(checkio_model(&pending[0].ios2_Req));
        assert(pending[0].ios2_Req.io_Error == 0);
        puts("PASS: FIX3D 15 queued MiamiDX reads, offline replies, reuse transitions");
    }

    /* TRACE1: inspect a chronological read-only snapshot of production
     * events. This is the new diagnostic used for the MiamiDX DOWN case. */
    {
        static struct AovpnTraceDump dump;
        struct IOSana2Req di;
        ULONG k;
        BOOL found_pending = FALSE, found_offline = FALSE;
        init_request(&di, &io0, AOVPN_TRACE_CMD);
        di.ios2_StatData = &dump;
        di.ios2_DataLength = sizeof(dump);
        pre = g_replies;
        submit(&di);
        assert(!di.ios2_Req.io_Error);
        assert(dump.magic == AOVPN_TRACE_MAGIC);
        assert(dump.version == AOVPN_TRACE_VERSION);
        assert(dump.size == sizeof(dump));
        assert(dump.unit == 0 && dump.count > 0 && dump.count <= AOVPN_TRACE_CAP);
        assert(g_replies == pre + 1);
        for (k=0; k<dump.count; ++k) {
            if (dump.entries[k].event == AOVPN_T_READ_QUEUED &&
                dump.entries[k].command == CMD_READ &&
                dump.entries[k].type == VPN_PPP_IP_PROTOCOL)
                found_pending = TRUE;
            if (dump.entries[k].event == AOVPN_T_BEGIN &&
                dump.entries[k].command == S2_OFFLINE)
                found_offline = TRUE;
            if (k > 0) assert(dump.entries[k].seq > dump.entries[k-1].seq);
        }
        assert(found_pending && found_offline);
        puts("PASS: TRACE1 chronological events, queued 0x0021 reads, S2_OFFLINE");
    }

    /* FIX4: MiamiDX leaves an OFFLINE event request pending while flushing
     * its receive queue. CMD_FLUSH must NOT cancel the S2_ONEVENT waiter.
     * S2_OFFLINE must subsequently complete that same event exactly once. */
    {
        struct IOSana2Req ev, fl, off;
        struct IOSana2Req pending[15];
        UBYTE sinks[15][64];
        unsigned k;
        pre = g_replies;
        init_request(&ev, &io0, S2_ONEVENT);
        ev.ios2_WireError = S2EVENT_OFFLINE;
        submit(&ev);
        assert(g_replies == pre);
        assert(ev.ios2_Req.io_Message.mn_Node.ln_Type == NT_MESSAGE);
        assert(d.units[0].events.lh_Head->ln_Succ);
        for (k=0; k<15; ++k) {
            init_request(&pending[k], &io0, CMD_READ);
            pending[k].ios2_PacketType = VPN_PPP_IP_PROTOCOL;
            pending[k].ios2_Data = sinks[k];
            pending[k].ios2_DataLength = sizeof(sinks[k]);
            submit(&pending[k]);
            assert(pending[k].ios2_Req.io_Error == 0);
            assert(!checkio_model(&pending[k].ios2_Req));
        }
        init_request(&fl, &io0, CMD_FLUSH);
        submit(&fl);
        assert(fl.ios2_Req.io_Error == 0);
        assert(g_replies == pre + 16); /* 15 data READs + FLUSH */
        assert(d.units[0].events.lh_Head->ln_Succ);
        assert(!checkio_model(&ev.ios2_Req));
        for (k=0; k<15; ++k) {
            assert(pending[k].ios2_Req.io_Error == IOERR_ABORTED);
            assert(checkio_model(&pending[k].ios2_Req));
        }
        init_request(&off, &io0, S2_OFFLINE);
        submit(&off);
        assert(off.ios2_Req.io_Error == 0);
        assert(g_replies == pre + 18); /* event + OFFLINE */
        assert(ev.ios2_Req.io_Error == 0);
        assert(ev.ios2_WireError == S2EVENT_OFFLINE);
        assert(checkio_model(&ev.ios2_Req));
        assert(!d.units[0].events.lh_Head->ln_Succ);
        set_online(&io0);
        puts("PASS: FIX4 15 RX requests + CMD_FLUSH preserves ONEVENT until OFFLINE");
    }

    /* Full data path, not read-disabled: all 15 independent MiamiDX-style
     * PPP IPv4 reads must receive real data written from the peer unit. */
    {
        struct IOSana2Req pending[15], each_write;
        UBYTE sink[15][64], packet[64];
        unsigned k;
        for (k=0; k<64; ++k) packet[k] = (UBYTE)(k+13);
        pre = g_replies;
        for (k=0; k<15; ++k) {
            init_request(&pending[k], &io0, CMD_READ);
            pending[k].ios2_PacketType = VPN_PPP_IP_PROTOCOL;
            pending[k].ios2_Data = sink[k];
            pending[k].ios2_DataLength = sizeof(sink[k]);
            submit(&pending[k]);
            assert(!checkio_model(&pending[k].ios2_Req));
        }
        assert(g_replies == pre);
        for (k=0; k<15; ++k) {
            packet[10] = (UBYTE)k;
            init_request(&each_write, &io1, CMD_WRITE);
            each_write.ios2_Data = packet;
            each_write.ios2_DataLength = sizeof(packet);
            each_write.ios2_PacketType = VPN_IPV4_PACKET_TYPE;
            submit(&each_write);
            assert(each_write.ios2_Req.io_Error == 0);
            assert(checkio_model(&pending[k].ios2_Req));
            assert(pending[k].ios2_Req.io_Error == 0);
            assert(pending[k].ios2_DataLength == sizeof(packet));
            assert(sink[k][10] == k);
        }
        assert(d.units[0].pending.count == 0);
        assert(!d.units[0].reads.lh_Head->ln_Succ);
        puts("PASS: FIX4 15 real PPP 0x0021 reads completed by peer IPv4 writes");
    }

    /* Closing a unit with pending read replies IOERR_ABORTED, not UAF. */
    read_packet(&io1,in);
    pre=g_replies;
    vpn_close(&d.device.dd_Library,&io1.ios2_Req);
    assert(g_replies==pre+1 && rd.ios2_Req.io_Error==IOERR_ABORTED);
    write_packet(&io0,out,64);
    assert(wr.ios2_Req.io_Error==S2ERR_OUTOFSERVICE);
    vpn_close(&d.device.dd_Library,&io0.ios2_Req);
    assert(d.device.dd_Library.lib_OpenCnt==0 && !g_disable_nest);
    puts("PASS: close aborts pending requests / peer becomes unavailable");
    puts("PASS: actual vpn.device source under host-model Exec API");
    return 0;
}

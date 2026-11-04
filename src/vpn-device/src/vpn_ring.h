/* SPDX-License-Identifier: BSD-2-Clause
 * vpn.device bounded datagram ring; same implementation in POC and RC.
 * No libc or OS dependencies; built on both host and m68k.
 */
#ifndef AOVPN_RING_H
#define AOVPN_RING_H

#define VPN_MTU            1400U
#define VPN_RING_CAPACITY    16U
#define VPN_IPV4_PACKET_TYPE 0x0800U
#define VPN_PPP_IPV4_PACKET_TYPE 0x001FU
/* PPP protocol number for IPv4 (RFC 1662); observed from MiamiDX. */
#define VPN_PPP_IP_PROTOCOL 0x0021U

struct VpnFrame {
    unsigned short length;
    unsigned short packet_type;
    unsigned char src[4];
    unsigned char dst[4];
    unsigned char payload[VPN_MTU];
};

struct VpnRing {
    unsigned short head;
    unsigned short count;
    struct VpnFrame frame[VPN_RING_CAPACITY];
};

/* These do not copy data. The caller serializes reserve/pop and payload
 * access; in vpn.device this is one Disable/Enable transaction. */
static struct VpnFrame *vpn_ring_reserve(struct VpnRing *r)
{
    unsigned int pos;
    if (r->count >= VPN_RING_CAPACITY)
        return (struct VpnFrame *)0;
    pos = ((unsigned int)r->head + r->count) % VPN_RING_CAPACITY;
    r->count++;
    return &r->frame[pos];
}

static struct VpnFrame *vpn_ring_peek(struct VpnRing *r)
{
    if (!r->count)
        return (struct VpnFrame *)0;
    return &r->frame[r->head];
}

static void vpn_ring_pop(struct VpnRing *r)
{
    if (r->count) {
        r->head = (unsigned short)((r->head + 1) % VPN_RING_CAPACITY);
        r->count--;
    }
}

static void vpn_ring_clear(struct VpnRing *r)
{
    r->head = 0;
    r->count = 0;
}
#endif

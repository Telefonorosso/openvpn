#include <stdio.h>
#include <assert.h>
#include "../src/vpn_ring.h"

int main(void)
{
    static struct VpnRing r;
    unsigned i;
    struct VpnFrame *p;
    for (i = 0; i < VPN_RING_CAPACITY; i++) {
        p = vpn_ring_reserve(&r);
        assert(p != NULL);
        p->length = 2;
        p->packet_type = VPN_IPV4_PACKET_TYPE;
        p->payload[0] = (unsigned char)i;
    }
    assert(!vpn_ring_reserve(&r));
    for (i = 0; i < VPN_RING_CAPACITY/2; i++) {
        p = vpn_ring_peek(&r);
        assert(p && p->payload[0] == i);
        vpn_ring_pop(&r);
    }
    for (i = VPN_RING_CAPACITY; i < VPN_RING_CAPACITY+VPN_RING_CAPACITY/2; i++) {
        p = vpn_ring_reserve(&r);
        assert(p);
        p->payload[0] = (unsigned char)i;
    }
    for (i = VPN_RING_CAPACITY/2; i < VPN_RING_CAPACITY+VPN_RING_CAPACITY/2; i++) {
        p = vpn_ring_peek(&r);
        assert(p && p->payload[0] == i);
        vpn_ring_pop(&r);
    }
    assert(!vpn_ring_peek(&r));
    vpn_ring_pop(&r);
    assert(!r.count);
    p = vpn_ring_reserve(&r);
    assert(p);
    vpn_ring_clear(&r);
    assert(!r.count);
    puts("PASS: bounded FIFO, full, wrap-around, empty, reset");
    return 0;
}

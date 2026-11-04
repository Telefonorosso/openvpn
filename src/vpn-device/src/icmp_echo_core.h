/* SPDX-License-Identifier: BSD-2-Clause
 * IPv4 ICMP echo responder packet core for the MiamiDX integration probe.
 * No AmigaOS / device dependencies. Not part of the vpn.device kernel.
 */
#ifndef AOVPN_ICMP_ECHO_CORE_H
#define AOVPN_ICMP_ECHO_CORE_H
#include <stddef.h>

static unsigned long aovpn_sum16(const unsigned char *data, unsigned int n)
{
    unsigned long s = 0;
    unsigned int i;
    for (i = 0; i + 1 < n; i += 2)
        s += ((unsigned long)data[i] << 8) | data[i + 1];
    if (n & 1U) s += ((unsigned long)data[n - 1] << 8);
    while (s >> 16) s = (s & 0xffffUL) + (s >> 16);
    return s;
}
static unsigned short aovpn_checksum(const unsigned char *data, unsigned int n)
{
    return (unsigned short)(~aovpn_sum16(data, n) & 0xffffU);
}
static void aovpn_set_checksum(unsigned char *p, unsigned int n, unsigned int off)
{
    unsigned short c;
    p[off] = 0; p[off + 1] = 0;
    c = aovpn_checksum(p, n);
    p[off] = (unsigned char)(c >> 8);
    p[off + 1] = (unsigned char)c;
}
/* Return 1 only after replacing a verified ICMP Echo Request with reply.
 * Packet length must equal IPv4 total length. No fragments. Only respond to
 * 10.77.0.1 -> 10.77.0.2, to avoid replying to unrelated traffic.
 */
static int aovpn_make_icmp_reply(unsigned char *ip, unsigned int n)
{
    unsigned int h, total, frag, ilen, i;
    unsigned char t;
    if (!ip || n < 28 || (ip[0] >> 4) != 4) return 0;
    h = (ip[0] & 15U) * 4U;
    if (h < 20 || h > 60 || n < h + 8) return 0;
    total = ((unsigned int)ip[2] << 8) | ip[3];
    if (total != n) return 0;
    frag = ((unsigned int)ip[6] << 8) | ip[7];
    if (frag & 0x3fffU) return 0; /* reject MF and any offset */
    if (ip[9] != 1 || aovpn_checksum(ip, h) != 0) return 0;
    if (ip[12] != 10 || ip[13] != 77 || ip[14] != 0 || ip[15] != 1 ||
        ip[16] != 10 || ip[17] != 77 || ip[18] != 0 || ip[19] != 2)
        return 0;
    ilen = n - h;
    if (ip[h] != 8 || ip[h + 1] != 0 || aovpn_checksum(ip + h, ilen) != 0)
        return 0;
    ip[h] = 0;
    aovpn_set_checksum(ip + h, ilen, 2);
    for (i = 0; i < 4; ++i) {
        t = ip[12 + i]; ip[12 + i] = ip[16 + i]; ip[16 + i] = t;
    }
    ip[8] = 64; /* TTL */
    aovpn_set_checksum(ip, h, 10);
    return 1;
}
#endif

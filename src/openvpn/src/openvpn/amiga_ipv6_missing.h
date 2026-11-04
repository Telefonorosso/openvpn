/* AmiOpenVPN 2.6.23 / AmigaOS IPv4-only build-time helpers.
 * These two predicates are pure address-format checks. They do NOT enable
 * the IPv6 transport or assume struct in6_addr's field names.
 * GPL-2.0-only, consistent with the OpenVPN integration.
 */
#ifndef AOVPN_AMIGA_IPV6_MISSING_H
#define AOVPN_AMIGA_IPV6_MISSING_H
#ifdef TARGET_AMIGA
static inline int aovpn_addr_is_v4mapped(const void *a)
{
    const unsigned char *p = (const unsigned char *)a;
    int i;
    for (i = 0; i < 10; ++i) if (p[i] != 0) return 0;
    return p[10] == 0xff && p[11] == 0xff;
}
static inline int aovpn_addr_is_linklocal(const void *a)
{
    const unsigned char *p = (const unsigned char *)a;
    return p[0] == 0xfe && (p[1] & 0xc0) == 0x80;
}
#ifndef IN6_IS_ADDR_V4MAPPED
#define IN6_IS_ADDR_V4MAPPED(p) aovpn_addr_is_v4mapped((p))
#endif
#ifndef IN6_IS_ADDR_LINKLOCAL
#define IN6_IS_ADDR_LINKLOCAL(p) aovpn_addr_is_linklocal((p))
#endif
#endif /* TARGET_AMIGA */
#endif /* AOVPN_AMIGA_IPV6_MISSING_H */

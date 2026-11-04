/*
 * AmiOpenVPN POC6-GW1-FIX1: MiamiDX default IPv4 gateway lookup.
 *
 * Read-only PF_ROUTE / RTM_GET, with the v4 wire ABI observed on the
 * hardware-validated MiamiRoute and the GW1-DIAG3/4/5 probes.
 *
 * IMPORTANT: cross-SDK <net/route.h> provides an incompatible rt_msghdr
 * (version=3, sizeof=74 in our toolchain). MiamiDX uses version 4 with
 * a 76-byte outgoing header and 92-byte reply header; encode/decode
 * those fields explicitly rather than casting to the SDK structure.
 *
 * The route socket is a bsdsocket.library descriptor: use WaitSelect(),
 * send(), recv() and CloseSocket(), never POSIX close()/select().
 *
 * The only RGI result populated is the verified gateway IPv4 address.
 * Interface name/netmask on the gateway's local interface are unknown,
 * rather than guessed from the route's 0/0 destination netmask.
 */
#include <net/route.h>

#define AOVPN_GW_V4_VERSION 4U
#define AOVPN_GW_V4_REQUEST_LEN 132U
#define AOVPN_GW_V4_REPLY_HEADER 92U
#define AOVPN_GW_REPLY_MAX 1024U
#define AOVPN_GW_TIMEOUT_SECONDS 1

static unsigned int
aovpn_gw_be16(const unsigned char *p)
{
    return ((unsigned int)p[0] << 8) | (unsigned int)p[1];
}

static unsigned long
aovpn_gw_be32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16)
           | ((unsigned long)p[2] << 8) | (unsigned long)p[3];
}

static void
aovpn_gw_put16(unsigned char *p, unsigned int val)
{
    p[0] = (unsigned char)(val >> 8);
    p[1] = (unsigned char)val;
}

static void
aovpn_gw_put32(unsigned char *p, unsigned long val)
{
    p[0] = (unsigned char)(val >> 24);
    p[1] = (unsigned char)(val >> 16);
    p[2] = (unsigned char)(val >> 8);
    p[3] = (unsigned char)val;
}

/* BSD 4.4 routing sockaddr: sa_len at byte 0, sa_family at byte 1.
 * RT_ROUNDUP(0) advances by one 32-bit word on m68k. */
static size_t
aovpn_gw_sockaddr_step(const unsigned char *p, size_t available)
{
    size_t len, aligned;
    if (available < 4U)
    {
        return 0;
    }
    len = (size_t)p[0];
    if (len != 0U && (len < 2U || len > available))
    {
        return 0;
    }
    aligned = len ? ((len + 3U) & ~3U) : 4U;
    return aligned <= available ? aligned : 0;
}

static bool
aovpn_gw_ipv4_sockaddr(const unsigned char *p, size_t step,
                       in_addr_t *address)
{
    if (step < 8U || p[0] < 8U || p[1] != (unsigned char)AF_INET)
    {
        return false;
    }
    *address = (in_addr_t)aovpn_gw_be32(p + 4);
    return true;
}

/* A default-route netmask can be encoded as a compressed sockaddr with
 * sa_len=0. Reject any nonzero mask bits, including uncompressed /8,
 * /24 or /32 masks; never confuse the route mask with LAN netmask. */
static bool
aovpn_gw_zero_mask(const unsigned char *p, size_t step)
{
    size_t i;
    if (p[0] == 0)
    {
        for (i = 1; i < 4U; ++i)
        {
            if (p[i] != 0)
            {
                return false;
            }
        }
        return true;
    }
    if (p[1] == (unsigned char)AF_INET && p[0] >= 8U)
    {
        in_addr_t mask;
        return aovpn_gw_ipv4_sockaddr(p, step, &mask) && mask == 0;
    }
    /* Compact AF_UNSPEC masks may contain only zero-filled bytes. */
    if (p[1] != 0)
    {
        return false;
    }
    for (i = 2; i < (size_t)p[0]; ++i)
    {
        if (p[i] != 0)
        {
            return false;
        }
    }
    return true;
}

/* Return true only for a confirmed 0.0.0.0/0 via a usable IPv4 router.
 * A zero result in an unrelated PF_ROUTE message must never be accepted. */
static bool
aovpn_gw_parse_reply(const unsigned char *data, size_t bytes,
                          unsigned long expected_seq, in_addr_t *out)
{
    unsigned int msg_len, type, addrs, flags;
    unsigned long seq, error;
    size_t offset;
    unsigned int bit;
    in_addr_t destination = 0, gateway = 0;
    bool have_destination = false, have_mask = false, have_gateway = false;

    if (bytes < AOVPN_GW_V4_REPLY_HEADER)
    {
        return false;
    }
    msg_len = aovpn_gw_be16(data);
    type = (unsigned int)data[3];
    flags = (unsigned int)aovpn_gw_be32(data + 8);
    addrs = (unsigned int)aovpn_gw_be32(data + 12);
    seq = aovpn_gw_be32(data + 20);
    error = aovpn_gw_be32(data + 24);
    if (msg_len < AOVPN_GW_V4_REPLY_HEADER || msg_len > bytes
        || data[2] != AOVPN_GW_V4_VERSION || type != RTM_GET
        || seq != expected_seq || error != 0
        || (flags & (RTF_UP | RTF_GATEWAY)) != (RTF_UP | RTF_GATEWAY)
        || (flags & RTF_HOST)
        || (addrs & (RTA_DST | RTA_GATEWAY | RTA_NETMASK))
           != (RTA_DST | RTA_GATEWAY | RTA_NETMASK)
        || (addrs & ~0xffffU) != 0U)
    {
        return false;
    }
#ifdef RTF_REJECT
    if (flags & RTF_REJECT)
    {
        return false;
    }
#endif
#ifdef RTF_BLACKHOLE
    if (flags & RTF_BLACKHOLE)
    {
        return false;
    }
#endif

    offset = AOVPN_GW_V4_REPLY_HEADER;
    for (bit = 0; bit < 16U; ++bit)
    {
        unsigned int addr_bit = 1U << bit;
        size_t step;
        const unsigned char *p;
        if (!(addrs & addr_bit))
        {
            continue;
        }
        if (offset > msg_len)
        {
            return false;
        }
        p = data + offset;
        step = aovpn_gw_sockaddr_step(p, (size_t)msg_len - offset);
        if (step == 0U)
        {
            return false;
        }
        if (addr_bit == RTA_DST)
        {
            if (have_destination || !aovpn_gw_ipv4_sockaddr(p, step, &destination))
            {
                return false;
            }
            have_destination = true;
        }
        else if (addr_bit == RTA_GATEWAY)
        {
            if (have_gateway || !aovpn_gw_ipv4_sockaddr(p, step, &gateway))
            {
                return false;
            }
            have_gateway = true;
        }
        else if (addr_bit == RTA_NETMASK)
        {
            if (have_mask || !aovpn_gw_zero_mask(p, step))
            {
                return false;
            }
            have_mask = true;
        }
        offset += step;
    }
    /* Reject trailing or truncated address data, unexpected routes, and
     * unusable gateways including loopback, multicast, link-local, or 0/8. */
    if (offset != msg_len || !have_destination || !have_gateway || !have_mask
        || destination != 0 || gateway == 0 || gateway == 0xffffffffU
        || (gateway & 0xff000000U) == 0x7f000000U
        || (gateway & 0xff000000U) == 0x00000000U
        || (gateway & 0xf0000000U) == 0xe0000000U
        || (gateway & 0xffff0000U) == 0xa9fe0000U)
    {
        return false;
    }
    *out = gateway;
    return true;
}

void
get_default_gateway(struct route_gateway_info *rgi, openvpn_net_ctx_t *ctx)
{
    unsigned char request[AOVPN_GW_V4_REQUEST_LEN];
    unsigned char reply[AOVPN_GW_REPLY_MAX];
    struct timeval tv;
    fd_set rfds;
    static unsigned int counter;
    unsigned long sequence = 0;
    in_addr_t gateway;
    int fd = -1, sent, n, i;
    unsigned int flags = (unsigned int)(RTF_UP | RTF_GATEWAY | RTF_STATIC);
    unsigned int addrs = (unsigned int)(RTA_DST | RTA_NETMASK | RTA_IFP);

    (void)ctx;
    CLEAR(*rgi);
    if (!SocketBase)
    {
        msg(M_WARN, "AOVPN-GW: bsdsocket.library unavailable");
        return;
    }
    if (gettimeofday(&tv, NULL) == 0)
    {
        sequence = (unsigned long)((unsigned int)tv.tv_sec
                                   ^ (unsigned int)tv.tv_usec);
    }
    sequence = (sequence ^ ++counter) & 0x7fffffffUL;
    if (!sequence)
    {
        sequence = 1;
    }
    memset(request, 0, sizeof(request));
    aovpn_gw_put16(request, sizeof(request));
    request[2] = AOVPN_GW_V4_VERSION;
    request[3] = RTM_GET;
    aovpn_gw_put32(request + 8, flags);
    aovpn_gw_put32(request + 12, addrs);
    aovpn_gw_put32(request + 20, sequence);
    /* Request layout verified on MiamiDX: header 76; DST (16),
     * NETMASK (16), IFP (24). Do not cast to SDK's rt_msghdr. */
    request[76] = 16;
    request[77] = (unsigned char)AF_INET;
    request[92] = 16;
    request[93] = (unsigned char)AF_INET;
    request[108] = 24;
#ifdef AF_LINK
    request[109] = (unsigned char)AF_LINK;
#else
    request[109] = 18;
#endif

    fd = socket(PF_ROUTE, SOCK_RAW, 0);
    if (fd < 0)
    {
        msg(M_WARN, "AOVPN-GW: cannot open MiamiDX routing socket (errno=%d)", errno);
        return;
    }
    if (fd >= FD_SETSIZE)
    {
        msg(M_WARN, "AOVPN-GW: routing socket FD too large for WaitSelect");
        goto done;
    }
    sent = send(fd, (const char *)request, (int)sizeof(request), 0);
    if (sent != (int)sizeof(request))
    {
        const int saved_errno = errno;
        msg(M_WARN, "AOVPN-GW: RTM_GET v4 send failed (ret=%d errno=%d)",
            sent, saved_errno);
        goto done;
    }
    for (i = 0; i < 3; ++i)
    {
        tv.tv_sec = AOVPN_GW_TIMEOUT_SECONDS;
        tv.tv_usec = 0;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        n = WaitSelect(fd + 1, &rfds, NULL, NULL, &tv, NULL);
        if (n <= 0)
        {
            msg(M_WARN, "AOVPN-GW: RTM_GET v4 reply wait=%d", n);
            break;
        }
        n = recv(fd, (char *)reply, (int)sizeof(reply), 0);
        if (n <= 0)
        {
            msg(M_WARN, "AOVPN-GW: RTM_GET v4 receive=%d", n);
            break;
        }
        if (!aovpn_gw_parse_reply(reply, (size_t)n, sequence, &gateway))
        {
            continue;
        }
        rgi->gateway.addr = gateway; /* OpenVPN expects host-order IPv4. */
        rgi->flags = RGI_ADDR_DEFINED;
        msg(M_INFO, "AOVPN-GW: default IPv4 gateway %u.%u.%u.%u (MiamiDX RTM_GET v4)",
            (unsigned int)((gateway >> 24) & 255U),
            (unsigned int)((gateway >> 16) & 255U),
            (unsigned int)((gateway >> 8) & 255U),
            (unsigned int)(gateway & 255U));
        break;
    }
    if (!(rgi->flags & RGI_ADDR_DEFINED))
    {
        msg(M_WARN, "AOVPN-GW: no validated IPv4 default gateway; left undefined");
    }
done:
    CloseSocket(fd);
}

void
get_default_gateway_ipv6(struct route_ipv6_gateway_info *rgi6,
                         const struct in6_addr *dest, openvpn_net_ctx_t *ctx)
{
    (void)dest;
    (void)ctx;
    CLEAR(*rgi6);
    /* MiamiDX / current AmiOpenVPN backend intentionally IPv4-only. */
}

#undef AOVPN_GW_V4_VERSION
#undef AOVPN_GW_V4_REQUEST_LEN
#undef AOVPN_GW_V4_REPLY_HEADER
#undef AOVPN_GW_REPLY_MAX
#undef AOVPN_GW_TIMEOUT_SECONDS

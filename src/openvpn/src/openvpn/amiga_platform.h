/* AmiOpenVPN: AmigaOS 3.x compatibility boundary. GPL-2.0-only. */
#ifndef AOVPN_AMIGA_PLATFORM_H
#define AOVPN_AMIGA_PLATFORM_H
#ifdef TARGET_AMIGA
/* Roadshow/AmiTCP netinclude exposes duplicate Unix-style prototypes in
 * sys/socket.h and arpa/inet.h unless this is defined before ANY network
 * header. Those prototypes can collide with GCC bsdsocket inline macros
 * (inet_addr/inet_aton/inet_network), and may depend on an unavailable
 * ssize_t. Keep the real APIs from <proto/bsdsocket.h> instead. */
#ifndef __NO_NET_API
#define __NO_NET_API 1
#endif
#include <exec/types.h>
#include <exec/libraries.h>
#include <proto/exec.h>
#include <proto/bsdsocket.h>
/* AmiOpenVPN POC2 FIX8: Roadshow exposes mbuf_free as an inline
 * bsdsocket.library API macro.  OpenVPN independently declares
 * mbuf_free(struct mbuf_set *), so discard only this one macro.
 * OpenVPN does not use Roadshow's kernel-mbuf management API. */
#ifdef mbuf_free
#undef mbuf_free
#endif
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

/* AmiOpenVPN POC2 FIX9: IPv6 address *representation* helpers.
 *
 * Some Amiga NDK headers expose struct in6_addr but omit the conventional
 * address formatting size and classification helpers.  OpenVPN refers to
 * them even if the actual UDP transport is restricted to IPv4.
 * This does NOT enable IPv6 sockets, routing, or a VPN IPv6 tunnel.
 */
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

#if !defined(IN6_IS_ADDR_UNSPECIFIED) || \
    !defined(IN6_IS_ADDR_LOOPBACK) || \
    !defined(IN6_ARE_ADDR_EQUAL)
/* The IPv6 address on the wire is exactly 16 bytes. Fail the build rather
 * than silently interpreting a differently sized NDK structure. */
typedef char aovpn_amiga_in6_addr_must_be_16_bytes[
    (sizeof(struct in6_addr) == 16) ? 1 : -1];

static inline int
aovpn_amiga_in6_all_zero(const struct in6_addr *addr)
{
    const unsigned char *p = (const unsigned char *)addr;
    unsigned int i;
    for (i = 0; i < 16; ++i)
    {
        if (p[i] != 0) return 0;
    }
    return 1;
}

static inline int
aovpn_amiga_in6_loopback(const struct in6_addr *addr)
{
    const unsigned char *p = (const unsigned char *)addr;
    unsigned int i;
    for (i = 0; i < 15; ++i)
    {
        if (p[i] != 0) return 0;
    }
    return p[15] == 1;
}

static inline int
aovpn_amiga_in6_equal(const struct in6_addr *a,
                     const struct in6_addr *b)
{
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    unsigned int i;
    for (i = 0; i < 16; ++i)
    {
        if (pa[i] != pb[i]) return 0;
    }
    return 1;
}

#ifndef IN6_IS_ADDR_UNSPECIFIED
#define IN6_IS_ADDR_UNSPECIFIED(a) aovpn_amiga_in6_all_zero((a))
#endif
#ifndef IN6_IS_ADDR_LOOPBACK
#define IN6_IS_ADDR_LOOPBACK(a) aovpn_amiga_in6_loopback((a))
#endif
#ifndef IN6_ARE_ADDR_EQUAL
#define IN6_ARE_ADDR_EQUAL(a, b) aovpn_amiga_in6_equal((a), (b))
#endif
#endif /* missing IPv6 address helper macros */

/* AmiOpenVPN POC2 FIX10: IPv6 unspecified address initializer. */
/* OpenVPN uses this aggregate initializer in common source files even in an
 * IPv4-only build. C aggregate initialization zero-fills all remaining
 * members, regardless of the NDK's particular in6_addr member layout.
 * This only makes the 16-byte IPv6 data type representable: it does not
 * enable IPv6 transport on the AmigaOS TCP/IP stack. */
#ifndef IN6ADDR_ANY_INIT
#define IN6ADDR_ANY_INIT { 0 }
#endif

/* Sockets in bsdsocket.library are not AmigaDOS file descriptors. */
extern struct Library *SocketBase;

/* AmiOpenVPN POC2 FIX19: strict-C99 libc declarations.
 * The linked AmigaOS/newlib libc provides these symbols, but its headers do
 * not expose their prototypes under the OpenVPN build's -std=c99 mode.
 * Missing char* returns are especially dangerous. Keep this compatibility
 * block under TARGET_AMIGA only. No ABI replacements are implemented here.
 */
#include <stddef.h>
extern char *strdup(const char *s);
extern char *strsep(char **stringp, const char *delim);
extern size_t strnlen(const char *s, size_t maxlen);
extern int strcasecmp(const char *s1, const char *s2);
extern int strncasecmp(const char *s1, const char *s2, size_t n);
extern long random(void);
extern void srandom(unsigned int seed);
#ifndef isascii
extern int isascii(int c);
#endif

/* Local-only, opt-in custom trace. Independent of OpenVPN --verb and
 * of all shutdown and error-handling paths. Defaults OFF. */
void aovpn_set_diagnostics(int enabled);
int aovpn_diagnostics_enabled(void);

int aovpn_socket_init(void);
/* AOVPN25: manual AmiSSL lifecycle */
int aovpn_amissl_init(void);
/* FIX28 regression restoration: guaranteed explicit AmigaOS resource cleanup
 * during openvpn_exit(), prior to C runtime teardown.
 */
void aovpn_amiga_shutdown(void);
/* AOVPN27: replaces unusable newlib argc/argv from GetArgStr(). */
int aovpn_amiga_rebuild_argv(int *argc, char ***argv);

/* TUN bridge backend supplies this mask and ready predicate in a later POC.
 * Zero means no TUN event source registered. */
/* POC4: single-process SANA-II TUN implementation on vpn.device unit 1.
 * open and close are deliberately separate from the TLS/socket lifecycle. */
int aovpn_bridge_open(void);
void aovpn_bridge_close(void);
int aovpn_bridge_read(unsigned char *buf, int len);
int aovpn_bridge_write(const unsigned char *buf, int len);
unsigned long aovpn_tun_signal_mask(void);
int aovpn_tun_read_ready(void);
int aovpn_tun_write_ready(void);

#define AOVPN_TUN_EVENT (-2)

/* AmiOpenVPN POC2 FIX15: specific MiamiDX/bsdsocket interfaces.
 * This is inserted AFTER the native AmigaOS socket declarations. */
#ifdef TARGET_AMIGA
/* inet_ntoa is provided by bsdsocket.library as Inet_NtoA(ULONG).
 * Its returned pointer belongs to the library and must be copied before
 * the next conversion. OpenVPN call sites copy it immediately. */
#ifndef inet_ntoa
#define inet_ntoa(address) Inet_NtoA((address).s_addr)
#endif

/* Socket descriptors are bsdsocket.library handles, NOT C runtime fds.
 * For socket readiness, AmigaOS uses WaitSelect. Callers whose use of
 * select(0,...) is only a delay are subject to WaitSelect semantics. */
#ifndef select
#define select(n,r,w,e,t) WaitSelect((n),(r),(w),(e),(t),NULL)
#endif

/* If the NDK lacks FD_SET while providing FD_ISSET and fd_set, use the
 * fd_set bitmap format used by AmigaOS3 Roadshow/AmiTCP NDK headers.
 * Explicit member access intentionally fails at compile time on a
 * different layout rather than corrupting memory at runtime.
 * Validation: FD_SET + FD_ISSET probe on actual target required. */
#if !defined(FD_SET)
#if !defined(FD_ISSET) || !defined(FD_ZERO) || !defined(FD_SETSIZE)
#error "AmiOpenVPN FIX15: missing native FD_* macros (inspect NDK)"
#endif
#define FD_SET(fd, setptr) do { \
    unsigned int aovpn_fd = (unsigned int)(fd); \
    if (aovpn_fd < (unsigned int)FD_SETSIZE) { \
        (setptr)->fds_bits[aovpn_fd / 32U] |= (1UL << (aovpn_fd % 32U)); \
    } \
} while (0)
#endif
#endif /* TARGET_AMIGA */

#endif
#endif

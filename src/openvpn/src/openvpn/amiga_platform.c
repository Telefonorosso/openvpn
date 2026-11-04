/* Native AmigaOS 68k socket library lifecycle. GPL-2.0-only. */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "amiga_platform.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef TARGET_AMIGA
#include <proto/dos.h> /* AOVPN27 */
#include <exec/tasks.h> /* Exec tc_SigAlloc shutdown validation */
/* AOVPN25: explicit AmiSSL v5 lifecycle, no libamisslauto.a */
#include <proto/amisslmaster.h>
#include <libraries/amisslmaster.h>
#include <libraries/amissl.h>
#include <amissl/amissl.h>
#include <utility/tagitem.h>

/* AOVPN25: explicit AmiSSL lifecycle. SocketBase belongs to AmiOpenVPN.
 * NEVER link libamisslauto.a with these global base definitions.
 * Calling OpenAmiSSLTags() requires a live SocketBase/errno pointer.
 */
struct Library *SocketBase = NULL;
struct Library *AmiSSLMasterBase = NULL;
struct Library *AmiSSLBase = NULL;
struct Library *AmiSSLExtBase = NULL;
/* POC4 CUSTOM-DIAG FIX1: quiet by default; enabled by a local .ovpn option.
 * During bootstrap the profile has not yet been parsed, so early markers
 * cannot be emitted via a configuration-file switch. Fatal errors still print.
 */
static int aovpn_custom_diag = 0;

void aovpn_set_diagnostics(int enabled)
{
    aovpn_custom_diag = enabled != 0;
}

int aovpn_diagnostics_enabled(void)
{
    return aovpn_custom_diag;
}

static int aovpn_amissl_opened = 0;

static void aovpn_amissl_cleanup(void)
{
    if (aovpn_amissl_opened) {
        if (aovpn_diagnostics_enabled()) {
            fputs("AOVPN25 before CloseAmiSSL\n", stderr);
            fflush(stderr);
        }
        CloseAmiSSL();
        aovpn_amissl_opened = 0;
    }
    AmiSSLBase = NULL;
    AmiSSLExtBase = NULL;
    if (AmiSSLMasterBase != NULL) {
        CloseLibrary(AmiSSLMasterBase);
        AmiSSLMasterBase = NULL;
    }
}

int aovpn_amissl_init(void)
{
    LONG rc;
    if (aovpn_amissl_opened && AmiSSLBase != NULL && AmiSSLExtBase != NULL)
        return 1;
    if (SocketBase == NULL) {
        fputs("AOVPN25 ERROR: socket not initialized\n", stderr); fflush(stderr);
        return 0;
    }
    if (AmiSSLMasterBase != NULL || AmiSSLBase != NULL || AmiSSLExtBase != NULL) {
        fputs("AOVPN25 ERROR: inconsistent AmiSSL base state\n", stderr); fflush(stderr);
        return 0;
    }
    if (aovpn_diagnostics_enabled()) {
        fputs("AOVPN25 before OpenLibrary(amisslmaster.library)\n", stderr); fflush(stderr);
    }
    AmiSSLMasterBase = OpenLibrary((CONST_STRPTR)"amisslmaster.library",
                                   AMISSLMASTER_MIN_VERSION);
    if (AmiSSLMasterBase == NULL) {
        fputs("AOVPN25 ERROR: cannot open amisslmaster.library (check AmiSSL v5 installation)\n", stderr);
        fflush(stderr);
        return 0;
    }
    if (aovpn_diagnostics_enabled()) {
        fputs("AOVPN25 before OpenAmiSSLTags\n", stderr); fflush(stderr);
    }
    rc = OpenAmiSSLTags(AMISSL_CURRENT_VERSION,
                        AmiSSL_UsesOpenSSLStructs, TRUE,
                        AmiSSL_GetAmiSSLBase, (ULONG)&AmiSSLBase,
                        AmiSSL_GetAmiSSLExtBase, (ULONG)&AmiSSLExtBase,
                        AmiSSL_SocketBase, (ULONG)SocketBase,
                        AmiSSL_ErrNoPtr, (ULONG)&errno,
                        TAG_DONE);
    if (aovpn_diagnostics_enabled()) {
        fprintf(stderr, "AOVPN25 OpenAmiSSLTags rc=%ld main=%p ext=%p\n",
                (long)rc, (void *)AmiSSLBase, (void *)AmiSSLExtBase);
        fflush(stderr);
    }
    if (rc != 0) {
        /* OpenAmiSSLTags() did not succeed: CloseAmiSSL() is not called. */
        AmiSSLBase = NULL;
        AmiSSLExtBase = NULL;
        CloseLibrary(AmiSSLMasterBase);
        AmiSSLMasterBase = NULL;
        fputs("AOVPN25 ERROR: AmiSSL v5 initialization failed\n", stderr);
        fflush(stderr);
        return 0;
    }
    aovpn_amissl_opened = 1;
    if (AmiSSLBase == NULL || AmiSSLExtBase == NULL) {
        fputs("AOVPN25 ERROR: missing library bases after successful open\n", stderr);
        fflush(stderr);
        aovpn_amissl_cleanup();
        return 0;
    }
    /* The socket cleanup handler was registered earlier by FIX20/FIX21.
     * atexit is LIFO: close AmiSSL first, then bsdsocket.library. */
    if (atexit(aovpn_amissl_cleanup) != 0) {
        fputs("AOVPN25 ERROR: cannot register AmiSSL cleanup\n", stderr);
        fflush(stderr);
        aovpn_amissl_cleanup();
        return 0;
    }
    if (aovpn_diagnostics_enabled()) {
        fputs("AOVPN25 AmiSSL ready; entering OpenVPN\n", stderr); fflush(stderr);
    }
    return 1;
}

/* AmiOpenVPN POC2 FIX21: startup trace for Guru 8000000A.
 * DIAGNOSTIC ONLY. Flush each marker so the last one survives a crash.
 * Do not use the trace build for network traffic or with private keys.
 */
#define AOVPN_MARK(x) do { \
    if (aovpn_diagnostics_enabled()) { \
        fputs("AOVPN21 " x "\n", stderr); fflush(stderr); \
    } \
} while (0)
static int aovpn_socket_opened_here = 0;
static int aovpn_socket_exit_registered = 0;

static void aovpn_socket_cleanup(void)
{
    AOVPN_MARK("C1 cleanup entered");
    if (aovpn_socket_opened_here && SocketBase != NULL) {
        AOVPN_MARK("C2 before CloseLibrary");
        CloseLibrary(SocketBase);
        AOVPN_MARK("C3 after CloseLibrary");
        SocketBase = NULL;
    }
    aovpn_socket_opened_here = 0;
    AOVPN_MARK("C4 cleanup exited");
}

int aovpn_socket_init(void)
{
    AOVPN_MARK("S1 socket init entered");
    if (SocketBase == NULL) {
        AOVPN_MARK("S2 before OpenLibrary");
        SocketBase = OpenLibrary((CONST_STRPTR)"bsdsocket.library", 4);
        AOVPN_MARK("S3 after OpenLibrary");
        if (SocketBase == NULL) {
            AOVPN_MARK("S4 OpenLibrary returned NULL");
            fputs("AmiOpenVPN: cannot open bsdsocket.library v4\n", stderr);
            fflush(stderr);
            return 0;
        }
        aovpn_socket_opened_here = 1;
        AOVPN_MARK("S5 before atexit registration");
        if (!aovpn_socket_exit_registered) {
            if (atexit(aovpn_socket_cleanup) != 0) {
                AOVPN_MARK("S6 atexit registration failed");
                aovpn_socket_cleanup();
                return 0;
            }
            aovpn_socket_exit_registered = 1;
        }
        AOVPN_MARK("S7 after atexit registration");
    } else {
        AOVPN_MARK("S2 existing SocketBase");
    }
    AOVPN_MARK("S8 before SetErrnoPtr");
    /* Same call as SetErrnoPtr(); GCC 16 rejects that inline macro. */
    SocketBaseTags(SBTM_SETVAL(SBTC_ERRNOPTR(sizeof(errno))), (ULONG)&errno, TAG_END);
    AOVPN_MARK("S9 after SetErrnoPtr");
    return 1;
}

/* Deliberate fail-closed diagnostics until vpn.device IPC exists. */
/* AOVPN27: native AmigaDOS command-line adapter */
/* AOVPN27: native AmigaDOS command-line adapter.
 * newlib's argc/argv have been observed invalid at main() on the tested
 * AmigaOS 68k configuration. Never dereference the incoming argv.
 * GetArgStr() is the NUL-terminated CLI arguments WITHOUT the program name.
 * Supports AmigaDOS double quotes and *-escapes; keeps --options unchanged.
 * Fixed limits fail closed rather than silently truncating VPN options.
 * Static storage persists through openvpn_main() and is not reentrant.
 * CLI-only prototype; no Workbench ToolTypes support.
 */
#define AOVPN27_LINE_MAX 4096U
#define AOVPN27_ARG_MAX 128
static char aovpn27_token_data[AOVPN27_LINE_MAX + 1U];
static char aovpn27_prog_name[256];
static char *aovpn27_argv[AOVPN27_ARG_MAX + 1];

static int aovpn27_space(unsigned char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

int aovpn_amiga_rebuild_argv(int *argc_out, char ***argv_out)
{
    const char *raw;
    unsigned int len = 0, src = 0, dst = 0;
    int argc = 1;
    if (argc_out == NULL || argv_out == NULL) {
        fputs("AOVPN27 ERROR: invalid output pointers\n", stderr);
        return 0;
    }
    raw = (const char *)GetArgStr();
    if (raw == NULL) {
        fputs("AOVPN27 ERROR: no AmigaDOS CLI argument string\n", stderr);
        fflush(stderr);
        return 0;
    }
    /* GetArgStr() is NUL terminated (AmigaDOS V36+). */
    while (len <= AOVPN27_LINE_MAX && raw[len] != '\0') {
        ++len;
    }
    if (len > AOVPN27_LINE_MAX) {
        fputs("AOVPN27 ERROR: command line too long\n", stderr);
        fflush(stderr);
        return 0;
    }
    aovpn27_argv[0] = (char *)"openvpn";
    if (GetProgramName(aovpn27_prog_name, (LONG)sizeof aovpn27_prog_name)
        && aovpn27_prog_name[0] != '\0') {
        aovpn27_argv[0] = aovpn27_prog_name;
    }
    while (src < len) {
        int in_quotes = 0;
        while (src < len && aovpn27_space((unsigned char)raw[src])) {
            ++src;
        }
        if (src == len) {
            break;
        }
        if (argc >= AOVPN27_ARG_MAX) {
            fputs("AOVPN27 ERROR: too many command-line arguments\n", stderr);
            fflush(stderr);
            return 0;
        }
        aovpn27_argv[argc++] = &aovpn27_token_data[dst];
        while (src < len) {
            char c = raw[src++];
            if (c == '"') {
                in_quotes = !in_quotes;
                continue;
            }
            /* AmigaDOS '*' escaping, notably *" and ** inside quotes.
             * Avoid treating ordinary asterisks as Unix backslashes. */
            if (c == '*' && src < len) {
                char next = raw[src];
                if (next == '"' || next == '*') {
                    c = next;
                    ++src;
                } else if (in_quotes && (next == 'N' || next == 'n')) {
                    c = '\n';
                    ++src;
                } else if (in_quotes && (next == 'E' || next == 'e')) {
                    c = (char)0x1B;
                    ++src;
                }
            }
            if (!in_quotes && aovpn27_space((unsigned char)c)) {
                break;
            }
            if (dst >= AOVPN27_LINE_MAX) {
                fputs("AOVPN27 ERROR: expanded command line too long\n", stderr);
                fflush(stderr);
                return 0;
            }
            aovpn27_token_data[dst++] = c;
        }
        if (in_quotes) {
            fputs("AOVPN27 ERROR: unterminated double-quoted argument\n", stderr);
            fflush(stderr);
            return 0;
        }
        aovpn27_token_data[dst++] = '\0';
    }
    aovpn27_argv[argc] = NULL;
    *argc_out = argc;
    *argv_out = aovpn27_argv;
    if (aovpn_diagnostics_enabled()) {
        fprintf(stderr, "AOVPN27 rebuilt argc=%d from GetArgStr\n", argc);
        fflush(stderr);
    }
    return 1;
}

/* POC4 SANA-II IP bridge.  The wire-data path uses the same API and 68k
 * callback ABI as the hardware-validated AOVPN-MiamiPeer (FIX4).
 * This implementation is strictly single-threaded and supports one client
 * process and one outstanding asynchronous CMD_READ.  The unit 0/MiamiDX
 * side is neither opened nor configured here.
 */
#include <exec/io.h>
#include <devices/sana2.h>
#include <utility/tagitem.h>
#include <string.h>

#define AOVPN4_DEVICE "vpn.device"
#define AOVPN4_UNIT 1UL
#define AOVPN4_MTU 1400U
#define AOVPN4_IPV4 0x0800UL

struct AovpnBridge {
    struct MsgPort *ctl_port, *rx_port, *tx_port;
    struct IOSana2Req *ctl, *rx, *tx;
    struct TagItem tags[3];
    UBYTE rx_packet[AOVPN4_MTU];
    int device_open, unit_online, active, rx_pending, rx_done;
};
static struct AovpnBridge aovpn4;
static int aovpn4_exit_registered = 0;

/* Must be compiled with m68k-amigaos-gcc; fixed-register arguments are
 * essential. CopyMem is valid with Exec task scheduling disabled (driver
 * FIX4 currently invokes this callback while holding Disable()). */
static BOOL aovpn4_copy(APTR to asm("a0"), APTR from asm("a1"),
                        ULONG size asm("d0"))
{
    CopyMem(from, to, size);
    return TRUE;
}
static void aovpn4_clone(struct IOSana2Req *dst, const struct IOSana2Req *src)
{
    dst->ios2_Req.io_Device = src->ios2_Req.io_Device;
    dst->ios2_Req.io_Unit = src->ios2_Req.io_Unit;
    dst->ios2_BufferManagement = src->ios2_BufferManagement;
}
static int aovpn4_control(UWORD command)
{
    struct AovpnBridge *p = &aovpn4;
    LONG rc;
    p->ctl->ios2_Req.io_Command = command;
    p->ctl->ios2_WireError = 0;
    rc = DoIO((struct IORequest *)p->ctl);
    if (rc || p->ctl->ios2_Req.io_Error)
    {
        fprintf(stderr, "AOVPN4 control command=%u rc=%ld ioerr=%ld wire=%lu\n",
                (unsigned)command, (long)rc,
                (long)p->ctl->ios2_Req.io_Error,
                (unsigned long)p->ctl->ios2_WireError);
        fflush(stderr);
        return 0;
    }
    return 1;
}
void aovpn_bridge_close(void)
{
    struct AovpnBridge *p = &aovpn4;
    p->active = 0;
    if (p->rx_pending && p->rx)
    {
        AbortIO((struct IORequest *)p->rx);
        (void)WaitIO((struct IORequest *)p->rx);
        p->rx_pending = 0;
    }
    p->rx_done = 0;
    if (p->device_open)
    {
        if (p->unit_online)
        {
            (void)aovpn4_control(S2_OFFLINE);
            p->unit_online = 0;
        }
        CloseDevice((struct IORequest *)p->ctl);
        p->device_open = 0;
    }
    if (p->tx) DeleteIORequest((struct IORequest *)p->tx);
    if (p->rx) DeleteIORequest((struct IORequest *)p->rx);
    if (p->ctl) DeleteIORequest((struct IORequest *)p->ctl);
    if (p->tx_port) DeleteMsgPort(p->tx_port);
    if (p->rx_port) DeleteMsgPort(p->rx_port);
    if (p->ctl_port) DeleteMsgPort(p->ctl_port);
    memset(p, 0, sizeof(*p));
}
static void aovpn4_arm_read(void)
{
    struct AovpnBridge *p = &aovpn4;
    if (!p->active || p->rx_pending || p->rx_done) return;
    p->rx->ios2_Req.io_Command = CMD_READ;
    p->rx->ios2_PacketType = AOVPN4_IPV4;
    p->rx->ios2_Data = p->rx_packet;
    p->rx->ios2_DataLength = AOVPN4_MTU;
    p->rx->ios2_WireError = 0;
    SendIO((struct IORequest *)p->rx);
    p->rx_pending = 1;
}
int aovpn_bridge_open(void)
{
    struct AovpnBridge *p = &aovpn4;
    struct Sana2DeviceQuery query;
    LONG rc;
    if (p->active || p->device_open) return 0;
    memset(p, 0, sizeof(*p));
    p->ctl_port = CreateMsgPort();
    p->rx_port = CreateMsgPort();
    p->tx_port = CreateMsgPort();
    if (!p->ctl_port || !p->rx_port || !p->tx_port) goto fail;
    p->ctl = (struct IOSana2Req *)CreateIORequest(p->ctl_port, sizeof(*p->ctl));
    p->rx = (struct IOSana2Req *)CreateIORequest(p->rx_port, sizeof(*p->rx));
    p->tx = (struct IOSana2Req *)CreateIORequest(p->tx_port, sizeof(*p->tx));
    if (!p->ctl || !p->rx || !p->tx) goto fail;
    p->tags[0].ti_Tag = S2_CopyToBuff;
    p->tags[0].ti_Data = (ULONG)aovpn4_copy;
    p->tags[1].ti_Tag = S2_CopyFromBuff;
    p->tags[1].ti_Data = (ULONG)aovpn4_copy;
    p->tags[2].ti_Tag = TAG_DONE;
    p->tags[2].ti_Data = 0;
    p->ctl->ios2_BufferManagement = p->tags;
    rc = OpenDevice((STRPTR)AOVPN4_DEVICE, AOVPN4_UNIT,
                    (struct IORequest *)p->ctl, 0);
    if (rc)
    {
        fprintf(stderr, "AOVPN4 OpenDevice(unit=1) failed rc=%ld\n", (long)rc);
        goto fail;
    }
    p->device_open = 1;
    aovpn4_clone(p->rx, p->ctl);
    aovpn4_clone(p->tx, p->ctl);
    memset(&query, 0, sizeof(query));
    query.SizeAvailable = sizeof(query);
    p->ctl->ios2_Req.io_Command = S2_DEVICEQUERY;
    p->ctl->ios2_StatData = &query;
    rc = DoIO((struct IORequest *)p->ctl);
    p->ctl->ios2_StatData = NULL;
    if (rc || query.MTU != AOVPN4_MTU || query.HardwareType != S2WireType_PPP)
    {
        fprintf(stderr, "AOVPN4 DEVICEQUERY failed rc=%ld MTU=%lu wire=%lu\n",
                (long)rc, (unsigned long)query.MTU,
                (unsigned long)query.HardwareType);
        goto fail;
    }
    /* unit 1 is the remote peer in the isolated 10.77.0.0/30 laboratory */
    p->ctl->ios2_SrcAddr[0] = 10;
    p->ctl->ios2_SrcAddr[1] = 77;
    p->ctl->ios2_SrcAddr[2] = 0;
    p->ctl->ios2_SrcAddr[3] = 2;
    if (!aovpn4_control(S2_CONFIGINTERFACE)) goto fail;
    if (!aovpn4_control(S2_ONLINE)) goto fail;
    p->unit_online = 1;
    p->active = 1;
    aovpn4_arm_read();
    /* Registered after AmiSSL/socket handlers: runs first on process exit.
     * Also handles M_FATAL paths which do not call close_tun(). */
    if (!aovpn4_exit_registered)
    {
        if (atexit(aovpn_bridge_close) != 0) goto fail;
        aovpn4_exit_registered = 1;
    }
    if (aovpn_diagnostics_enabled()) {
        fputs("AOVPN4 vpn.device unit1 OPEN/RX armed\n", stderr);
    fflush(stderr);
    }
    return 1;
fail:
    aovpn_bridge_close();
    return 0;
}
unsigned long aovpn_tun_signal_mask(void)
{
    struct AovpnBridge *p = &aovpn4;
    return p->active && p->rx_port ? (1UL << p->rx_port->mp_SigBit) : 0UL;
}
int aovpn_tun_read_ready(void)
{
    struct AovpnBridge *p = &aovpn4;
    struct Message *msg;
    if (!p->active) return 0;
    if (p->rx_done) return 1;
    if (!p->rx_pending) return 0;
    /* GetMsg acknowledges the completed request; never WaitIO after GetMsg.
     * WaitIO is used only for a still-inflight request during shutdown. */
    msg = GetMsg(p->rx_port);
    if (!msg) return 0;
    if (msg != &p->rx->ios2_Req.io_Message)
    {
        fprintf(stderr, "AOVPN4 unexpected reply on RX MsgPort\n");
        return 0;
    }
    p->rx_pending = 0;
    p->rx_done = 1;
    return 1;
}
int aovpn_tun_write_ready(void)
{
    return aovpn4.active != 0;
}
int aovpn_bridge_read(unsigned char *buf, int len)
{
    struct AovpnBridge *p = &aovpn4;
    ULONG n;
    if (!buf || len <= 0 || !p->active) { errno = ENODEV; return -1; }
    if (!aovpn_tun_read_ready()) { errno = EAGAIN; return -1; }
    p->rx_done = 0;
    n = p->rx->ios2_DataLength;
    if (p->rx->ios2_Req.io_Error != 0) { errno = EIO; return -1; }
    if (n < 20 || n > AOVPN4_MTU || (unsigned int)len < n ||
        (p->rx_packet[0] >> 4) != 4)
    {
        errno = EMSGSIZE;
        aovpn4_arm_read();
        return -1;
    }
    CopyMem(p->rx_packet, buf, n);
    aovpn4_arm_read();
    return (int)n;
}
int aovpn_bridge_write(const unsigned char *buf, int len)
{
    struct AovpnBridge *p = &aovpn4;
    LONG rc;
    int i;
    if (!p->active) { errno = ENODEV; return -1; }
    if (!buf || len < 20 || len > (int)AOVPN4_MTU || (buf[0] >> 4) != 4)
    {
        errno = EMSGSIZE;
        return -1;
    }
    p->tx->ios2_Req.io_Command = CMD_WRITE;
    p->tx->ios2_PacketType = AOVPN4_IPV4;
    p->tx->ios2_Data = (APTR)buf;
    p->tx->ios2_DataLength = (ULONG)len;
    p->tx->ios2_WireError = 0;
    for (i = 0; i < 4; ++i)
    {
        p->tx->ios2_SrcAddr[i] = buf[12+i];
        p->tx->ios2_DstAddr[i] = buf[16+i];
    }
    rc = DoIO((struct IORequest *)p->tx);
    if (rc || p->tx->ios2_Req.io_Error)
    {
        fprintf(stderr, "AOVPN4 CMD_WRITE rc=%ld err=%ld wire=%lu\n",
                (long)rc, (long)p->tx->ios2_Req.io_Error,
                (unsigned long)p->tx->ios2_WireError);
        errno = EIO;
        return -1;
    }
    return len;
}

/* POC4 EXEC-CLEANUP FIX1: restore POC2 FIX28's explicit exit path.
 * The validated FIX28 released the timer signal and a second allocated
 * signal before process exit; relying exclusively on atexit regressed.
 *
 * Close vpn.device before its socket/TLS dependencies.  All four local
 * cleanup functions tolerate a second call from atexit or close_tun().
 * The counter uses this task's Exec allocation bitmap (system bits 0..15
 * remain reserved and will generally appear as 0000FFFF).
 */
#if defined(AOVPN_BUNDLED_GETTIMEOFDAY)
extern void aovpn_amiga_timer_shutdown(void);
#endif
void
aovpn_amiga_shutdown(void)
{
    struct Task *task = FindTask(NULL);
    ULONG before = task ? task->tc_SigAlloc : 0UL;
    ULONG after;

    aovpn_bridge_close();
#if defined(AOVPN_BUNDLED_GETTIMEOFDAY)
    aovpn_amiga_timer_shutdown();
#endif
    aovpn_amissl_cleanup();
    aovpn_socket_cleanup();

    after = task ? task->tc_SigAlloc : 0UL;
    if (aovpn_diagnostics_enabled()) {
        fprintf(stderr,
                "AOVPN28 shutdown SigAlloc %08lX -> %08lX; released %08lX\n",
                (unsigned long)before, (unsigned long)after,
                (unsigned long)(before & ~after));
        fflush(stderr);
    }
}

#endif

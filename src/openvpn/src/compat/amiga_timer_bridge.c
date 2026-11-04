/* AmiOpenVPN POC2 FIX6 - native timer.device bridge, AmigaOS-only ABI.
 * GPL-2.0-only (same distribution terms as the AmiOpenVPN port).
 *
 * IMPORTANT: Do not include <sys/time.h> here. On classic AmigaOS the
 * NDK's <devices/timer.h> declares a different `struct timeval` than the
 * POSIX C runtime. The two structures live in separate translation units.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(TARGET_AMIGA) && defined(AOVPN_BUNDLED_GETTIMEOFDAY)
#include <exec/types.h>
#include <exec/io.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <stdlib.h>

/* The first Amiga clock read opens timer.device; later reads reuse the
 * request. OpenVPN POC2 uses a single Exec task. NOT reentrant/thread-safe.
 */
static struct MsgPort *aovpn_timer_port = NULL;
static struct timerequest *aovpn_timer_request = NULL;
static int aovpn_timer_open = 0;
static int aovpn_timer_atexit = 0;

static void
aovpn_timer_cleanup(void)
{
    if (aovpn_timer_open && aovpn_timer_request) {
        CloseDevice((struct IORequest *)aovpn_timer_request);
        aovpn_timer_open = 0;
    }
    if (aovpn_timer_request) {
        DeleteIORequest((struct IORequest *)aovpn_timer_request);
        aovpn_timer_request = NULL;
    }
    if (aovpn_timer_port) {
        DeleteMsgPort(aovpn_timer_port);
        aovpn_timer_port = NULL;
    }
}

/* POC4 EXEC-CLEANUP FIX1 (restores hardware-validated FIX28 semantics):
 * The OpenVPN exit path must be able to free the timer MsgPort signal
 * explicitly.  Keep the atexit handler as an idempotent fallback.
 */
void
aovpn_amiga_timer_shutdown(void)
{
    aovpn_timer_cleanup();
}

static int
aovpn_timer_init(void)
{
    if (aovpn_timer_open) {
        return 0;
    }
    aovpn_timer_port = CreateMsgPort();
    if (!aovpn_timer_port) {
        return -1;
    }
    aovpn_timer_request = (struct timerequest *)CreateIORequest(
        aovpn_timer_port, sizeof(struct timerequest));
    if (!aovpn_timer_request) {
        aovpn_timer_cleanup();
        return -1;
    }
    if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ,
                   (struct IORequest *)aovpn_timer_request, 0) != 0) {
        aovpn_timer_cleanup();
        return -1;
    }
    aovpn_timer_open = 1;
    if (!aovpn_timer_atexit) {
        if (atexit(aovpn_timer_cleanup) != 0) {
            aovpn_timer_cleanup();
            return -1;
        }
        aovpn_timer_atexit = 1;
    }
    return 0;
}

/* Public bridge uses only scalar 32-bit values. No `struct timeval` crosses
 * this ABI boundary. Both ULONG and unsigned long are 32-bit on m68k.
 */
int
aovpn_amiga_read_system_time(unsigned long *seconds,
                             unsigned long *microseconds)
{
    if (!seconds || !microseconds || aovpn_timer_init() != 0) {
        return -1;
    }

    aovpn_timer_request->tr_node.io_Command = TR_GETSYSTIME;
    if (DoIO((struct IORequest *)aovpn_timer_request) != 0) {
        return -1;
    }
    if (aovpn_timer_request->tr_time.tv_micro >= 1000000UL) {
        return -1;
    }
    *seconds = (unsigned long)aovpn_timer_request->tr_time.tv_secs;
    *microseconds = (unsigned long)aovpn_timer_request->tr_time.tv_micro;
    return 0;
}
#endif

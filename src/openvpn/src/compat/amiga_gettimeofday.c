/* AmiOpenVPN POC2 FIX6 - newlib gettimeofday adapter.
 * GPL-2.0-only (same distribution terms as the AmiOpenVPN port).
 *
 * POSIX and classic AmigaOS use incompatible `struct timeval` definitions.
 * Never include <devices/timer.h> in this translation unit.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(TARGET_AMIGA) && defined(AOVPN_BUNDLED_GETTIMEOFDAY)
#include <sys/time.h>  /* native newlib prototype and __stkparm__ ABI */
#include <stdint.h>
#include <errno.h>

#define AOVPN_UNIX_TO_AMIGA_EPOCH UINT64_C(252460800)

/* Implemented in amiga_timer_bridge.c; scalar values only. */
extern int aovpn_amiga_read_system_time(unsigned long *, unsigned long *);

int
gettimeofday(struct timeval *restrict result,
             struct timezone *restrict timezone_ptr)
{
    unsigned long amiga_seconds;
    unsigned long microseconds;
    uint64_t unix_seconds;
    (void)timezone_ptr;

    if (!result) {
        errno = EINVAL;
        return -1;
    }
    if (aovpn_amiga_read_system_time(&amiga_seconds, &microseconds) != 0) {
        errno = EIO;
        return -1;
    }

    unix_seconds = (uint64_t)amiga_seconds + AOVPN_UNIX_TO_AMIGA_EPOCH;
    result->tv_sec = (time_t)unix_seconds;
    if ((uint64_t)result->tv_sec != unix_seconds) {
        errno = EOVERFLOW;
        return -1;
    }
    result->tv_usec = (suseconds_t)microseconds;
    return 0;
}

/* AmiOpenVPN POC2 FIX15: newlib's reentrant gettimeofdayr.o calls this
 * underlying syscall symbol. Keep the actual clock implementation in
 * gettimeofday() / timer.device (FIX5/6). This wrapper does not substitute
 * a coarse Unix-time clock and preserves the caller's error value. */
int __attribute__((__stkparm__))
_gettimeofday(struct timeval *result, void *timezone_ptr)
{
    return gettimeofday(result, (struct timezone *)timezone_ptr);
}

#endif

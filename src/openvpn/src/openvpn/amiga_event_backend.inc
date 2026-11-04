/* Included within upstream event.c after struct event_set definition.
 * Uses AmiTCP/MiamiDX WaitSelect for BSD sockets AND Exec signal mask.
 * No busy polling, no Win32 HANDLEs. GPL-2.0-only.
 *
 * POC4: a real vpn.device unit 1 signal is included in WaitSelect.
 * TUN writes are synchronous and therefore immediately writable.
 */
#ifdef TARGET_AMIGA
#include "amiga_platform.h"
#include <signal.h>
#include <errno.h>

#define AEV_CAPACITY 64
struct ae_item {
    event_t fd;
    unsigned int flags;
    void *arg;
};
struct ae_set {
    struct event_set_functions func; /* must be first */
    struct ae_item item[AEV_CAPACITY];
    int used;
    int fast;
};
static void ae_free(struct event_set *es)
{
    free(es);
}
static void ae_reset(struct event_set *es)
{
    struct ae_set *ae = (struct ae_set *)es;
    if (ae->fast) ae->used = 0;
}
static void ae_del(struct event_set *es, event_t ev)
{
    struct ae_set *ae = (struct ae_set *)es;
    int i;
    for (i = 0; i < ae->used; ++i) {
        if (ae->item[i].fd == ev) {
            memmove(&ae->item[i], &ae->item[i+1],
                    (size_t)(ae->used - i - 1) * sizeof(ae->item[0]));
            --ae->used;
            return;
        }
    }
}
static void ae_ctl(struct event_set *es, event_t ev,
                   unsigned int flags, void *arg)
{
    struct ae_set *ae = (struct ae_set *)es;
    int i;
    if (!ae->fast) {
        for (i = 0; i < ae->used; ++i) {
            if (ae->item[i].fd == ev) {
                if (flags == 0) {
                    ae_del(es, ev);
                } else {
                    ae->item[i].flags = flags;
                    ae->item[i].arg = arg;
                }
                return;
            }
        }
    }
    if (!flags) return;
    if (ae->used >= AEV_CAPACITY) {
        msg(M_FATAL, "Amiga WaitSelect event set capacity exceeded");
        return;
    }
    ae->item[ae->used].fd = ev;
    ae->item[ae->used].flags = flags;
    ae->item[ae->used].arg = arg;
    ++ae->used;
}
static int ae_wait(struct event_set *es, const struct timeval *timeout,
                   struct event_set_return *out, int outlen)
{
    struct ae_set *ae = (struct ae_set *)es;
    struct timeval tv = {0, 0};
    fd_set rfds, wfds;
    ULONG signals = (ULONG)SIGBREAKF_CTRL_C;
    ULONG tunnel_signals = (ULONG)aovpn_tun_signal_mask();
    int maxfd = -1, i, nout = 0, rc;
    int tun_immediate = 0;
    const struct timeval *wait_tv = timeout;
    if (timeout) tv = *timeout;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    signals |= tunnel_signals;
    for (i = 0; i < ae->used; ++i) {
        event_t fd = ae->item[i].fd;
        unsigned int f = ae->item[i].flags;
        if (fd == AOVPN_TUN_EVENT) {
            /* Do not block on WaitSelect when a decrypted packet can
             * already be written to SANA-II, or a read has completed. */
            if ((f & EVENT_READ) && aovpn_tun_read_ready()) tun_immediate = 1;
            if ((f & EVENT_WRITE) && aovpn_tun_write_ready()) tun_immediate = 1;
            continue;
        }
        if (fd < 0 || fd >= FD_SETSIZE) {
            msg(M_FATAL, "Amiga event backend invalid socket descriptor %d", (int)fd);
            return -1;
        }
        if (f & EVENT_READ) openvpn_fd_set(fd, &rfds); /* AmiOpenVPN POC2 FIX16 */
        if (f & EVENT_WRITE) openvpn_fd_set(fd, &wfds);
        if (fd > maxfd) maxfd = fd;
    }
    /* WaitSelect writes the received Exec signals back into 'signals'. */
    if (tun_immediate) {
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        wait_tv = &tv;
    }
    rc = WaitSelect(maxfd + 1, &rfds, &wfds, NULL,
                    (struct timeval *)wait_tv, &signals);
    if (signals & SIGBREAKF_CTRL_C) {
        throw_signal(SIGINT); /* OpenVPN signal_info, not C runtime raise() */
        errno = EINTR;
        return -1;
    }
    if (rc < 0) return -1;
    for (i = 0; i < ae->used && nout < outlen; ++i) {
        const event_t fd = ae->item[i].fd;
        const unsigned int f = ae->item[i].flags;
        unsigned int ready = 0;
        if (fd == AOVPN_TUN_EVENT) {
            if ((f & EVENT_READ) && aovpn_tun_read_ready()) ready |= EVENT_READ;
            if ((f & EVENT_WRITE) && aovpn_tun_write_ready()) ready |= EVENT_WRITE;
        } else {
            if ((f & EVENT_READ) && FD_ISSET(fd, &rfds)) ready |= EVENT_READ;
            if ((f & EVENT_WRITE) && FD_ISSET(fd, &wfds)) ready |= EVENT_WRITE;
        }
        if (ready) {
            out[nout].rwflags = ready;
            out[nout].arg = ae->item[i].arg;
            ++nout;
        }
    }
    return nout;
}
static struct event_set *ae_init(int *maxevents, unsigned int flags)
{
    struct ae_set *ae = (struct ae_set *)calloc(1, sizeof(*ae));
    if (!ae) return NULL;
    ae->func.free = ae_free;
    ae->func.reset = ae_reset;
    ae->func.del = ae_del;
    ae->func.ctl = ae_ctl;
    ae->func.wait = ae_wait;
    ae->fast = !!(flags & EVENT_METHOD_FAST);
    if (*maxevents < 1) { free(ae); return NULL; }
    if (*maxevents > AEV_CAPACITY) *maxevents = AEV_CAPACITY;
    return (struct event_set *)ae;
}
#endif /* TARGET_AMIGA */

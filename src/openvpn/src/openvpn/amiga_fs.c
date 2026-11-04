/* AmiOpenVPN POC2 FIX16: AmigaDOS filesystem adapter.
 * These functions deliberately do not attempt to override POSIX libc globally.
 * AmigaDOS protection bits are deny-bits; values are translated to POSIX
 * owner permissions for the limited OpenVPN platform_* checks.
 * GPL-2.0-only; this remains unvalidated against the user's actual NDK.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef TARGET_AMIGA
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>

/* The original NDK contract supplies FIBF_{READ,WRITE,EXECUTE}. */
#if !defined(FIBF_READ) || !defined(FIBF_WRITE) || !defined(FIBF_EXECUTE)
#error "FIX16 needs AmigaDOS FIBF_* protection bits; inspect NDK"
#endif

/* Convert a DOS error to an errno without disguising failures as success. */
static int aovpn_dos_error(void)
{
    const LONG doserr = IoErr();
#ifdef ERROR_OBJECT_NOT_FOUND
    if (doserr == ERROR_OBJECT_NOT_FOUND) return ENOENT;
#endif
#ifdef ERROR_OBJECT_IN_USE
    if (doserr == ERROR_OBJECT_IN_USE) return EBUSY;
#endif
#ifdef ERROR_DELETE_PROTECTED
    if (doserr == ERROR_DELETE_PROTECTED) return EACCES;
#endif
    return EIO;
}

static int aovpn_examine_file(const char *path, struct FileInfoBlock *info)
{
    BPTR lock;
    LONG ok;
    if (!path || !*path || !info) { errno = EINVAL; return -1; }
    lock = Lock((STRPTR)path, ACCESS_READ);
    if (!lock) { errno = aovpn_dos_error(); return -1; }
    ok = Examine(lock, info);
    if (!ok) { const int e = aovpn_dos_error(); UnLock(lock); errno = e; return -1; }
    UnLock(lock);
    return 0;
}

int aovpn_amiga_stat(const char *path, struct stat *out)
{
    struct FileInfoBlock *fib;
    long flags;
    unsigned long seconds;
    if (!out) { errno = EINVAL; return -1; }
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    if (!fib) { errno = ENOMEM; return -1; }
    if (aovpn_examine_file(path, fib) < 0) { FreeDosObject(DOS_FIB, fib); return -1; }
    memset(out, 0, sizeof(*out));
    flags = fib->fib_Protection;
    out->st_mode = (fib->fib_DirEntryType > 0 ? S_IFDIR : S_IFREG);
    if (!(flags & FIBF_READ)) out->st_mode |= S_IRUSR;
    if (!(flags & FIBF_WRITE)) out->st_mode |= S_IWUSR;
    if (!(flags & FIBF_EXECUTE)) out->st_mode |= S_IXUSR;
    out->st_size = fib->fib_Size;
    out->st_nlink = 1;
    /* AmigaDOS DateStamp is relative to 1978-01-01 (not 1970). */
    seconds = 252460800UL + ((unsigned long)fib->fib_Date.ds_Days * 86400UL)
              + ((unsigned long)fib->fib_Date.ds_Minute * 60UL)
              + ((unsigned long)fib->fib_Date.ds_Tick / 50UL);
    out->st_mtime = (time_t)seconds;
    out->st_atime = out->st_mtime;
    out->st_ctime = out->st_mtime;
    FreeDosObject(DOS_FIB, fib);
    return 0;
}

int aovpn_amiga_access(const char *path, int mode)
{
    struct stat st;
    if (aovpn_amiga_stat(path, &st)) return -1;
    if (((mode & R_OK) && !(st.st_mode & S_IRUSR))
        || ((mode & W_OK) && !(st.st_mode & S_IWUSR))
        || ((mode & X_OK) && !(st.st_mode & S_IXUSR)))
    {
        errno = EACCES;
        return -1;
    }
    return 0;
}

int aovpn_amiga_unlink(const char *path)
{
    if (!path || !*path) { errno = EINVAL; return -1; }
    if (DeleteFile((STRPTR)path)) return 0;
    errno = aovpn_dos_error();
    return -1;
}
#endif /* TARGET_AMIGA */

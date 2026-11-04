/* Real Roadshow/SANA-II integration on an isolated demo stack, no Internet. */
#define AOVPN_GUEST_LOG "RunProg:roadshow-guest.log"
#include "../emulator/dos-entry.h"
#include <exec/errors.h>
#include <devices/sana2.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <proto/bsdsocket.h>
#include <libraries/bsdsocket.h>

struct Library *SocketBase;

#include "udp-probe.h"

int aovpn_probe_main(void)
{
    struct TagItem add_tags[] = {{IFA_PointToPoint, TRUE}, {IFA_IPType, 0x0021},
        {IFA_LimitMTU, 1400}, {TAG_DONE, 0}};
    struct TagItem config_tags[] = {{IFC_Address, (ULONG)"10.8.0.15"},
        {IFC_NetMask, (ULONG)"255.255.255.255"},
        {IFC_DestinationAddress, (ULONG)"10.8.0.1"}, {IFC_State, SM_Online}, {TAG_DONE, 0}};
    LONG result = 20;
    BOOL attached = FALSE;

    /* Only the test runner's private disposable volume is modified. */
    if (!AssignLock((STRPTR)"ENV", Lock((STRPTR)"RunProg:Env", ACCESS_READ)) ||
        !AssignLock((STRPTR)"ENVARC", Lock((STRPTR)"RunProg:Env", ACCESS_READ)) ||
        !AssignAdd((STRPTR)"LIBS", Lock((STRPTR)"RunProg:Libs", ACCESS_READ))) {
        puts("FAIL: isolated environment assigns");
        goto done;
    }
    SocketBase = OpenLibrary((STRPTR)"RunProg:Libs/bsdsocket.library", 4);
    if (!SocketBase) { puts("FAIL: Roadshow OpenLibrary"); goto done; }
    printf("Stack: %s\n", SocketBase->lib_IdString);
    if (AddInterfaceTagList((STRPTR)"ptp0", (STRPTR)"RunProg:vpn.device", 0, add_tags)) {
        printf("FAIL: AddInterface errno %ld\n", Errno()); goto done;
    }
    attached = TRUE;
    if (ConfigureInterfaceTagList((STRPTR)"ptp0", config_tags)) {
        printf("FAIL: ConfigureInterface errno %ld\n", Errno()); goto done;
    }
    puts("PASS: Roadshow configured point-to-point vpn.device unit 0");

    result = udp_roundtrip();
done:
    if (attached && !RemoveInterface((STRPTR)"ptp0", TRUE)) {
        printf("FAIL: Roadshow interface cleanup errno %ld\n", Errno()); result = 20;
    }
    if (SocketBase) CloseLibrary(SocketBase);
    if (!result) puts("PASS: Roadshow stack integration complete");
    return result;
}

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

static BOOL hookcopy(APTR to asm("a0"), APTR from asm("a1"), ULONG len asm("d0"))
{
    CopyMem(from, to, len);
    return TRUE;
}

int aovpn_probe_main(void)
{
    struct MsgPort *port = NULL;
    struct IOSana2Req *io = NULL;
    struct TagItem copy_tags[] = {{S2_CopyToBuff, (ULONG)hookcopy},
        {S2_CopyFromBuff, (ULONG)hookcopy}, {TAG_DONE, 0}};
    struct TagItem add_tags[] = {{IFA_PointToPoint, TRUE}, {IFA_IPType, 0x0021},
        {IFA_LimitMTU, 1400}, {TAG_DONE, 0}};
    struct TagItem config_tags[] = {{IFC_Address, (ULONG)"10.8.0.15"},
        {IFC_NetMask, (ULONG)"255.255.255.255"},
        {IFC_DestinationAddress, (ULONG)"10.8.0.1"}, {IFC_State, SM_Online}, {TAG_DONE, 0}};
    struct sockaddr_in peer = {0};
    static UBYTE payload[] = "AOVPN Roadshow UDP probe";
    UBYTE packet[256] = {0};
    LONG sock = -1;
    LONG result = 20;
    ULONG i, offset;
    BOOL attached = FALSE, pending = FALSE;

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

    port = CreateMsgPort();
    if (!port) { puts("FAIL: CreateMsgPort"); goto done; }
    io = (struct IOSana2Req *)CreateIORequest(port, sizeof(*io));
    if (!io) { puts("FAIL: CreateIORequest"); goto done; }
    io->ios2_BufferManagement = copy_tags;
    if (OpenDevice((STRPTR)"RunProg:vpn.device", 1, (struct IORequest *)io, 0)) {
        io->ios2_Req.io_Device = NULL;
        puts("FAIL: OpenDevice unit 1"); goto done;
    }
    io->ios2_Req.io_Command = S2_ONLINE;
    if (DoIO((struct IORequest *)io)) { puts("FAIL: S2_ONLINE"); goto done; }
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { puts("FAIL: UDP socket"); goto done; }
    peer.sin_family = AF_INET;
    peer.sin_port = 41194; /* target and network are both big endian */
    peer.sin_addr.s_addr = 0x0a080001;

    io->ios2_Req.io_Command = CMD_READ;
    io->ios2_PacketType = 0x0800;
    io->ios2_Data = packet;
    io->ios2_DataLength = sizeof(packet);
    SendIO((struct IORequest *)io);
    pending = TRUE;
    if (sendto(sock, payload, sizeof(payload), 0, (struct sockaddr *)&peer, sizeof(peer)) != sizeof(payload)) {
        printf("FAIL: UDP send errno %ld\n", Errno()); goto done;
    }
    for (i = 0; i < 100 && !CheckIO((struct IORequest *)io); ++i)
        Delay(1);
    if (!CheckIO((struct IORequest *)io)) { puts("FAIL: UDP packet timeout"); goto done; }
    pending = FALSE;
    if (WaitIO((struct IORequest *)io)) { puts("FAIL: UDP device read"); goto done; }
    offset = (packet[0] & 15) * 4 + 8;
    if (io->ios2_DataLength < 28 || (packet[0] >> 4) != 4 || packet[9] != 17 ||
        offset < 28 || io->ios2_DataLength != offset + sizeof(payload) ||
        packet[16] != 10 || packet[17] != 8 || packet[18] != 0 || packet[19] != 1) {
        puts("FAIL: IPv4/UDP packet metadata"); goto done;
    }
    for (i = 0; i < sizeof(payload); ++i)
        if (packet[offset + i] != payload[i]) { puts("FAIL: UDP payload"); goto done; }
    puts("PASS: Roadshow UDP socket traversed vpn.device to unit 1");
    result = 0;
done:
    if (pending) { AbortIO((struct IORequest *)io); WaitIO((struct IORequest *)io); }
    if (sock >= 0) CloseSocket(sock);
    if (io) {
        if (io->ios2_Req.io_Device) CloseDevice((struct IORequest *)io);
        DeleteIORequest((struct IORequest *)io);
    }
    if (port) DeleteMsgPort(port);
    if (attached && !RemoveInterface((STRPTR)"ptp0", TRUE)) {
        printf("FAIL: Roadshow interface cleanup errno %ld\n", Errno()); result = 20;
    }
    if (SocketBase) CloseLibrary(SocketBase);
    if (!result) puts("PASS: Roadshow stack integration complete");
    return result;
}

/* Real UDP socket roundtrip through both units of the shared SANA-II device. */
#include <exec/errors.h>
#include <devices/sana2.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <proto/bsdsocket.h>

static BOOL udp_copy(APTR to asm("a0"), APTR from asm("a1"), ULONG len asm("d0"))
{
    CopyMem(from, to, len);
    return TRUE;
}

static int udp_roundtrip(void)
{
    struct MsgPort *port = NULL;
    struct IOSana2Req *io = NULL;
    struct TagItem copy_tags[] = {{S2_CopyToBuff, (ULONG)udp_copy},
        {S2_CopyFromBuff, (ULONG)udp_copy}, {TAG_DONE, 0}};
    struct sockaddr_in peer = {0};
    struct timeval timeout;
    fd_set ready;
    ULONG signals = 0;
    static UBYTE payload[] = "AOVPN UDP stack probe";
    UBYTE packet[256] = {0}, reply[sizeof(payload)];
    LONG sock = -1;
    ULONG i, header, length;
    int result = 20;
    BOOL pending = FALSE;

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
    if (sock < 0 || sock >= FD_SETSIZE) { puts("FAIL: UDP socket"); goto done; }
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
    for (i = 0; i < 100 && !CheckIO((struct IORequest *)io); ++i) Delay(1);
    if (!CheckIO((struct IORequest *)io)) { puts("FAIL: UDP packet timeout"); goto done; }
    pending = FALSE;
    if (WaitIO((struct IORequest *)io)) { puts("FAIL: UDP device read"); goto done; }
    header = (packet[0] & 15) * 4;
    length = io->ios2_DataLength;
    if (length < 28 || (packet[0] >> 4) != 4 || packet[9] != 17 || header < 20 ||
        length != header + 8 + sizeof(payload) || packet[16] != 10 ||
        packet[17] != 8 || packet[18] != 0 || packet[19] != 1) {
        puts("FAIL: IPv4/UDP packet metadata"); goto done;
    }
    for (i = 0; i < sizeof(payload); ++i)
        if (packet[header + 8 + i] != payload[i]) { puts("FAIL: UDP payload"); goto done; }
    /* Swap endpoint address/port words to make the reply. IPv4 and UDP one's
     * complement checksums are invariant under these word permutations. */
    for (i = 0; i < 4; ++i) { UBYTE t = packet[12+i]; packet[12+i] = packet[16+i]; packet[16+i] = t; }
    for (i = 0; i < 2; ++i) { UBYTE t = packet[header+i]; packet[header+i] = packet[header+2+i]; packet[header+2+i] = t; }
    io->ios2_Req.io_Command = CMD_WRITE;
    io->ios2_DataLength = length;
    if (DoIO((struct IORequest *)io)) { puts("FAIL: UDP reply injection"); goto done; }
    for (i = 0; i < sizeof(ready); ++i) ((UBYTE *)&ready)[i] = 0;
    timeout.tv_sec = 2; timeout.tv_usec = 0;
    FD_SET(sock, &ready);
    if (WaitSelect(sock + 1, &ready, NULL, NULL, &timeout, &signals) != 1 ||
        recv(sock, reply, sizeof(reply), 0) != sizeof(reply)) {
        puts("FAIL: UDP reply receive"); goto done;
    }
    for (i = 0; i < sizeof(reply); ++i)
        if (reply[i] != payload[i]) { puts("FAIL: UDP reply payload"); goto done; }
    puts("PASS: UDP socket roundtrip through both vpn.device units");
    result = 0;
done:
    if (pending) { AbortIO((struct IORequest *)io); WaitIO((struct IORequest *)io); }
    if (sock >= 0) CloseSocket(sock);
    if (io) {
        if (io->ios2_Req.io_Device) CloseDevice((struct IORequest *)io);
        DeleteIORequest((struct IORequest *)io);
    }
    if (port) DeleteMsgPort(port);
    return result;
}

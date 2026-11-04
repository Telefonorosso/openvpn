/* Real OpenVPN authentication/data path and production Miami lifecycle hooks. */
#define AOVPN_GUEST_LOG "RunProg:miami-vpn-guest.log"
#include "miami-control.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <proto/bsdsocket.h>
#include <amitcp/socketbasetags.h>
static LONG socket_error, resolver_error;

static struct Library *open_sockets(void)
{
    SocketBase = OpenLibrary((STRPTR)"bsdsocket.library", 4);
    if (SocketBase) SocketBaseTags(SBTM_SETVAL(SBTC_ERRNOPTR(sizeof(socket_error))), (ULONG)&socket_error,
        SBTM_SETVAL(SBTC_HERRNOLONGPTR), (ULONG)&resolver_error, TAG_END);
    return SocketBase;
}

static LONG shell(const char *text)
{
    LONG rc = SystemTags((STRPTR)text, NP_StackSize, 262144, TAG_DONE);
    printf("Command %s returned %ld\n", text, rc);
    return rc;
}

static BOOL launch(const char *text, const char *log)
{
    BPTR input = Open((STRPTR)"NIL:", MODE_OLDFILE);
    BPTR output = Open((STRPTR)log, MODE_NEWFILE);
    LONG rc;
    if (!input || !output) {
        if (input) Close(input);
        if (output) Close(output);
        return FALSE;
    }
    rc = SystemTags((STRPTR)text, SYS_Asynch, TRUE, SYS_Input, input, SYS_Output, output, NP_StackSize, 262144, TAG_DONE);
    if (rc == -1) { Close(input); Close(output); return FALSE; }
    return TRUE;
}

static BOOL contains(const char *file, const char *needle)
{
    UBYTE data[512];
    ULONG matched = 0, i;
    LONG n;
    BOOL found = FALSE;
    BPTR handle = Open((STRPTR)file, MODE_OLDFILE);
    if (!handle) return FALSE;
    while (!found && (n = Read(handle, data, sizeof(data))) > 0) {
        for (i = 0; i < (ULONG)n; ++i) {
            if (data[i] == needle[matched]) ++matched;
            else matched = data[i] == needle[0] ? 1 : 0;
            if (!needle[matched]) { found = TRUE; break; }
        }
    }
    Close(handle);
    return found;
}

static BOOL save_client_log(const char *target);

static BOOL wait_log(const char *needle)
{
    ULONG i;
    for (i = 0; i < 3000; ++i) {
        if (i % 50 == 0) save_client_log("RunProg:client-live.log");
        if (contains("RAM:openvpn.log", needle)) return TRUE;
        if (contains("RAM:openvpn.log", "Options error:") || contains("RAM:openvpn.log", "Exiting due to fatal error") ||
            contains("RAM:openvpn.log", "VERIFY ERROR:") || contains("RAM:openvpn.log", "TLS Error: TLS handshake failed")) {
            puts("FAIL: client reported a fatal startup error"); return FALSE;
        }
        Delay(5);
    }
    printf("FAIL: client did not log %s\n", needle);
    return FALSE;
}

static BOOL save_client_log(const char *target)
{
    UBYTE data[1024];
    LONG n;
    BOOL ok = TRUE;
    BPTR input = Open((STRPTR)"RAM:openvpn.log", MODE_OLDFILE), output;
    if (!input) return FALSE;
    output = Open((STRPTR)target, MODE_NEWFILE);
    if (!output) { Close(input); return FALSE; }
    while ((n = Read(input, data, sizeof(data))) > 0)
        if (Write(output, data, n) != n) { ok = FALSE; break; }
    if (n < 0) ok = FALSE;
    if (!Close(output)) ok = FALSE;
    Close(input);
    return ok;
}

static BOOL encrypted_echo(void)
{
    struct sockaddr_in peer = {0};
    struct timeval timeout;
    fd_set ready;
    ULONG i, signals = 0;
    static char payload[] = "Miami real encrypted VPN echo";
    char reply[sizeof(payload)];
    LONG sock = socket(AF_INET, SOCK_DGRAM, 0);
    BOOL ok = FALSE;
    if (sock < 0 || sock >= FD_SETSIZE) goto done;
    peer.sin_family = AF_INET;
    peer.sin_port = 41195;
    peer.sin_addr.s_addr = 0x0a080001;
    if (sendto(sock, payload, sizeof(payload), 0, (struct sockaddr *)&peer, sizeof(peer)) != sizeof(payload)) goto done;
    for (i = 0; i < sizeof(ready); ++i) ((UBYTE *)&ready)[i] = 0;
    timeout.tv_sec = 5; timeout.tv_usec = 0;
    FD_SET(sock, &ready);
    if (WaitSelect(sock + 1, &ready, NULL, NULL, &timeout, &signals) != 1 ||
        recv(sock, reply, sizeof(reply), 0) != sizeof(reply)) goto done;
    for (i = 0; i < sizeof(reply); ++i) if (reply[i] != payload[i]) goto done;
    puts("PASS: application UDP reply through encrypted OpenVPN session");
    ok = TRUE;
done:
    if (sock >= 0) CloseSocket(sock);
    if (!ok) puts("FAIL: encrypted UDP echo");
    return ok;
}

static BOOL dns_answer(const char *name, UBYTE last)
{
    struct hostent *entry = gethostbyname((STRPTR)name);
    UBYTE *address;
    if (!entry || entry->h_addrtype != AF_INET || entry->h_length != 4 || !entry->h_addr_list[0]) {
        printf("FAIL: DNS resolution errno=%ld h_errno=%ld\n", socket_error, resolver_error); return FALSE;
    }
    address = (UBYTE *)entry->h_addr_list[0];
    printf("DNS %s returned %ld.%ld.%ld.%ld\n", name, (LONG)address[0], (LONG)address[1], (LONG)address[2], (LONG)address[3]);
    if (address[0] != 10 || address[1] != (last == 1 ? 8 : 0) || address[2] != (last == 1 ? 0 : 2) || address[3] != last) {
        puts("FAIL: pushed/restored DNS selection"); return FALSE;
    }
    return TRUE;
}

int aovpn_probe_main(void)
{
    ULONG i, cycle;
    LONG result = 20, rc;
    BOOL started = FALSE;
    if (!assign_dir("T", "RunProg:Tmp") || !assign_dir("ENV", "RunProg:Env") || !assign_dir("ENVARC", "RunProg:Env") ||
        !assign_dir("DEVS", "RunProg:Devs") || !assign_dir("S", "RunProg:Scripts") ||
        !assign_dir("MIAMI", "RunProg:") || !assign_dir("LIBS", "RunProg:Runtime/Libs") ||
        !assign_dir("FONTS", "RunProg:Runtime/Fonts") || !assign_dir("C", "RunProg:Runtime/C") ||
        !assign_dir("REXX", "RunProg:Runtime/Rexxc") || !assign_dir("AmiSSL", "RunProg:Runtime/SSL") ||
        !assign_dir("MUI", "RunProg:Runtime/System/MUI") || !assign_dir("OPENVPN", "RunProg:OpenVPN")) {
        puts("FAIL: isolated runtime assigns"); goto done;
    }
    if (shell("C:Execute RunProg:set-clock.script")) goto done;
    RexxSysBase = (struct RxsLib *)OpenLibrary((STRPTR)"rexxsyslib.library", 0);
    if (!RexxSysBase) { puts("FAIL: rexxsyslib.library"); goto done; }
    if (!launch("RunProg:MiamiDx NOGUI DONTCONNECT SETTINGS=RunProg:openvpn.miami", "RunProg:miami-child.log")) { puts("FAIL: Miami process creation"); goto done; }
    started = TRUE;
    for (i = 0; i < 500 && !FindPort((STRPTR)"MIAMI.1"); ++i) Delay(1);
    if (!FindPort((STRPTR)"MIAMI.1")) { puts("FAIL: Miami startup timeout"); goto done; }
    if (!launch("REXX:RexxMast", "RunProg:rexx.log")) { puts("FAIL: RexxMast process creation"); goto done; }
    for (i = 0; i < 500 && !FindPort((STRPTR)"REXX"); ++i) Delay(1);
    if (!FindPort((STRPTR)"REXX")) { puts("FAIL: RexxMast startup timeout"); goto done; }
    for (i = 0; i < 500; ++i) {
        rc = command("ISONLINE eth0");
        if (rc == 0 || rc == 1) break;
        Delay(1);
    }
    shell("MIAMI:MiamiIfConfig -a >RunProg:interfaces-start.log");
    rc = command("ONLINE eth0");
    if ((rc != 0 && rc != 5) || command("ISONLINE eth0") != 1) { puts("FAIL: physical ONLINE"); goto done; }
    if (shell("MIAMI:MiamiIfConfig eth0 inet 10.0.2.15 netmask 255.255.255.0")) goto done;
    shell("MIAMI:MiamiRoute -n delete default");
    if (shell("MIAMI:MiamiRoute -n add default 10.0.2.2")) goto done;
    shell("MIAMI:MiamiNetStat -rn >RunProg:routes-before.log");
    SocketBase = open_sockets();
    if (!SocketBase || !dns_answer("before.vpn-ci.invalid", 2)) goto done;
    CloseLibrary(SocketBase); SocketBase = NULL;
    for (cycle = 0; cycle < 2; ++cycle) {
        if (!DeleteFile((STRPTR)"RAM:openvpn.log") && IoErr() != ERROR_OBJECT_NOT_FOUND) {
            puts("FAIL: cannot clear previous client log"); goto done;
        }
        rc = command("ONLINE ptp0");
        if ((rc != 0 && rc != 5) || !wait_online(1)) goto done;
        if (!wait_log("Initialization Sequence Completed")) goto done;
        if (!contains("RAM:openvpn.log", "[UP] END") || !contains("RAM:openvpn.log", "[ROUTE-UP] END")) {
            puts("FAIL: production up/route-up hooks"); goto done;
        }
        SocketBase = open_sockets();
        if (!SocketBase) { puts("FAIL: socket library"); goto done; }
        if (!encrypted_echo() || !dns_answer(cycle ? "during2.vpn-ci.invalid" : "during1.vpn-ci.invalid", 1)) goto done;
        shell(cycle ? "MIAMI:MiamiNetStat -rn >RunProg:routes-connected-2.log" : "MIAMI:MiamiNetStat -rn >RunProg:routes-connected-1.log");
        CloseLibrary(SocketBase); SocketBase = NULL;
        rc = command("OFFLINE ptp0");
        if ((rc != 0 && rc != 5) || !wait_online(0) || !wait_log("process exiting") ||
            !contains("RAM:openvpn.log", "[PRE-DOWN] END")) { puts("FAIL: production disconnect hooks"); goto done; }
        if (!save_client_log(cycle ? "RunProg:client-2.log" : "RunProg:client-1.log")) goto done;
        shell(cycle ? "MIAMI:MiamiNetStat -rn >RunProg:routes-after-2.log" : "MIAMI:MiamiNetStat -rn >RunProg:routes-after-1.log");
        SocketBase = open_sockets();
        if (!SocketBase || !dns_answer(cycle ? "after2.vpn-ci.invalid" : "after1.vpn-ci.invalid", 2)) goto done;
        CloseLibrary(SocketBase); SocketBase = NULL;
    }
    result = 0;
done:
    save_client_log("RunProg:client-final.log");
    if (SocketBase) { CloseLibrary(SocketBase); SocketBase = NULL; }
    if (started && FindPort((STRPTR)"MIAMI.1")) {
        command("OFFLINE ptp0");
        command("OFFLINE eth0");
        rc = command("QUIT"); if (rc) result = 20;
        for (i = 0; i < 500 && FindPort((STRPTR)"MIAMI.1"); ++i) Delay(1);
        if (FindPort((STRPTR)"MIAMI.1")) result = 20;
    }
    if (FindPort((STRPTR)"REXX")) {
        shell("C:rxc");
        for (i = 0; i < 500 && FindPort((STRPTR)"REXX"); ++i) Delay(1);
        if (FindPort((STRPTR)"REXX")) result = 20;
    }
    puts("Cleanup: closing ARexx library");
    if (RexxSysBase) CloseLibrary((struct Library *)RexxSysBase);
    /* Restore the launcher commands so its Done helper records our exit. */
    if (!assign_dir("C", "RunBoot:C")) result = 20;
    if (!result) puts("PASS: Miami encrypted VPN and production reconnect complete");
    printf("Guest returning %ld\n", result);
    return result;
}

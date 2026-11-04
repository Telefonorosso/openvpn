# POC3 FIX5 — Numeric IPv4 diagnostic formatting (experimental)

The latest hardware log passes FIX4 numeric `getaddrinfo()` and socket phase 1. It stops after `R00` during socket phase 2, which reuses and **prints** a previously resolved remote address. That print path calls `print_sockaddr_ex()` -> `getnameinfo()` even for a numeric IPv4 host; the latter is not yet validated on this AmigaOS port.

FIX5 changes **only** `src/openvpn/socket.c` on top of FIX4 + TRACE1/2/3. For `AF_INET`, `print_sockaddr_ex()` obtains four network-order bytes and the port from the existing `sockaddr_in`, using `snprintf` instead of `getnameinfo`. It leaves IPv6 unchanged and adds R09-R11 markers around the phase-2 preserved-address formatting path. This is **not** a proven fix for Guru 80000004, which might have another cause.

Extract the ZIP into the isolated OpenVPN source root with FIX4 already installed:

```sh
cd /root/openvpn/fix27-clean/openvpn-2.6.23
unzip -o /root/openvpn/AmiOpenVPN-POC3-IPV4-NAMEINFO-FIX5.zip
bash ./apply_ipv4_nameinfo_fix5.sh
```

The script verifies exact SHA256, performs `patch --dry-run` and preserves `.IPV4FIX4_BACKUP`. Never force it on a differing local tree.

Then cross-compile with the existing validated build script:

```sh
cd /root/openvpn/fix27-clean
AMISSL_SDK=/root/openvpn/AmiSSL/Developer \
./AmiOpenVPN-2.6.23-68K-NATIVE-STARTUP-HARDWARE-VALIDATED-MILESTONE/tools/build_milestone.sh ./openvpn-2.6.23
```

Install the new executable under a separate name (`AmiOpenVPN-FIX5`). Keep `dev null`, `proto udp4`, numeric `remote`, `verb 3`, `tmp-dir T:`. Only MiamiDX Ethernet needs to be up. Look for `R09`, `R10`, `V01`, `R11`, `R08`, then `N13`. The presence of `N13` means phase 2 completed, not that TLS has been authenticated.

Rollback: after ensuring the correct isolated source tree, restore `src/openvpn/socket.c.IPV4FIX4_BACKUP` to `src/openvpn/socket.c` and rebuild. This patch does not modify AmiSSL or `vpn.device`.

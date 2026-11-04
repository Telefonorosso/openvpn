# AmiOpenVPN POC3 STARTUP TRACE3 — FIX27 + TRACE2 only

TRACE2 on hardware reached `N09 frame and buffers ready; before socket phase1`, then AmigaOS Guru `80000004`. TRACE3 adds **diagnostic markers only** to `src/openvpn/socket.c` around `link_socket_init_phase1()`, `resolve_remote()`, `get_cached_dns_entry()`, `openvpn_getaddrinfo()` and `getaddrinfo()`.

- No changes to network behavior, address family, ABI, AmiSSL, or `vpn.device`.
- Requires TRACE2 already applied to `src/openvpn/init.c`.
- Requires unmodified **FIX27** `src/openvpn/socket.c` SHA-256: `d3cfbbf253d147b0368910307bada96458e27b73e46cf7792026b52f61c085e6`.
- Patch rejects other source hashes and never runs `patch --force`.
- This is **NOT** a validated solution for the Guru; it only narrows the failure.

## Build on Debian

```sh
cd /root/openvpn/fix27-clean/openvpn-2.6.23
unzip -o /root/openvpn/AmiOpenVPN-POC3-STARTUP-TRACE3-FIX27-ONLY.zip
bash ./apply_trace3.sh
cd /root/openvpn/fix27-clean
AMISSL_SDK=/root/openvpn/AmiSSL/Developer \
./AmiOpenVPN-2.6.23-68K-NATIVE-STARTUP-HARDWARE-VALIDATED-MILESTONE/tools/build_milestone.sh \
./openvpn-2.6.23
```

Copy `openvpn-2.6.23/aovpn-build/src/openvpn/openvpn` to the Amiga as `AmiOpenVPN-TRACE3`, without overwriting FIX27 or TRACE2.

## Test

```text
Stack 262144
AmiOpenVPN-TRACE3 --config client.ovpn
```

Retain `tmp-dir T:` and `verb 3`.

## Interpretation

- Last marker `S02`: trouble entering `resolve_remote()`.
- Last `R01`: trouble inside DNS cache lookup.
- Last `G01`: trouble invoking numeric `getaddrinfo()`.
- Last `G02` or later: `getaddrinfo()` returned; inspect following markers and diagnostics.
- `S04` seen: phase 1 is complete, so investigate the subsequent TUN logic instead.

Report last marker and Guru code. Do not change DNS/IPv6 behavior until this is localized.

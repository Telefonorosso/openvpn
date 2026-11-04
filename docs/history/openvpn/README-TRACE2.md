# AmiOpenVPN POC3 STARTUP TRACE2 — diagnostic-only

This archive is a **small context patch**, not an overlay. Do not run any old POC3 apply_overlay.py script.

It expects FIX27 OpenVPN 2.6.23 source tree with **TRACE1 already applied**; `init.c` must be untouched FIX27.
The installer checks exact SHA-256 hashes and refuses unknown local changes. Never force or ignore a check failure.

## How to apply

Copy this ZIP into your isolated FIX27 source root (example `/root/openvpn/fix27-clean/openvpn-2.6.23`):

```sh
unzip -o AmiOpenVPN-POC3-STARTUP-TRACE2-FIX27-ONLY.zip
bash ./apply_trace2.sh
```

Backups: `src/openvpn/openvpn.c.TRACE1_BAK` and `src/openvpn/init.c.FIX27_BAK`.

## Build (from `/root/openvpn/fix27-clean`)

```sh
AMISSL_SDK=/root/openvpn/AmiSSL/Developer \
./AmiOpenVPN-2.6.23-68K-NATIVE-STARTUP-HARDWARE-VALIDATED-MILESTONE/tools/build_milestone.sh \
./openvpn-2.6.23
```

Install as `AmiOpenVPN-TRACE2`, preserving previous binaries. Keep `verb 3` and `tmp-dir T:` in `client.ovpn`.

## Read the log

Trace markers `O28`, `P00`–`P04`, and `N00`–`N19` identify the initialization phase:
- `P02`, `N01` — entering instance initialization
- `N07` — starting crypto/TLS initialization
- `N08` — crypto initialized
- `N10` — socket phase1 reached, TUN next
- `N13` — socket phase2 finished
- `P04` — main tunnel event loop reached

These are **execution markers**, not proof of handshake. We have not cross-compiled this diagnostic revision and it has not been run on AmigaOS.

## Expected hashes before application

- `src/openvpn/openvpn.c` (FIX27 + TRACE1): `f40dc402907fc71d46b60a45498f18bac86708e5ab92045354e8a66e01a165b7`
- `src/openvpn/init.c` (FIX27): `d200fa76a570de21857224058e059c2ef640c9dcdffcad9690e2f6e818cae84e`

## Expected hashes after application

- `src/openvpn/openvpn.c`: `40b436646931a9407f6a45be99e4812c5569b048eb217f6e135e01b5a9b30820`
- `src/openvpn/init.c`: `4f94cea339abe8ebed5988983557d9e71e4770acf633fad5e9a33cde02fb1f16`

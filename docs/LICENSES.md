# Licenses and third-party materials

- `src/openvpn/` is derived from OpenVPN 2.6.23. Its source tree keeps
  `COPYING`, `COPYRIGHT.GPL` and upstream license/copyright notices. OpenVPN
  is GPL version 2, with the exceptions detailed in the bundled `COPYING`.
  These apply to redistribution of the modified OpenVPN code and binaries.
- `src/vpn-device/` includes independently written `vpn.device` code marked
  BSD-2-Clause and reused helpers from `sana2loop` by Simon Dick. The exact
  third-party notices and original BSD-2-Clause license are retained in
  `src/vpn-device/THIRD-PARTY-NOTICES.md` and
  `src/vpn-device/third_party/sana2loop/LICENSE`.
- The source trees contain additional original copyright statements, which
  remain applicable. This packaging documentation adds no new licensing terms.
- AmiSSL v5 Developer SDK, AmigaOS NDK, SANA-II header packages and cross
  compiler are required external dependencies and **are not included**.

Do not remove upstream notices or assume all parts of this combined release
have a single license. This notice is a summary, not legal advice.

# File locations after the repository reorganization

The source repository layout is separate from the Amiga installation layout.
Neither building nor extracting a package installs anything onto an Amiga.

| Repository file | Amiga location / use |
| --- | --- |
| `prebuilt/beta2/openvpn` | The existing client, e.g. `OPENVPN:openvpn` |
| `prebuilt/beta2/vpn.device` | The existing shared driver, `DEVS:Networks/vpn.device` |
| `stacks/aminetxduo/*.script` and `*.rexx` | AmiNetXDuo hook scripts in the existing OpenVPN directory |
| `prebuilt/beta2/anxdns` | AmiNetXDuo DNS helper, alongside the hook scripts |
| `stacks/amitcp-ng/*.script` and `*.rexx` | AmiTCP-NG hook scripts in the existing OpenVPN directory |
| `stacks/amitcp-ng/NetInterfaces/ptp0` | Existing example interface file for `DEVS:NetInterfaces/ptp0` |
| `prebuilt/beta2/amitcpresolver` | AmiTCP-NG DNS helper, alongside the hook scripts |
| `stacks/miamidx/` | Existing MiamiDX scripts, preferences and icon; consult its README |
| `prebuilt/beta2/RequestString` | Existing utility used by the MiamiDX scripts |
| `examples/` | Existing example profiles/configuration; adapt to the actual VPN |

Keep the original hook filenames and configured `OPENVPN:` assign. The scripts
have not been rewritten for the repository paths. Select the files for the
installed TCP/IP stack; do not overlay the three sets of hooks on one another.
Review the existing `vpn.startup` before using it on a configured machine.

The original root README's requirements, configuration and run instructions
remain applicable. These are historical BETA2 binaries, not newly verified
builds of the reorganized source. See `prebuilt/beta2/README.md` and
`docs/REPOSITORY.md` for provenance; use `docs/BUILD.md` for current build commands.

## Newly built CMake packages

`bin/` contains the rebuilt client/DNS helpers and the historical RequestString
utility; `Devs/Networks/vpn.device` is freshly built. Copy these to the corresponding
Amiga locations above. Select **one** integration under `stacks/` and preserve
its original filenames and assigns. `examples/` contains templates; `docs/`
contains notices, version and changelog. There is no automatic installer and no
runtime AmiSSL library in the package: install the supported AmiSSL v5 runtime
separately. Back up working configuration before changing a live installation.
A package and passing smoke tests alone do not verify full VPN interoperability.

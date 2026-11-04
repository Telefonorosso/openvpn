# AmiOpenVPN package

This archive contains one rebuilt AmigaOS 68k OpenVPN client, the shared
SANA-II driver, DNS helpers and the supported stack integration files.

| Path | Use |
| --- | --- |
| `bin/openvpn` | Client; copy into the configured `OPENVPN:` directory |
| `Devs/Networks/vpn.device` | Shared driver; copy to `DEVS:Networks/` |
| `bin/anxdns`, `bin/amitcpresolver` | Select the DNS helper for your stack |
| `bin/RequestString` | Unchanged historical utility for the Miami scripts |
| `stacks/` | Select one stack's hooks and configuration |
| `examples/` | Templates; supply your own server and credentials |
| `docs/VERSION`, `docs/CHANGELOG.md` | Port revision and change history |

Read [installation locations](INSTALL.md) and [license notices](LICENSES.md).
AmiSSL v5 is an external runtime and is not included. Keep the original script
filenames and configured assigns. Preserve working settings before installing.
Extracting this archive does not automatically install or start anything.

The GitHub `release-candidate` artifact also contains `SOURCE_COMMIT`, source
and dependency provenance and `SHA256SUMS` for the inner product ZIP/tar.gz.
Use only the candidate for the intended tested commit/version. A source archive
or dirty local tree is marked unverified and is not an exact-commit release.

Source, maintainer instructions, test coverage and releases are in
[Telefonorosso/openvpn](https://github.com/Telefonorosso/openvpn).
Candidates become releases only after the maintainer reviews and publishes
them. Passing the smoke checks is not qualification of every VPN provider,
protocol or hardware configuration.

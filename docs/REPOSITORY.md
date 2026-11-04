# Repository guide

## One project, one driver

`Telefonorosso/openvpn` remains the project upstream. The same `openvpn` and
`vpn.device` implementations serve the supported TCP/IP stacks; integration
scripts and DNS helpers are the parts separated by stack.

`src/openvpn/` tracks the official OpenVPN source; `src/vpn-device/` contains
the Amiga SANA-II bridge. Both retain their original copyright and license
files. `docs/LICENSES.md` gives the original source bundle's summary.

`stacks/<stack>/` is a source packaging layout, not a destination assign. Copy
the selected hook scripts to the location expected by the existing profile,
and install device/interface configuration as documented by that stack.
The scripts themselves are byte-for-byte copies of the existing BETA2 files.
`RequestString`, when required by the MiamiDX scripts, is in `prebuilt/beta2/`.

## Provenance

- Repository state before reorganization: `22fbaba4399b8cda980d969c212af4f91a902272`.
- Official OpenVPN tag: `v2.6.23`, commit `49620843c487a7fee58e3030b7c88681c115e24e`.
- Amiga port source: the existing source-bundle import `5a9287b5a3e7062d2ba2ef22ca363cc1a4d6a254` in
  `tinic/openvpn`. Its ZIP blob matches the ZIP published in this repository:
  `1f4a80435c4ab2a4cf9d95f8fdcff465747052f0` (Git blob SHA-1).
- Stack scripts and DNS helpers: copied from this repository's current main,
  including the latest `BETA2-AMINETXDUO` directory, without edits.

The original source archive remains available at its immutable historical URL:
[original source bundle](https://github.com/Telefonorosso/openvpn/blob/22fbaba4399b8cda980d969c212af4f91a902272/AmiOpenVPN-2.6.23-GitHub-Source-Bundle.zip).
Active source files are tracked directly now. Old backup copies are available
in that archive rather than presented beside the active implementations.
Historical standalone patches/notes are retained in `docs/history/openvpn/`;
they have already been applied where applicable and are not build steps.

The source import contains generated Autotools files from the release bundle.
Retaining those makes this first change structural. A later build-tool commit
can define regeneration and the pinned GCC/NDK/AmiSSL SDK inputs explicitly.

## Builds, releases and validation

Next steps are a documented cross-build entry point, pinned build dependencies,
then CI-produced archives and release checksums. Tag builds should produce one
shared client/driver plus the selected stack integration files. Existing binary
downloads stay available in `prebuilt/beta2/` during that transition.

This reorganization does not claim a new binary build, emulator pass or hardware
pass. Existing upstream/driver test sources were retained unchanged; no new
harness or benchmark was added. IPv6 support and the existing GCC 16 build
adjustments remain separate proposed changes, so they can be reviewed on their
own merits.

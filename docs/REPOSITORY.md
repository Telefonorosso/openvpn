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

The source import retains generated Autotools files from the release bundle.
The root CMake build compiles the active sources directly without regenerating
those files. See `docs/BUILD.md` for pinned dependencies and runnable commands,
and `AGENTS.md` for development, validation and release rules.

## Builds, releases and validation

CI runs Linux/macOS host regressions, an AmigaPorts GCC cross-build and bounded
Copperline/AROS guest FIFO and SANA-II smoke tests, plus a real Roadshow demo UDP/driver check.
Miami-specific runtime coverage is pending; see `tests/stacks/README.md`. Packages contain one rebuilt
shared client/driver, DNS helpers and all stack integrations, plus the unchanged
historical RequestString utility. `prebuilt/beta2/` remains historical evidence.
The initial import/layout commits preserve active source bytes. The later build
commit additionally carries the existing GCC 16 errno-pointer compatibility
fix and a configurable diagnostic device path; it is not purely structural.

The new smoke tests are not full VPN interoperability evidence. Existing README
stack validation claims describe the previous port, not automatic validation of
every newly built artifact. Separate IPv6 proposals remain outside this change.
`VERSION` identifies the Amiga revision; `CHANGELOG.md` records unreleased work.

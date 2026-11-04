# Changelog

## 2.6.23-amiga.1 — unreleased

- Import official OpenVPN v2.6.23 as a Git subtree and retain the existing
  Amiga port, driver, helpers and stack integration scripts.
- Separate active source, per-stack integrations, examples and historical
  BETA2 binaries, with licenses and provenance retained.
- Add direct CMake builds using pinned AmigaPorts GCC and AmiSSL SDK inputs,
  including the existing GCC 16 errno-pointer compatibility adjustment.
- Add Linux/macOS host regressions, a Linux cross-build, Copperline/AROS
  FIFO and SANA-II guest smoke tests, and checked draft-release packaging.
- Add real Roadshow and MiamiDX UDP roundtrips through both SANA-II device
  units, plus Miami OFFLINE/ONLINE and shutdown checks on a licensed runtime.
  Add a registered private lane for encrypted client traffic, production
  route/DNS hooks and reconnect against an isolated local server. Keep other
  stacks' end-to-end gaps and failing AROS experiments explicit.
- Add checksum-pinned volamos target FIFO sanitizer/dirty-heap checks on CI.
- Add a tested one-command build/package entrypoint, explicit clean/dirty/archive
  source identity, and maintainer/agent handoff and release instructions.
- Document development, upstream updates, versioning and test coverage limits.

This is an unreleased port revision, not a new OpenVPN upstream version.
Historical `prebuilt/beta2` downloads retain their original bytes and identity.
The smoke checks do not establish full encrypted VPN interoperability.

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
- Document development, upstream updates, versioning and test coverage limits.

This is an unreleased port revision, not a new OpenVPN upstream version.
Historical `prebuilt/beta2` downloads retain their original bytes and identity.
The smoke checks do not establish full encrypted VPN interoperability.

# Existing BETA2 binaries

These files are unchanged copies of the binaries distributed before the
repository reorganization (`22fbaba4399b8cda980d969c212af4f91a902272`). Identical copies from
different stack directories were deduplicated by Git blob identity.

- `openvpn`: shared client, previously repeated in all three stack directories.
- `vpn.device`: shared driver, previously repeated in AmiTCP-NG and MiamiDX.
- `anxdns`, `amitcpresolver`: the existing stack-specific DNS helpers.
- `RequestString`: the existing MiamiDX script utility.

`SHA256SUMS` identifies the exact retained binary bytes. These are historical
downloads, not outputs verified against the newly browsable source tree.
The original bundle documents different client/driver source milestones; a
matching version string does not establish binary reproducibility. A future
reviewed build/release workflow will produce binaries with exact source,
compiler and dependency provenance.

Install these using the existing stack instructions. The source repository's
directory layout is not an Amiga filesystem layout.

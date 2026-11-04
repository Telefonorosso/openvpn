# Agent guide

## Project and scope

This is Francesco's AmigaOS 68k OpenVPN port. `Telefonorosso/openvpn` is the
project upstream; contributor forks are review lanes, not separate official
client/driver implementations. Read `README.md`, `docs/REPOSITORY.md`,
`docs/BUILD.md`, `docs/UPSTREAM.md` and the stack-specific README before editing.
Check working-tree status and applicable parent instructions first. Preserve
other people's changes, credentials, configuration and unpublished work.

## Source layout and upstream tracking

- `src/openvpn/`: official OpenVPN subtree with the existing Amiga adaptations.
- `src/vpn-device/`: shared SANA-II device, existing regressions and diagnostics.
- `src/helpers/`: DNS helpers; `stacks/<stack>/`: integration policy and scripts.
- `cmake/`, `ci/`, `tools/`, `tests/`: this port's build and validation layer.
- `examples/`: templates, never live secrets. `prebuilt/beta2/`: immutable legacy
  binaries with checksums; do not overwrite them with new build outputs.
- `docs/history/`: provenance, not patches to apply during a normal build.

Use the squashed subtree workflow in `docs/UPSTREAM.md` for upstream releases;
never replace active source with another ZIP. Record the exact upstream tag
and commit, preserve Amiga fixes when resolving conflicts, and keep portable
fixes separate from platform adaptations. Prefer integration-layer changes
outside the subtree where possible. Keep upstream-compatible fixes small and
independently testable for eventual export. Do not submit upstream, merge,
push release tags or publish releases without maintainer authorization.

## Clean, repeatable builds

Use out-of-source `build/` directories. Never generate configuration, objects,
SDKs, ROMs or packages inside active source directories. Do not commit build
outputs or untracked copies of upstream code. Keep paths portable; no developer
machine paths, credentials or proprietary ROMs in source or workflows.

Install CMake 3.24+, Python 3.12+, a native C compiler/build tool, and an LHA
reader (`lhasa` on Linux, `lha` on macOS). Run:

```sh
python3 tools/check-version.py
python3 tools/fetch-deps.py
```

Set `AMIGA_GCC_ROOT` and `AMISSL_SDK` to the printed paths, then:

```sh
cmake -S . -B build/amiga -DCMAKE_TOOLCHAIN_FILE=cmake/amigaports.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/amiga --parallel
cmake --install build/amiga --prefix build/package
cpack --config build/amiga/CPackConfig.cmake -B build/packages
```

Use separate build directories for native tests and Amiga targets. The root
CMake build compiles C directly; do not invoke the archived developer build
scripts or regenerate imported Autotools outputs as an incidental cleanup.
OpenVPN's existing source manifest supplies its source list. Target feature
checks must compile/link with the cross compiler, never run target executables
on the host or accidentally use host headers/libraries. AmiSSL is an external
SDK/runtime: only its stubs are linked here, no crypto library is rebuilt.

Dependency versions, checksums, emulator commit and Rust version belong in
`ci/dependencies.json`. When changing one, verify the public artifact hashes,
update documentation, use a clean target build and run the affected CI jobs.
Never disable verification or silently fall back to an unpinned dependency.
The client/helpers target 68020; the freestanding driver and probe target
68000. Preserve these CPU/ABI, stack-size and entry-point requirements.

## Code changes

Use the style and license headers of the edited component. Avoid broad
formatting changes, generated-file churn, duplicate implementations and new
runtime dependencies for small fixes. Handle allocation, DOS/Exec/SANA-II
errors and cleanup explicitly. Keep device interrupt/scheduler invariants,
request ownership, abort semantics, buffer lifetimes and bounds intact. Do not
introduce blocking work or logging in interrupt/Forbid regions. Do not change
public ABI, supported stacks or protocol behavior incidentally.

Keep stack-specific routing, DNS and lifecycle policy in `stacks/`. The source
layout is not the Amiga installation layout. Preserve configured assigns and
hook names; never overlay scripts for multiple stacks on a configured system.
Preserve the physical route to the VPN server and restoration on disconnect.
Report unverified stack behavior rather than promoting it to validated status.

## Tests: write and run

For portable driver logic, extend the existing tests under
`src/vpn-device/tests/` and register executable tests in `tests/CMakeLists.txt`.
Keep assertions active (`-UNDEBUG`) in Release builds. Prefer deterministic
regressions that fail before the fix and assert observable behavior: packet
contents/order/bounds, errors, lifecycle or abort completion. Do not mirror
implementation details, weaken assertions or suppress failures to make CI green.
Host mocks cannot establish target ABI or real Exec/device behavior.

```sh
cmake -S . -B build/host -DAOVPN_HOST_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure
```

These run packet-ring, ICMP and mocked-Exec device regressions on Linux/macOS.
For OpenVPN protocol changes, use appropriate upstream tests as well and record
which were actually run. Retained upstream tests are not all registered in the
port's CI; never claim the full upstream suite passed from three host tests.

Use the pinned volamos runtime for fast target FIFO memory checks with its
sanitizer and dirty heap (`docs/BUILD.md`). Preserve actual exit statuses;
never suppress a sanitizer finding to green CI. Its API emulation and optional
host sockets are separate coverage from real Miami or Roadshow operation.

## Emulators and hardware

CI uses pinned Copperline with its open-source AROS ROMs, not a commercial
Kickstart or Workbench image. Follow `docs/BUILD.md` to build that exact commit,
copy its `assets/aros/` beside the binary and create sibling `portable.txt`.
Run the freshly built tests and driver together:

```sh
python3 tools/emulator-smoke.py --emulator /path/to/copperline --bin-dir build/amiga/bin --results build/emulator-results
```

The runner requires real guest return status, bounded timeouts and the device's
fresh completion log. It records hashes/logs and preserves timeout screenshots.
The FIFO test executes target libc and assertions. The SANA-II probe exercises
query, both directions, buffered delivery and pending-read abort against the
fresh driver. Its DOS-only wrapper changes entry/logging, not device assertions.
HUNK starts at byte zero: retain the assembly entry branch ahead of any
NDK-generated code. DOS numeric format arguments differ from libc; preserve
32-bit widths. `RunProg:` is the isolated guest mount, not an installed device.

Neither smoke test establishes an encrypted OpenVPN connection, certificate
verification, reconnect, routing or DNS restoration. Changes to those paths
need explicit end-to-end evidence with the supported stack and AmiSSL runtime.
Use an authorized emulator image or coordinate a real machine with its owner;
never interrupt installation/audit/performance runs. Save and restore previous
state, use RAM candidates where possible, preserve management access and do not
swap installed devices/libraries, images or reboot without authorization.
Never upload private keys, VPN credentials, provider profiles or private ROMs.
Do not leave debug processes or candidate files behind; keep a candidate active
for hands-on review when the human requests it, until that review finishes.

Keep stack-specific test source and instructions under `tests/stacks/`.
The Roadshow demo test must use its pinned public archive and a fresh driver,
not an installed stack. Never upload its extracted libraries or private image
contents as artifacts. Miami has a repeatable licensed-runtime roundtrip/reconnect/shutdown test.
Its private CI job is enabled only for a manually selected reviewed ref; never
run untrusted PR code on that host. Missing/skipped private jobs are not passes.
The fuller AROS experiments fail shutdown and must not be relabeled as validated. See `tests/stacks/README.md` for the real-stack checks.

## Versioning and releases

`VERSION` is authoritative for the Amiga port: `MAJOR.MINOR.PATCH-amiga.N`.
The first three numbers must match `src/openvpn/version.m4`; `N` is a positive
port revision. Increment it for each new published Amiga revision of the same
upstream version; reset it to 1 when importing a new upstream version. Do not
bump per commit and do not edit upstream version metadata just to revise the
port. Existing driver ABI/library version fields are separate compatibility
contracts, not automatically derived from the package version.

Update `CHANGELOG.md` with user-visible changes and known limitations, mark the
entry unreleased until authorized release, and run:

```sh
python3 tools/check-version.py
python3 tools/check-version.py --tag "v$(cat VERSION)"
```

CMake validates the version, puts the port identity in the client's special
build string and uses it for package filenames. A release tag is exactly
`v` followed by `VERSION`. CI rejects mismatched tags. Do not rewrite existing
release tags or legacy binaries. Prefer an annotated tag on the exact reviewed
commit, only after the maintainer authorizes tagging.

Before integration, check substantive review findings and all expected jobs on
the latest exact head: version check, both host jobs, Amiga cross-build and
Copperline guest tests, plus actual native Miami evidence for affected stack
changes. Queued, skipped, cancelled or old-SHA results are not
passes. Inspect artifacts and coverage, not just aggregate status. Record source
SHA, dependency pins, compiler version, binary/package hashes and actual results.
Do not relabel mocked, partial or hardware-only results as a full suite pass.

Version-tag CI creates only a draft release after those gates pass. The
maintainer reviews notes, licenses, artifact checksums, stack installation paths
and remaining interoperability evidence before publishing. No auto-merge,
automatic release publication or issue closure. Refer to issues without
closing keywords unless closure was explicitly approved.

## Finishing work

Use focused commits and normal reviewed PRs; describe changes, reasons, tests
and limitations concretely. Preserve authorship and source provenance. Remove
only task-created scratch, unused build outputs and merged temporary branches
when safe. Retain compact evidence and genuinely pending work. Verify the final
working-tree state; never reset, stash or delete unrelated edits for cleanliness.

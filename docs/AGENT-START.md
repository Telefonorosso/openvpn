# Start here: agent operating procedure

Read `AGENTS.md` first. This page is the short execution path for a new agent.
The handoff is self-contained; no prior conversation, developer machine paths,
installed candidate or private credential is part of the public build.

## 1. Identify the repository and preserve work

Run these read-only commands in the actual OpenVPN checkout:

```sh
git rev-parse --show-toplevel
git remote -v
git status --short
git branch --show-current
git rev-parse HEAD
python3 tools/check-version.py
```

The authoritative repository is `Telefonorosso/openvpn`; `tinic/openvpn` is a
contributor fork. Check `CMakeLists.txt`, `VERSION` and `src/openvpn/version.m4`
exist before editing. Stop if this is AmiNetXDuo, Copperline or a parent Git
checkout. Preserve dirty work; do not reset, stash, force-push or remove it.
A source archive can build, but has no verified Git identity. Build artifacts
mark it `UNVERIFIED-SOURCE-ARCHIVE`; never present that as a verified commit.

Create a focused branch using the normal project workflow. Preserve the initial
import and subtree merge history; use merge commits for that history. Do not
squash/rebase subtree imports. Keep official upstream edits distinct from port
integration, packaging and stack hooks. Never fork the shared client/driver by
stack just to change routing or DNS.

## 2. Build using the tested entrypoint

Prerequisites: Python 3.12+, CMake 3.24+ (including CTest/CPack), a native C
compiler/build tool and `lha`. Pinned cross inputs currently support Linux
x86_64 and macOS Apple Silicon. Windows and Intel Mac cross-builds are not
provided; use GitHub's Linux job instead. See `docs/BUILD.md` for package names.

```sh
python3 tools/build.py
```

This runs existing host tests, verifies/downloads GCC + AmiSSL SDK + volamos,
builds Amiga targets, runs the existing target FIFO memory test, and packages.
SDK paths are set by the script. It uses `build/`, makes no Amiga installation,
and never tags or publishes. `--jobs N` adjusts compiler parallelism. Shortcuts:

```sh
python3 tools/build.py --host-only
python3 tools/build.py --cross-only
```

Outputs: host XML in `build/host`, Amiga binaries in `build/amiga/bin`, volamos
results in `build/volamos-results`, and packages/provenance/checksums in
`build/packages`. Each configure uses CMake's `--fresh` to clear stale cached
SDK/compiler paths. It preserves existing build files and older package names;
use only the current version's files listed in `SHA256SUMS`. A dirty Git tree
is marked `COMMIT+dirty`; a source archive is explicitly unverified.

`--require-clean-source` refuses archives and dirty checkouts before building
and checks source cleanliness again before writing candidate metadata. CI uses
this flag so a green candidate always has a verified clean checkout identity.

The helper does not run real-stack/full-system tests locally or claim their
coverage. CI still runs the existing public emulator and Roadshow tests.
The licensed Miami lane is documented in `tests/stacks/README.md`.

## 3. Select the required existing validation

| Change | Required evidence |
| --- | --- |
| Documentation only | Check instructions, local links and version terminology; normal public CI |
| Build helper, CMake, packaging, pins | Run the one-command path, inspect package contents/source/checksums; normal public CI |
| Portable ring/driver logic | Host regressions, target FIFO and actual device probe; affected real-stack tests |
| Shared client, device, lifecycle, DNS or routing | Public checks plus affected real-stack/full VPN tests; full Miami when its behavior is affected |
| Upstream OpenVPN or AmiSSL/toolchain update | Clean build with recorded pins, public checks, full supported interoperability evidence |
| Release | Follow `docs/RELEASING.md`; explicit human approval for exact tag and publication |

`ci/validation-baseline.json` and `docs/HANDOFF.md` record the tested historical
core. Compare source/pin changes against that SHA before using baseline evidence.
Record a later SHA and its exact coverage separately. Do not copy the old green
result onto a new SHA, call skipped/private jobs passes, or claim all upstream
OpenVPN tests ran. No standing private runner is promised: obtain an authorized
operator's run when required. Missing required validation is a reported blocker,
not an implied pass or permission to weaken coverage.

## 4. Finish a reviewable PR

Report what/why, exact commit, actual job/test verdicts, artifact hashes and any
remaining coverage limits. Keep failures visible and respond to substantive
reviews. Verify all five public CI jobs on the latest head. For runtime changes,
include affected native evidence. Check no credentials, private ROMs/libraries,
registration keys or registered screenshots are in source/artifacts.

Review the package layout against `docs/INSTALL.md`. Preserve legacy binaries
and source provenance. Keep code-only changes small. Update `CHANGELOG.md` for
user-visible changes; do not bump the version for every commit.

Merge/tag/publish only with the appropriate human authorization. Issue closure
requires separate approval. Remove only own scratch, unused build outputs and
finished branches after retaining useful small evidence; preserve pending PRs,
other owners, licensed assets and unpublished work. Leave the repository clean
apart from pre-existing user changes. Never make the maintainer clean our debris.

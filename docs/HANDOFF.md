# Handoff and validation record

## What is delivered

One shared client and driver, per-stack hooks, tracked official OpenVPN subtree,
CMake build, pinned dependencies, versioning, candidate packaging and CI. The
maintainer website path starts in [MAINTAINER.md](../MAINTAINER.md); a new agent
starts in [AGENTS.md](../AGENTS.md) and [AGENT-START.md](AGENT-START.md).
For an independent browser-only review, use [the copy/paste brief](REVIEW.md).

The original active import preserved 550 source/support blobs. Historical BETA2
binaries and scripts keep their bytes. The large PR includes official OpenVPN
source, not hundreds of thousands of lines of newly authored port code. Read
[provenance](REPOSITORY.md) before moving or deleting historical files.

## Verified executable baseline

Core source: `ef95ec51f40022b664a03d3f37baf09f578c4f31`.
[Machine-readable record](../ci/validation-baseline.json) includes versions,
run links, actual counts and packaged binary hashes.

| Evidence | Actual result |
| --- | --- |
| [Public PR CI](https://github.com/tinic/openvpn/actions/runs/41765138553) | Five successful public jobs; private and release jobs skipped, not passed |
| [Exact-head licensed CI](https://github.com/tinic/openvpn/actions/runs/41765143907) | Six successful jobs including both real Miami tests; draft release skipped |
| Host | Three regressions each on Linux and macOS |
| Target | FIFO under volamos sanitizer/dirty heap; actual AROS FIFO/device probe |
| Roadshow demo 1.15 | Real UDP socket/device roundtrip, interface removal, return 0 |
| MiamiDX 1.0c | Socket/device cycles, OFFLINE/ONLINE and QUIT, return 0 |
| Registered Miami/full client | Two encrypted UDP cycles, CA/server verification, TLS 1.3/AES-256-GCM/TLS-crypt, production hooks, pushed/restored DNS and endpoint-route preservation/cleanup, actual Done return 0 |
| Candidate | ZIP/tar.gz checksums/source identity and identical payloads verified; tested client/driver hashes match packages |

The full fixture used authorized Kickstart 3.1/MUI 3.8, registered Miami,
pinned AmiSSL 5.27 binary runtime and an isolated Linux server with temporary
RSA-2048 credentials. It ran original startup/hooks. No live Amiga was deployed,
rebooted or changed. The earlier background-startup hypothesis was rejected;
no such product change is included. The one-job runner, temporary variables,
credentials and task build directories were removed after evidence collection.

New handoff/docs/build-helper commits do not inherit an exact-SHA native claim
from this baseline. Their public CI validates the current head and the new
entrypoint; an unchanged-core comparison can justify retaining the separate
baseline. Changes to client/device/hooks/pins require their affected tests.

## Honest coverage limits

This is a controlled IPv4 UDP fixture. It does not establish every provider,
cipher, MTU, IPv6, performance or abnormal-exit behavior. Roadshow's public test
checks its socket/device path; no encrypted Roadshow lifecycle result is claimed.
Fuller public AROS/Miami experiments failed shutdown in Copperline and Amiberry;
they are not supported passing evidence. Volamos models target APIs for fast
memory checks and is separate from real Miami/MUI/SANA-II behavior.

## Adoption boundary

The authoritative repository's Actions settings and first main-branch run are
controlled by its owner. A contributor cannot enable those settings or approve
its own upstream merge. The fork runs above prove the supplied workflow; they
are not runs in `Telefonorosso/openvpn`. [The maintainer guide](../MAINTAINER.md)
has the exact owner steps and expected results. Nothing in this handoff grants
merge, release-tag, publication or issue-closure authorization.

The private Miami job is a manually selected reviewed-ref test. An authorized
operator supplies its documented variables, runner and licensed assets. No
standing private infrastructure or undistributed credential is assumed by the
normal build. Public CI is credential-free except GitHub's normal job token.

There are no deferred implementation TODOs in this handoff. The limitations
above are declared test scope and owner decisions, not hidden unfinished work.

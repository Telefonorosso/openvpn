# Build and CI

## Native CMake cross-build

The Amiga build compiles and links C sources directly with AmigaPorts GCC.
It does not launch an Autotools or Makefile build. The port's existing
`openvpn_SOURCES` manifest supplies the OpenVPN source inventory; CMake
compile/link probes generate the target configuration header.

Dependencies are pinned in `ci/dependencies.json`: AmigaPorts GCC
`v16.2-rc23` (built from the AmigaPorts GCC project), AmiSSL Developer SDK
`5.27`, and a specific Copperline source commit. Archive SHA-256 checks are
mandatory. The SDK supplies headers and stubs; OpenVPN uses the user's
installed AmiSSL libraries at runtime. No AmiSSL/OpenSSL library is built.

On Linux install CMake, a C build tool, Python 3.12+, and `lhasa`; on macOS use
the corresponding Homebrew tools including `lha`. Download the pinned inputs:

```sh
python3 tools/fetch-deps.py
```

Set `AMIGA_GCC_ROOT` and `AMISSL_SDK` to the paths printed by that command, then:

```sh
cmake -S . -B build/amiga -DCMAKE_TOOLCHAIN_FILE=cmake/amigaports.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build/amiga --parallel
cmake --install build/amiga --prefix build/package
cpack --config build/amiga/CPackConfig.cmake -B build/packages
```

Targets are `openvpn`, `vpn_device` (output `vpn.device`), `anxdns`,
`amitcpresolver`, and the guest smoke programs. The client/helpers target
68020; the freestanding device and smoke programs target 68000. The unchanged
historical `RequestString` utility accompanies the stack scripts in packages;
it is not presented as a rebuilt target. ZIP and tar.gz packages contain fresh
client/driver/helper binaries, stack integrations and license notices.

## Host regressions

```sh
cmake -S . -B build/host -DAOVPN_HOST_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure
```

These execute the existing packet-ring, ICMP and mocked-Exec driver tests.
Assertions remain enabled in release builds. Host tests do not establish the
Amiga ABI or interoperability with a TCP/IP stack.

## Copperline guest smoke tests

CI builds the pinned Copperline commit with its `frontend` feature and bundled
open-source AROS ROMs. It uses no proprietary Kickstart, Workbench image, VPN
credential or external VPN service. Copy `assets/aros/` from that exact
Copperline checkout beside its binary and create `portable.txt` there to keep
the generated guest volumes in the build directory. Then run:

```sh
python3 tools/emulator-smoke.py --emulator /path/to/copperline --bin-dir build/amiga/bin --results build/emulator-results
```

The tests execute freshly cross-built HUNK programs. The FIFO test exercises
the C runtime and ring assertions. The device test invokes the existing
`AOVPN-VPNProbe` checks: device query, both transfer directions, buffered
delivery, and pending-read abort. Its freestanding DOS-only entry/logging
avoids depending on `mathieeedoubbas.library`, which the minimal AROS ROM
does not include. It does not replace or mock the driver's Exec/SANA-II calls.
`RunProg:` supplies the test's private file mount and driver path. An explicit
HUNK entry branch precedes any NDK-generated helper code.

Copperline's actual guest return code is required, and the device's fresh
guest log must contain the completion marker. A missing binary, ROM, return
marker, nonzero result or timeout fails the job. Logs, hashes and timeout
screenshots are retained as artifacts. These smoke tests do **not** cover an
encrypted OpenVPN session, routing, DNS restoration or all supported stacks.

## GitHub workflow and releases

Every PR and main push runs Linux/macOS host regressions, a Linux AmigaPorts
cross-build, and the Copperline guest smoke job. Cross-build artifacts include
the source commit, dependency provenance and package SHA-256 checksums.
The version check also validates `VERSION` against upstream `version.m4`;
release tags must equal `v` followed by `VERSION`. See `AGENTS.md` for revision
and changelog rules. Version-tag pushes create a **draft** release only after
the version, both host, cross-build and guest jobs pass.
The maintainer reviews and publishes it; this change does not push a release tag
or publish a release. Francesco's repository remains the authoritative project.

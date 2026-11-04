# Stack compatibility tests

One rebuilt `openvpn`/`vpn.device` serves every stack. Keep stack lifecycle
scripts and configuration in `stacks/<stack>/`, not separate executable forks.
These checks are separate from the common FIFO and driver-probe tests.

## Roadshow 1.15 demo: automated real-stack test

`roadshow.c` opens the real Roadshow library, attaches the freshly built
`vpn.device` unit 0 as a PPP point-to-point interface, configures the addresses
and brings it online. A real UDP socket sends a packet through Roadshow and
the driver; unit 1 must receive the expected IPv4/UDP payload. The test reflects
that packet back through unit 1 and requires the socket to receive the exact
reply. It then removes the interface. No physical card, Internet connection,
provider credential or encryption service is required.

```sh
python3 tests/stacks/fetch-roadshow.py
python3 tests/stacks/run-roadshow.py --emulator /path/to/copperline --bin-dir build/amiga/bin --demo-archive build/stack-dependencies/Roadshow-Demo-1.15.lha --results build/roadshow-results
```

Install `lhasa`/`lha` first. The demo originates from the
[publisher](http://roadshow.apc-tcp.de/download-en.php). CI fetches the
byte-identical demo in the [Emu68 Imager distribution](https://github.com/mja65/Emu68-Imager-Software),
whose [instructions](https://mja65.github.io/Emu68-Imager/instructions.html) document permission to include it. Both its Git commit and the original
publisher archive's SHA-256 are pinned in `ci/stack-dependencies.json`.
This avoids the publisher's interactive PHP download form, which intermittently
returned HTML to GitHub runners. Missing inputs or checksum mismatches fail;
there is no unchecked fallback. Each run uses a temporary private directory, never a host installation.
Only logs, hashes and timeout screenshots are retained. Demo libraries are not
committed, packaged or uploaded as artifacts. Do not substitute a registered
archive, keys or live network configuration into public CI.

This tests Roadshow's real socket/interface/SANA-II interaction with the shared
driver. It does not run OpenVPN encryption, DNS hooks, routing hooks or reconnect.
There are no dedicated Roadshow lifecycle scripts in the published source;
AmiTCP-NG hooks are not automatically validated as Roadshow hooks by this test.

## MiamiDX 1.0c: real licensed-runtime test

The published Miami package has the same client/driver bytes as the other
packages; its special parts are `MiamiIfConfig`, `MiamiRoute`, MiamiIPFW,
Miami's DNS database/ARexx integration and saved preferences. These remain in
`stacks/miamidx/`, without separate client/driver forks.

`miami.c` launches the public demo in documented `NOGUI DONTCONNECT` mode,
brings only `ptp0` online through its real ARexx host, sets isolated test
addresses with the supplied MiamiIfConfig command and checks a real UDP
roundtrip through both device units. It repeats after OFFLINE/ONLINE, closes
its socket/library handles, requests QUIT and requires the host port to vanish.
The completion marker and actual guest return code are both mandatory. The
fixture's ONLINE command returns warning 5; the test still requires state 1,
packet roundtrips, OFFLINE state 0 and clean shutdown. A warning is never used
as the sole evidence of a working connection.

No RexxMast interpreter is required: the probe sends normal ARexx command
messages directly to Miami's host. Outstanding messages retain their live reply
target until the VM deadline on timeout; they are never freed under the host.
The test uses no-op copies of startup/shutdown hooks to isolate the driver.
It does not establish a provider VPN connection or validate the production
routing/DNS/firewall hooks.

Download the two pinned public Miami archives and provide an authorized OS tree
with `Libs`, `Fonts` and `System/MUI`, plus a matching Kickstart ROM:

```sh
python3 tests/stacks/fetch-miami.py
python3 tests/stacks/run-miami.py --emulator /path/to/copperline --bin-dir build/amiga/bin --main-archive build/stack-dependencies/MiamiDx10cmain.lha --gui-archive build/stack-dependencies/MiamiDx10c-MUI.lha --guest-system /path/to/authorized-system --rom /path/to/authorized-kickstart.rom --results build/miami-results
```

The repeatable check passed with Kickstart 3.1, MUI 3.8 and the existing licensed
ClassicWB runtime. ASL, commodities, ARexx and ordinary maths/GUI libraries are
required; the runner copies only the needed runtime subset into a disposable
volume. It records hashes and logs, and removes staged files afterward. No
registered keys, ROMs or copied system libraries are committed or uploaded.

### CI access and coverage

Public PR CI compiles the Miami probe along with the other Amiga targets.
The complete runtime job is explicitly enabled using **Run workflow** with
`miami_native=true` on a reviewed source ref. Configure repository variables
`MIAMI_TEST_RUNNER` (a private runner label), `MIAMI_GUEST_SYSTEM` and `MIAMI_ROM`
(the host's authorized read-only asset paths). The job consumes the fresh hosted
cross-build and the hosted pinned Copperline binary, verifies its checksum and
source pin, then runs the exact same command. No compiler build runs on the
private host. Use an ephemeral one-job runner and remove its work afterward.
Never change this to automatically execute unreviewed fork PR code on a host
with private assets. A skipped/unconfigured native job is **not** a Miami pass.

### AROS experiments: unsupported, not a pass

The minimal bundled AROS ROM lacks asl.library and cannot start Miami. A fuller
2026-11-03 AROS runtime, with Zune or MUI 3.8, completed packet roundtrips and
interface cycles but failed to complete shutdown. Amiberry also halted during
that experiment. This is not counted as a complete test and is not used as the
Miami CI runtime. It does not establish the cause of the AROS failure or a defect
in the AmigaOS-supported client/driver. Do not weaken the shutdown assertions to
make this variant green.

## End-to-end VPN coverage still needed

For each supported stack, use an isolated authorized guest and a local test
OpenVPN server with throwaway credentials to verify encrypted traffic, MTU,
provider route preservation, production Up/RouteUp/RoutePreDown hooks, pushed
DNS, firewall changes, reconnect and abnormal-exit cleanup. Record exact source,
stack/runtime versions and actual results; sanitize keys and credentials. The
socket/device tests above do not replace these end-to-end checks.

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

[The authorized private operator procedure](../../docs/PRIVATE-MIAMI.md) covers
asset prerequisites, repository variables, guarded one-job pickup, exact-SHA
verdicts and cleanup without relying on the original chat.


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

## Encrypted Miami VPN test (registered private runtime)

`run-miami-vpn.py` runs the freshly built client and shared driver, using the
actual Miami startup, Up, RouteUp, RoutePreDown and shutdown scripts. It uses
throwaway RSA-2048 certificates and a local OpenVPN server inside a disposable Linux
user/network namespace. The server endpoint is outside the physical subnet,
so the production endpoint route must keep it reachable after split-default
routes are installed. The UDP peer answers only the assigned tunnel address;
a cleartext host/NAT shortcut cannot satisfy the test. Distinct DNS queries
check physical DNS before, pushed VPN DNS during, and restoration after two
connect/disconnect cycles. The interpreted 68020 gets a 180-second TLS handshake
window on both ends; certificate verification, EKU and TLS-crypt stay enabled.
Both hook completion and actual guest return status
are required; logs and route snapshots are retained.

This lane requires authorized MiamiDX registration files (the demo disables
lifecycle events and administrative utilities), an authorized OS/MUI runtime,
and a physical Ethernet driver for the emulator. Originals remain read-only;
private copies, generated keys and certificates are removed with the VM.
Repository variables `MIAMI_KEYS_DIR` and `MIAMI_NETWORK_DEVICE` supply the
private host paths in addition to the three variables above. Linux x86_64,
`unshare -Urn`, `/dev/net/tun`, `ip`, `openssl`, `dpkg-deb` and `lha` are needed.
Public dependency downloads happen before network isolation. No packages or
services are installed and the host network namespace is never configured.

Miami intentionally asks permission before ARexx changes its DNS database.
The pinned Copperline control API approves only the visually verified stock
Kickstart requester, guarded by its exact warning-text pixel digest and screen
size. Only then does it send Intuition's documented Left-Amiga-V shortcut
for the leftmost (Yes) choice. An unknown dialog fails; this is not a generic
timed Yes keypress.
Screenshots and registration banners remain private and are never artifacts.
Theme/runtime changes require reviewing that exact dialog fixture again.

```sh
python3 tests/stacks/run-miami-vpn.py --emulator /path/to/copperline --bin-dir build/amiga/bin --main-archive build/stack-dependencies/MiamiDx10cmain.lha --gui-archive build/stack-dependencies/MiamiDx10c-MUI.lha --guest-system /path/to/authorized-system --rom /path/to/authorized-kickstart.rom --keys-dir /path/to/authorized-miami-keys --network-device /path/to/a2065.device --results build/miami-vpn-results
```

## Remaining end-to-end coverage

For each supported stack, use an isolated authorized guest and a local test
OpenVPN server with throwaway credentials to verify encrypted traffic, MTU,
provider route preservation, production Up/RouteUp/RoutePreDown hooks, pushed
DNS, firewall changes, reconnect and abnormal-exit cleanup. Record exact source,
stack/runtime versions and actual results; sanitize keys and credentials. The
socket/device tests above do not replace these end-to-end checks.

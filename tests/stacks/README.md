# Stack compatibility tests

One rebuilt `openvpn`/`vpn.device` serves every stack. Keep stack lifecycle
scripts and configuration in `stacks/<stack>/`, not separate executable forks.
These checks are separate from the common FIFO and driver-probe tests.

## Roadshow 1.15 demo: automated real-stack test

`roadshow.c` opens the real Roadshow library, attaches the freshly built
`vpn.device` unit 0 as a PPP point-to-point interface, configures the addresses
and brings it online. A real UDP socket then sends a packet through Roadshow
and the driver; unit 1 must receive the expected IPv4/UDP payload within a
bounded interval. The test removes the interface afterward. No physical card,
Internet connection, provider credential or encryption service is required.

```sh
python3 tests/stacks/fetch-roadshow.py
python3 tests/stacks/run-roadshow.py --emulator /path/to/copperline --bin-dir build/amiga/bin --demo-archive build/stack-dependencies/Roadshow-Demo-1.15.lha --results build/roadshow-results
```

Install `lhasa`/`lha` first. The fetcher uses the public demo linked by the
[publisher](http://roadshow.apc-tcp.de/download-en.php), served by
[Amiga Future](https://www.amigafuture.de/app.php/dlext/details?df_id=3657), and
verifies the archive SHA-256 in `ci/stack-dependencies.json`. A changed form,
missing input or checksum mismatch fails; there is no alternate unchecked
source. Each run uses a temporary private directory, never a host installation.
Only logs, hashes and timeout screenshots are retained. Demo libraries are not
committed, packaged or uploaded as artifacts. Do not substitute a registered
archive, keys or live network configuration into public CI.

This tests Roadshow's real socket/interface/SANA-II interaction with the shared
driver. It does not run OpenVPN encryption, DNS hooks, routing hooks or reconnect.
There are no dedicated Roadshow lifecycle scripts in the published source;
AmiTCP-NG hooks are not automatically validated as Roadshow hooks by this test.

## MiamiDX: runtime test pending a fuller guest environment

The published Miami package has the same client/driver bytes as the other
packages; its special parts are `MiamiIfConfig`, `MiamiRoute`, MiamiIPFW,
Miami's DNS database/ARexx integration and saved preferences. These remain in
`stacks/miamidx/`; the saved example targets a particular PCMCIA adapter and
must be adapted to the guest's physical interface.

A bounded attempt to start the public MiamiDX 1.0c demo with its documented
`NOGUI DONTCONNECT SETTINGS=...` options on the minimal AROS ROM exits with
code 20 and reports `Can't open version 0 of asl.library`. This is an environment
blocker, not a Miami pass or evidence that OpenVPN is incompatible. Do not add
a green mock job or mark a skipped Miami test as validated. A full authorized
AmigaOS/MUI/ARexx setup (including its ordinary runtime libraries) is needed
before the existing Miami hooks can be exercised. Do not bundle private ROMs
or registered keys into this repository or public artifacts.

In that isolated guest, use the fresh shared binaries and a disposable copy of
the existing Miami preferences. Configure `ptp0` on vpn.device unit 0 and keep
unit 1 for the OpenVPN backend. Record exact stack/runtime/source versions,
then verify:

1. Start/stop the point-to-point interface using the Miami ARexx controls.
2. Establish a tunnel to a local test OpenVPN server with throwaway credentials;
   confirm payload traffic in both directions and packet boundaries/MTU.
3. Run Up/RouteUp hooks; check the server's physical host route, both split
   default routes, pushed DNS and the DNS firewall rule.
4. Disconnect/rejoin; inspect hook errors and confirm previous routes/DNS and
   firewall state are restored. Check cleanup after an abnormal client exit.

Retain actual command/guest logs and failure verdicts, sanitize credentials,
and keep this coverage pending until it really runs. Public CI currently covers
Roadshow's direct stack/driver interaction, not this Miami end-to-end checklist.

# POC2 FIX2 — live diagnostic counters (host-tested)

Based on POC2 FIX1. Includes all sources; no patch application is needed.

- Private `AOVPN_DIAG_CMD` (`0x7F20`) uses `ios2_StatData` and `ios2_DataLength` to return a versioned `struct AovpnDiag` snapshot.
- Driver keeps separate counters for units 0/1: incoming/accepted/rejected `CMD_WRITE`, last type, IO flags, error/wire error, rejection causes, `CMD_READ` submissions/queued/completed/errors, FIFO enqueues/deliveries, current FIFO occupancy, online and open counts.
- No file IO, no printf or DOS use in `vpn.device`; snapshots are read-only. No changes to packet forwarding semantics.
- `AOVPN-Diag` opens both units non-exclusively to query statistics and prints to stdout. Use `AOVPN-Diag >RAM:vpn-diag.txt` to save.
- No parameter needed: CLI uses no argv parsing.
- A snapshot does not clear counters. Counters reset on device resident reload/reboot, not merely on interface OFFLINE. Current `open_count` includes the transient CLI diagnostic opening.

Build (Debian cross compiler and SANA-II headers):

```sh
make CC=m68k-amigaos-gcc SANA_INCLUDE=/root/sana/include all
```

Output: `build/vpn.device`, `build/AOVPN-Diag`, `build/AOVPN-VPNProbe`.

Install `vpn.device` to `DEVS:vpn.device`, reboot to unload the previous resident, copy `AOVPN-Diag` to any CLI-accessible path. With MiamiDX ONLINE and `AOVPN-MiamiPeer` running, run `AOVPN-Diag >RAM:vpn-before.txt`, execute `ping 10.77.0.2`, then `AOVPN-Diag >RAM:vpn-after.txt`. Compare the two files. Keep both units running; do NOT run VPNProbe concurrently.

Host checks: `make test`; m68k cross-build and real hardware not tested here. Do not claim hardware validation until tested.

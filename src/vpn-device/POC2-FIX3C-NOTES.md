# AmiOpenVPN vpn.device POC2 FIX3C — isolate MiamiDX CMD_READ(0x0021)

## Rationale

FIX2 was hardware-tested: MiamiDX stayed ONLINE, but 9 `CMD_WRITE` requests
with packet type `0x0021` were all rejected. After FIX3 added support for this
type to a shared predicate, MiamiDX immediately took `ptp0` DOWN and closed
its open. A live diagnostic showed **15 `CMD_READ` requests newly queued**,
whereas under FIX2 no reads were queued. FIX3B restored FIX2 behavior for
tracking/statistics, but the immediate DOWN persisted.

FIX3C changes **exactly one conditional in the driver** compared with FIX3B:
`CMD_READ` now uses `ipv4_tracking_type()` (the FIX2 type set, `0x0800` and
`0x001f`) rather than `ipv4_type()` (includes `0x0021`).

This is a controlled A/B isolation experiment, **not** the eventual SANA-II
solution. The `0x0021` IPv4 protocol number is standard PPP and must ultimately
be supported for reads as well as writes once the lifecycle issue is fixed.

## Exact scope

| Operation | `0x0021` accepted? | Compared to FIX3B |
| --- | --- | --- |
| `CMD_WRITE` | Yes | Unchanged |
| `CMD_READ` | **No** | **Changed**: FIX2 behavior |
| `S2_TRACKTYPE` | No | Unchanged: FIX2 behavior |
| `S2_UNTRACKTYPE` | No | Unchanged: FIX2 behavior |
| `S2_GETTYPESTATS` | No | Unchanged: FIX2 behavior |
| `AOVPN-Diag` | Supported | Unchanged |

Legacy `0x0800` and `0x001f` traffic on both units remains supported.

No changes to resident ABI, device query, `OpenDevice`/`CloseDevice`,
`S2_ONLINE`/`S2_OFFLINE`, FIFO ring, Exec I/O queue mechanics,
`AbortIO`, copy hooks, or diagnostics structure. The separate
`AOVPN-MiamiPeer` POC2 binary is unchanged. The `AOVPN-Diag` banner still
mentions FIX2 because the diagnostics tool's version was not changed.

## Debian extraction / build — full source ZIP, no diffs

```sh
unzip AmiOpenVPN-VPNDEVICE-POC2-FIX3C-READ-WRITE-AB-HOST-TESTED-SOURCES.zip
cd AmiOpenVPN-VPNDEVICE-POC2-FIX3C
make test
make CC=m68k-amigaos-gcc SANA_INCLUDE=/root/sana/include all
```

Outputs: `build/vpn.device`, `build/AOVPN-Diag`, `build/AOVPN-VPNProbe`.
The m68k cross-compiler is NOT present in the preparation environment,
so no 68k binary is in the archive; compilation and real-device behavior
remain to be tested on the user's Debian/Amiga environment.

## AmigaOS A/B sequence

1. Back up existing `DEVS:vpn.device` (keep known-good POC1 and FIX2).
2. Install FIX3C's `build/vpn.device` and reboot to unload the previous resident.
3. Start the existing `AOVPN-MiamiPeer` first; then bring MiamiDX `ptp0`
   (unit 0, 10.77.0.1 -> 10.77.0.2, MTU 1400) ONLINE.
4. Immediately inspect `MiamiIfConfig ptp0` and `AOVPN-Diag`.
5. If UP remains stable, send just **3 to 5 pings**, then run `AOVPN-Diag`
   again and stop the peer with Ctrl+C to read `SUMMARY`.

**Interpretation:**

- `ptp0` remains UP where FIX3B went DOWN: strong indication that accepting
  `CMD_READ(0x0021)` is a trigger. This **does not** establish why: it may be
  a lifecycle, resource, or MiamiDX interaction specific to queued reads.
- `ptp0` still immediately DOWN: the acceptance of `CMD_READ(0x0021)` alone
  does not account for the behavior, or there is a second condition.
- While the driver still supports `CMD_WRITE(0x0021)`, the peer **may** see
  ICMP Echo Requests (`observed>0`). Echo Replies are **not expected to reach
  MiamiDX**, because its reads of type `0x0021` are rejected on purpose.
- Incoming unit 0 FIFO can hold only **16** datagrams; keep the test short
  and avoid flooding it while reads are deliberately rejected.
- `AOVPN-Diag` counters are cumulative; `read_queued` is not the current
  number of pending requests. `AOVPN-Diag` itself temporarily opens a unit.

## Host validation

`make test` passed against the actual `vpn_device.c` with mocked Exec:
- 0x0021 MiamiDX write -> unit 1 0x0800 read works.
- Unit 0 0x0021 read is rejected `S2ERR_NOT_SUPPORTED` and does not enqueue.
- The peer-to-Miami datagram can be recovered using a 0x0800 read.
- 15 consecutive 0x0021 reads reject immediately with no queue growth.
- FIX2 TRACKTYPE/UNTRACKTYPE/GETTYPESTATS rejection remains unchanged.
- Existing ring, AbortIO, diagnostics and sana2loop reuse tests pass.

**Host-tested only.** No Amiga cross-build or hardware-validation claim.

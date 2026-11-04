# AmiOpenVPN vpn.device — POC2 TRACE1 (diagnostic, host-tested)

## Why TRACE1
FIX3D was rejected by hardware: MiamiDX ptp0 goes DOWN immediately on enabling `CMD_READ(0x0021)`. FIX3C keeps ptp0 UP and proved both halves of the packet data path, but intentionally rejects the inbound MiamiDX reads. Rather than try another speculative change, TRACE1 **keeps FIX3D behavior** and records lifecycle events in a fixed-size RAM ring.

TRACE1 is a diagnostic experiment, **not a functional fix**, and it has not been hardware validated. Keep the hardware-proven FIX3C backup.

## Changes vs FIX3D
- No modifications to the SANA-II dispatch decisions or `CMD_READ`/`CMD_WRITE` behavior.
- New private command `AOVPN_TRACE_CMD` (`0x7F21`) copies a read-only per-unit trace snapshot.
- New `AOVPN-Trace` CLI enumerates the chronological records and can redirect output to RAM:.
- A bounded 128-record ring per unit in driver RAM; no DOS I/O, heap allocations or console output from the device. Capture occurs during BeginIO, completion/queueing, AbortIO, Open/Close, and asynchronous delivery.
- The older AOVPN-Diag FIX2 ABI remains unchanged and usable.

## Debian build (complete sources, no patches)

```
unzip AmiOpenVPN-VPNDEVICE-POC2-TRACE1-LIFECYCLE-DIAGNOSTICS-HOST-TESTED-SOURCES.zip
cd AmiOpenVPN-VPNDEVICE-POC2-TRACE1-LIFECYCLE-DIAGNOSTICS
make test
make CC=m68k-amigaos-gcc SANA_INCLUDE=/root/sana/include all
```

Expected outputs: `build/vpn.device`, `build/AOVPN-Trace`, `build/AOVPN-Diag`, `build/AOVPN-VPNProbe`.

## On real AmigaOS

1. Back up `DEVS:vpn.device` (preferably the FIX3C working version).
2. Install new `build/vpn.device` and `build/AOVPN-Trace`; reboot.
3. Start `AOVPN-MiamiPeer` and **leave it running** so vpn.device's instance and trace persist.
4. Bring MiamiDX `ptp0` ONLINE once. Let it fall DOWN naturally.
5. Before doing any pings, run from a Shell:

```
AOVPN-Trace >RAM:vpn-trace.txt
AOVPN-Diag >RAM:vpn-diag.txt
```

Please share both files; if the trace is long, share the whole file (not only the last few lines). The final OPEN event in each unit may come from the diagnostic tool itself.

## Interpretation

`BEGIN` logs the incoming request fields before the device resets io_Error or ios2_WireError. `DONE` logs immediate completion and error. `READ-QUEUED` is asynchronous waiting; `READ-REPLIED` is packet delivery. `ABORT-IN`, `ABORT-OK`, `ABORT-MISS` capture explicit AbortIO. `CLOSE-IN`/`CLOSE-OUT` distinguish a client CloseDevice from S2_OFFLINE; compare this with `BEGIN`/`DONE` for S2_OFFLINE. `EVENT-QUEUED` and `EVENT-REPLIED` show S2_ONEVENT processing. `RQ`, `OQ`, `EQ` are **instantaneous** pending read/orphan/event queue depths, not cumulative counters. `UP`, `OP`, `FIFO` snapshot the unit online flag, open count and pending frames.

Log records are per-unit, ordered by local sequence (not timestamp); the tool shows the most recent 128 entries. Units cannot be globally interleaved. The tool itself opens a unit for reading the trace, which can add an OPEN entry; the command does not change ONLINE state or the packet queues.

The tracing adds overhead and can affect timing: it is for diagnosis only. Host-model tests do not establish Amiga 68k ABI or hardware behavior; those require the user's cross-build and hardware test.

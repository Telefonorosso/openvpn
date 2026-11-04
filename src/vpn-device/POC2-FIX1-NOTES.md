# POC2 FIX1 — MiamiDX PPP IPv4 packet-type compatibility

Status: HOST-TESTED SOURCE FIX, NOT HARDWARE VALIDATED.

## Motivation

The hardware-validated POC1 driver reports `S2WireType_PPP` but accepts
only SANA-II packet type `0x0800` (Ethernet IPv4). The PPP IPv4 protocol
number is `0x001f` (31). MiamiDX ptp0 transmitted 6 packets (Opkts +6),
while Ipkts stayed zero. This does not prove which packet type MiamiDX used,
but provides a concrete interoperability problem to correct.

## Changes

- Accept `0x001f` and `0x0800` for IPv4 CMD_WRITE, CMD_READ and type stats.
- Normalize both types internally to a single IPv4 ring payload (`0x0800`).
- Preserve the reader's requested packet type on a successful CMD_READ.
- Keep S2_READORPHAN returning the ring's normalized type.
- Preserve POC1 data copy, ring, abort, and read/write behavior.
- Host regression tests explicitly cover mixed-type reads/writes in both directions.

## Validation

`make test`: PASS (host model, ring, re-used functions, syntax).

Not compiled into a 68k driver here (m68k-amigaos-gcc unavailable),
and not yet tested with MiamiDX on hardware.

## Hardware procedure

1. Back up your existing `DEVS:vpn.device` and preserve POC1 ZIP.
2. Compile from this tree with the same known-good cross-toolchain and SANA headers.
3. Replace `DEVS:vpn.device` only while MiamiDX and AOVPN-MiamiPeer are stopped.
   Reboot to ensure the resident old device has been unloaded.
4. Launch existing POC2 AOVPN-MiamiPeer unit 1, and bring MiamiDX unit 0 ONLINE.
5. Verify ptp0 local `10.77.0.1`, peer `10.77.0.2`, MTU `1400`.
6. Run `MiamiNetStat -i`, `ping 10.77.0.2`, then `MiamiNetStat -i` again.
7. Success: Opkts increments; Ipkts increments; peer prints ICMP echo replies.
8. If still failing, capture peer SUMMARY and Miami statistics, then instrument
   `CMD_WRITE` type/error and unit queue counts rather than changing IP settings.

Original POC1 baseline preserved in its separately supplied ZIP.

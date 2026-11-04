# AmiOpenVPN vpn.device — POC2 FIX4 (real RX, ONEVENT-preserving FLUSH)

**Status:** host-tested source build; **not yet cross-compiled or validated on Amiga hardware**. Replaces the experimental FIX3C data-plane split. Do not treat this as a completed OpenVPN/TLS implementation.

## What changed from TRACE1

The actual `vpn.device` driver preserves *both* transmission and reception for MiamiDX's PPP IPv4 type `0x0021` (`CMD_WRITE` and `CMD_READ`). Its actual transport is a bidirectional bounded ring between units 0 and 1. No fabricated packets, success responses, or special-case `CMD_READ` rejection is used by the device. This restores full receive handling absent from the FIX3C A/B experiment.

One targeted behavioral change to TRACE1's `CMD_FLUSH`: **do not abort pending `S2_ONEVENT` subscriptions**. `CMD_FLUSH` continues to abort pending typed and orphan reads and discard the FIFO, but `S2_ONEVENT(S2EVENT_OFFLINE)` remains pending until `S2_OFFLINE` actually occurs. This matches the sana2loop/loopback.device reference implementation. `CMD_RESET` retains TRACE1 behavior so that this comparison changes one command only.

The TRACE1 chronological logger and FIX2 live counters remain available, with no writes to a filesystem inside the driver. The Exec `NT_MESSAGE` handling from FIX3D remains present. No UnitTask rewrite, change to peer addressing, routing, or packet type normalization.

### Why this is a test, not a guaranteed fix

In the previous TRACE1 hardware trace, MiamiDX initiated the first `CMD_FLUSH` **before** the driver's incorrect event cancellation. Thus this change corrects a real discrepancy with loopback.device, but it does not by itself prove why MiamiDX decided to initiate the teardown. If `ptp0` still falls, please capture `AOVPN-Trace` and do not automatically roll back to read-disabled FIX3C.

## Build on Debian

Requires `m68k-amigaos-gcc` and your SANA-II headers (the same setup used for POC1):

```sh
unzip AmiOpenVPN-VPNDEVICE-POC2-FIX4-REAL-RX-ONEVENT-HOST-TESTED-SOURCES.zip
cd AmiOpenVPN-VPNDEVICE-POC2-FIX4-REAL-RX-ONEVENT
make test
make CC=m68k-amigaos-gcc SANA_INCLUDE=/root/sana/include all
```

Outputs:

- `build/vpn.device` — full bidirectional SANA-II implementation
- `build/AOVPN-MiamiPeer` — *diagnostic* ICMP responder on unit 1
- `build/AOVPN-Diag`, `build/AOVPN-Trace` — live device diagnostics
- `build/AOVPN-VPNProbe` — SANA-II transport regression check (run alone)

The `tests/mock_ndk` directory is **only** for running host tests on non-Amiga systems; it is not used to compile the Amiga binary. The driver does not contain mock read logic. The included ICMP peer is intentionally a minimal test application, **not** a production VPN or OpenVPN peer.

## Test on AmigaOS

1. Back up `DEVS:vpn.device` (e.g. the known-good FIX3C or POC1) before replacing it. Copy the newly compiled `vpn.device` into `DEVS:`.
2. Restart the Amiga to force the old device resident out of memory. Keep your tested MiamiDX interface settings: `ptp0` SANA-II device `vpn.device`, unit 0, local `10.77.0.1`, peer `10.77.0.2`, MTU `1400`.
3. Start `AOVPN-MiamiPeer` in Shell A. Bring MiamiDX `ptp0` ONLINE.
4. In Shell B:

```
MiamiIfConfig ptp0
AOVPN-Diag
ping 10.77.0.2
AOVPN-Diag
MiamiNetStat -ni
```

Successful outcome: `ptp0` remains UP; unit 0 `WRITE accepted` and `READ completed` rise; unit 1 `READ completed` and `WRITE accepted` rise; `ping` receives ICMP Echo Replies.

If it drops, **before stopping the peer**, collect:

```
AOVPN-Trace >RAM:vpn-trace.txt
AOVPN-Diag >RAM:vpn-diag.txt
```

In the trace, the important indicator is that the first `CMD_FLUSH` should **leave the S2_ONEVENT queue depth at 1**; a real subsequent `S2_OFFLINE` then replies to the event. If the first flush is still initiated by MiamiDX, the trigger predates this fix and further debugging is required.

## Validation limits

Host tests exercise real driver dispatch/queue operations linked against an in-memory Exec model, including 15 simultaneous `CMD_READ(0x0021)` receiving 15 peer writes and FLUSH/ONEVENT/OFFLINE completion. They **do not** validate the real Amiga Exec ABI, scheduler, MiamiDX state machine, or packet delivery to an actual stack. Hardware results must be reported separately.

## Sources and licensing

Reused `sana2loop` helper functions are covered by its bundled BSD-2-Clause license. See `THIRD-PARTY-NOTICES.md` and `third_party/sana2loop/LICENSE`. The original POC2 ICMP responder source is bundled unchanged for a complete test build.

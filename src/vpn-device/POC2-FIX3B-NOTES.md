# AmiOpenVPN vpn.device POC2 FIX3B — control-plane A/B isolation

## Why this experimental build exists

Physical AmigaOS/MiamiDX FIX2 evidence showed **nine** outgoing IPv4
`CMD_WRITE` requests with packet type `0x0021`, all rejected by the device.
FIX3 accepted `0x0021`, but MiamiDX then took `ptp0` UP and immediately
DOWN, without any `CMD_WRITE`; live diagnostics showed 15 accepted/queued
reads on unit 0 and the peer on unit 1 waiting for its first packet.

The shared `ipv4_type()` predicate in FIX3 had *also* extended the
`0x0021` acceptance of S2_TRACKTYPE, S2_UNTRACKTYPE and
S2_GETTYPESTATS. That changed the control-plane behavior relative to FIX2.
We do **not** know that this caused the DOWN transition. FIX3B is an A/B
isolation experiment that restores precisely that control-plane behavior.

## Changes compared with FIX3

- `CMD_READ` and `CMD_WRITE`: **unchanged**, accept `0x0021`, `0x001f`,
  and `0x0800` as IPv4 on either unit.
- `S2_TRACKTYPE`, `S2_UNTRACKTYPE`, `S2_GETTYPESTATS`: use the separate
  `ipv4_tracking_type()` predicate, accepting `0x001f` and `0x0800` as
  FIX2 did, but **not** `0x0021`.
- No changes to FIFO, Exec request lifecycle, copy hooks, event handling,
  live counters, device query, ABI, `AOVPN-Diag`, or `AOVPN-VPNProbe`.

The underlying single `u->tracked` flag remains a known simplistic
implementation; FIX3B does not attempt to fix it.

## Debian: build from the complete source ZIP (no patches)

```sh
unzip AmiOpenVPN-VPNDEVICE-POC2-FIX3B-CONTROLPLANE-ISOLATION-HOST-TESTED-SOURCES.zip
cd AmiOpenVPN-VPNDEVICE-POC2-FIX3B
make test
make CC=m68k-amigaos-gcc SANA_INCLUDE=/root/sana/include all
```

Outputs: `build/vpn.device`, `build/AOVPN-Diag`, `build/AOVPN-VPNProbe`.
The separate existing `AOVPN-MiamiPeer` POC2 executable is unchanged.

## AmigaOS A/B test

1. Back up the previous `DEVS:vpn.device` and replace it with this build.
2. Reboot, to ensure the previous resident is unloaded.
3. Start `AOVPN-MiamiPeer` on unit 1. Bring the existing MiamiDX `ptp0`
   (unit 0, local `10.77.0.1`, peer `10.77.0.2`, MTU 1400) ONLINE.
4. Immediately check `MiamiIfConfig ptp0` and `AOVPN-Diag`.
5. **Only if ptp0 stays UP**, run `ping 10.77.0.2` and check diagnostics.

Expected A/B interpretation:
- If `ptp0` now stays UP, tracking/statistics behavior is implicated;
  it is **not yet proof** of a root cause. Test ICMP and live counters.
- If `ptp0` still goes DOWN, the changed tracking acceptance is not
  sufficient to explain the failure; investigate queued-read/close/
  offline lifecycle next, preferably with instrumentation.
- `READ queued` is cumulative; it does not report the *current* number
  of outstanding requests.

## Validation status

**Host-tested only** using `make test` (actual vpn.device C source with
mocked Exec). Host tests include mixed-type IPv4 traffic, explicit FIX2
control-plane responses for `0x0021`, and 15 concurrent reads canceled by
S2_OFFLINE. No m68k cross-compiler was available in this environment;
**no 68k binary or hardware-validated fix is included.** The POC1
hardware-validation and subsequent FIX2/FIX3 field observations remain
separate from this experimental build.

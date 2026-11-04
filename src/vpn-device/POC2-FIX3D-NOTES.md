# AmiOpenVPN POC2 FIX3D — Exec pending-I/O message state

## Hypothesis grounded in hardware A/B results

FIX3C stayed ONLINE with MiamiDX and successfully delivered 8 Echo Requests to
`AOVPN-MiamiPeer`. The peer generated 8 replies, all buffered on unit 0,
but MiamiDX's `CMD_READ` for packet type `0x0021` was deliberately rejected.

FIX3/FIX3B accepted `CMD_READ(0x0021)`, queued 15 such requests, and MiamiDX
brought the interface DOWN immediately. Inspection of `armnet.device` and
`usbnet.device` showed an Exec I/O invariant missing from `vpn.device`:
`BeginIO` should set `io_Message.mn_Node.ln_Type = NT_MESSAGE` when starting
an operation. For queued I/O, a stale `NT_REPLYMSG` can be observed as a
completed request by `CheckIO()` even though the driver hasn't replied.
The missing state transition is a definite defect in `vpn.device`; whether
it fully explains MiamiDX's DOWN transition still requires hardware testing.

## Driver changes from hardware-observed FIX3C

Exactly **two behavioral changes** to `src/vpn_device.c`:

1. At the beginning of `vpn_beginio()`, mark the incoming message `NT_MESSAGE`
   before dispatching it. The existing IOF_QUICK semantics, `ReplyMsg()`
   invocation, `AbortIO`, and FIFO logic are otherwise left untouched.
2. Permit PPP IPv4 `0x0021` for `CMD_READ`, as already supported for
   `CMD_WRITE`, to enable MiamiDX to receive replies.

Control-plane `S2_TRACKTYPE` / `S2_UNTRACKTYPE` / `S2_GETTYPESTATS` behavior
remains that of FIX3B and FIX3C. The driver has not acquired any new DOS
operations, storage, logging or background threads. `AOVPN-Diag` remains
byte-for-byte unchanged (its legacy banner still says "FIX2").

The mock NDK also corrects `NT_MESSAGE=5`, `NT_REPLYMSG=7` to reflect the
real Amiga node type constants. The host `ReplyMsg()` stub now performs the
node state change to `NT_REPLYMSG`, instead of only counting replies.

## Build on Debian

Extract this ZIP and use the complete source tree directly, without patches:

```sh
unzip AmiOpenVPN-VPNDEVICE-POC2-FIX3D-EXEC-NT_MESSAGE-HOST-TESTED-SOURCES.zip
cd AmiOpenVPN-VPNDEVICE-POC2-FIX3D
make test
make CC=m68k-amigaos-gcc SANA_INCLUDE=/root/sana/include all
```

Outputs: `build/vpn.device`, `build/AOVPN-VPNProbe`, `build/AOVPN-Diag`.
There is no 68k compiler in the packaging environment; this is a **source-only,
host-tested** milestone.

## Hardware sequence

1. Back up the FIX3C driver (`DEVS:vpn.device`) and replace it with the
   newly cross-compiled FIX3D `build/vpn.device`.
2. Reboot AmigaOS so the old resident driver cannot remain loaded.
3. Start the existing `AOVPN-MiamiPeer` on unit 1.
4. Bring MiamiDX `ptp0` (unit 0, local `10.77.0.1`, remote `10.77.0.2`,
   MTU 1400) ONLINE. No netmask changes are required.
5. Verify `MiamiIfConfig ptp0`. If it stays UP, do 3–5 pings and check:

```text
ping 10.77.0.2
AOVPN-Diag
MiamiNetStat -i
```

**Expected, not yet observed:** both `CMD_WRITE` and `CMD_READ(0x0021)`
operate, inbound packets count on `ptp0`, and ICMP Echo Replies arrive.
If `ptp0` still drops immediately, do not change packet-type handling again;
collect `AOVPN-Diag` after DOWN. The next step should then be *read-only*
lifecycle instrumentation capturing individual I/O command/flags/error
transitions, not another speculative protocol modification.

## Host validation

`make test` validates:
- all prior FIFO/overflow/abort/diagnostic/reference-helper tests;
- MiamiDX `0x0021` WRITE -> peer `0x0800` READ;
- peer `0x0800` WRITE -> MiamiDX `0x0021` READ;
- stale `NT_REPLYMSG` reset to `NT_MESSAGE` when a request becomes pending;
- pending request invisible to a CheckIO-like predicate until `ReplyMsg()`;
- 15 outstanding MiamiDX-style reads, OFFLINE completion, and reusing a
  just-completed IORequest without a spurious early completion;
- unchanged FIX2 control-plane tracking type acceptance.

Mocks do not reproduce actual Amiga Exec scheduling, MiamiDX or real 68k ABI.
No claim of hardware validation is made for FIX3D.

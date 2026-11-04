# POC2 FIX3 — MiamiDX PPP IPv4 packet type compatibility

## Hardware evidence (FIX2)

Nine MiamiDX ICMP Echo Requests yielded nine `CMD_WRITE` calls on unit 0,
all rejected as `type/raw`; `Last WRITE type=0x00000021`, flags=0,
error=8. The peer on unit 1 had a queued `CMD_READ` and `observed=0`.

## Narrow change

The SANA-II PPP device now recognizes the actual PPP IPv4 protocol number
`0x0021` (RFC 1662) in addition to the pre-existing `0x001f` and `0x0800`
aliases. The internal FIFO continues to carry a canonical `0x0800` tag;
the `CMD_READ` filter accepts all three, so the existing MiamiPeer can remain
unchanged. Diagnostic counters and the packet transport are otherwise unchanged.

Host tests cover both directions between MiamiDX's `0x0021` and the peer's
`0x0800` as well as the earlier aliases and POC1 behaviors.

## Build (Debian cross-toolchain)

```sh
make CC=m68k-amigaos-gcc SANA_INCLUDE=/root/sana/include all
make test
```

Install `build/vpn.device` as `DEVS:vpn.device` (keep a backup), copy
`build/AOVPN-Diag`, reboot, and retry `AOVPN-MiamiPeer` plus ping from
MiamiDX. Observe `AOVPN-Diag` counters before and after.

**Validation:** host-tested; m68k cross-build and hardware ping not tested
in this environment. The POC1 hardware-validated baseline is retained separately.

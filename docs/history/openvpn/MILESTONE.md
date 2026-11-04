# AmiOpenVPN 2.6.23 - POC6 GW1 FIX1

## HARDWARE VALIDATED - October 27, 2026

Native OpenVPN 2.6.23 port for AmigaOS 68k.

### New functionality

Native default IPv4 gateway discovery through MiamiDX.

- BSD PF_ROUTE socket through bsdsocket.library
- RTM_GET version 4
- MiamiDX-compatible routing message ABI
- IPv4 default-route validation
- Native OpenVPN get_default_gateway() integration
- No external MiamiRoute invocation
- No ARexx dependency for gateway discovery

### Hardware validation

Command:
    openvpn --show-gateway

Successfully detected:
    192.168.1.1

Physical interface:
    eth0

Test conditions:
    MiamiDX running
    eth0 ONLINE
    ptp0 DOWN

### Validation scope

Gateway discovery: HARDWARE VALIDATED
Dynamic full-tunnel routing: NOT YET VALIDATED

### Source baseline

openvpn-poc5a-script-exec1

### Build

m68k-amigaos-gcc 6.5.0b
-m68020 -O1 -std=gnu99
AmiSSL v5 SDK

### Status

POC6-GW1-FIX1 is the hardware-validated baseline
for native MiamiDX default gateway discovery.

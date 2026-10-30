<p align="center">
  <img src="boingvpn.png" alt="AmiOpenVPN logo">
</p>

# AmiOpenVPN

**OpenVPN 2.6.23 for AmigaOS 68k.**

Native IPv4 OpenVPN client using **AmiSSL v5**, the TCP/IP stack's **`bsdsocket.library`**, and the SANA-II **`vpn.device`**. TLS and data-channel encryption run on the Amiga, without a Linux-side VPN daemon or proxy. Supports standard `.ovpn` profiles, UDP, TLS 1.3, AES-GCM, `tls-crypt`, hostname resolution and AmigaDOS lifecycle scripts.

## Requirements

- AmigaOS 3.1 (tested) with a fast 68k CPU; Amiga 600 + PiStorm/Emu68 is a validated setup.
- AmiSSL v5 and a working Internet connection through a supported TCP/IP stack.
- `openvpn`, `vpn.device`, stack-specific scripts and a valid provider `.ovpn` profile.

## TCP/IP stack compatibility

| Stack | Status | Notes |
| --- | --- | --- |
| **MiamiDX** | **Hardware validated** | Point-to-point `ptp0`, routing and DNS scripts. |
| **AmiTCP-NG** | **Hardware validated** | `ptp0`, full tunnel and lifecycle scripts; pushed DNS update/restoration via `amitcpresolver`. |
| **Roadshow** | **Expected compatible; validation pending** | SANA-II interface setup should be equivalent to AmiTCP-NG; route/DNS scripts use Roadshow commands. |
| **PrismNet 1.0.8** | **Validated under WinUAE** | `NetCtl` integration; TLS 1.3, AES-256-GCM, full tunnel and pushed DNS tested. |

**The OpenVPN client is stack-independent.** IP addresses, routing, default-gateway handling and DNS are managed by stack-specific AmigaDOS scripts; MiamiDX scripts must not be used unchanged with other stacks.

## Configure the VPN interface

`vpn.device` provides two SANA-II units:

- **Unit 0:** TCP/IP stack point-to-point interface, normally `ptp0` (PPP IPv4 packet type `0x0021`).
- **Unit 1:** OpenVPN TUN backend (raw IPv4, `0x0800`).

Configure **unit 0** as an IPv4 point-to-point interface with **MTU 1400**, unless the VPN configuration specifies otherwise. Keep the physical Internet interface active; install VPN routes only after tunnel establishment.

- **MiamiDX:** configure `ptp0` as a point-to-point interface using `vpn.device` unit 0.
- **AmiTCP-NG:** set up `DEVS:NetInterfaces/ptp0` and start it with `AddNetInterface ptp0`.
- **Roadshow:** use the analogous `DEVS:NetInterfaces/ptp0` / `AddNetInterface` configuration; verify the options on your release.
- **PrismNet:** manage the interface through `NetCtl` and PrismNet-specific scripts.

Tunnel IP addresses must agree with the server's assigned/pushed configuration. No fixed IP pair is required by AmiOpenVPN.

## Install and configure

1. Install AmiSSL v5. Copy `vpn.device` to `DEVS:` and place the executable, profile and scripts in a directory (e.g. one assigned as `OPENVPN:`).
2. Bring up the physical connection and configure `ptp0` on **unit 0**.
3. Select lifecycle scripts for your TCP/IP stack and supply your provider's CA, credentials and any required client certificate/key and `tls-crypt` material.

Example `client.ovpn`:

```text
client
proto udp
dev tun
remote vpn.example.org 1194
remote-cert-tls server
resolv-retry infinite
nobind
persist-key
persist-tun
tun-mtu 1400
ifconfig-noexec
route-noexec
script-security 2
up OPENVPN:Up.script
route-up OPENVPN:RouteUp.script
route-pre-down OPENVPN:RoutePreDown.script
verb 3
```

**This is a template, not a complete provider profile.** Adapt server, authentication, certificates and script paths.

The stack-specific scripts handle:

- `Up.script`: set the tunnel interface address.
- `RouteUp.script`: install routes, preserve the physical route to the VPN server and apply pushed DNS where supported.
- `RoutePreDown.script`: remove VPN routes and restore network/DNS settings.

The hooks run through AmigaDOS `C:Execute`; OpenVPN passes local environment variables such as `ifconfig_local`, `trusted_ip`, `route_vpn_gateway`, `route_net_gateway` and `foreign_option_N`. AmiTCP-NG uses the validated `amitcpresolver` DNS workflow; PrismNet uses `NetCtl`. Roadshow's complete lifecycle remains to be verified.

## Run

From an AmigaDOS Shell:

```text
Stack 262144
OPENVPN:openvpn --config OPENVPN:client.ovpn
```

Verify tunnel connectivity, routes and DNS. On abnormal termination, check that routes and resolver settings have been restored. Use `verb 4`/`verb 5` for standard diagnostics or `aovpn-diagnostics` for Amiga-specific logging.

## Certificates and local CSR

A **Certificate Signing Request (CSR) has been generated successfully on AmigaOS itself**. A locally generated CSR can be submitted to a CA or VPN administrator for signing while its associated private key remains on the Amiga. Configure the resulting signed certificate and private key in the OpenVPN profile when client-certificate authentication is required.

## Limitations

- **IPv4 TUN over UDP** is the validated mode; no IPv6 tunnel, TAP bridging or DCO support.
- Roadshow is not yet independently validated; PrismNet testing was conducted under WinUAE.
- Routing, gateway discovery, DNS changes and cleanup depend on the stack-specific scripts. Comprehensive DNS leak prevention and all failure/reconnect scenarios are not guaranteed.
- Protect private keys, verify server certificates and review scripts that change routes or firewall rules. This port has not undergone a security audit.

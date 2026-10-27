<p align="center">
  <img src="boingvpn.png" alt="AmiOpenVPN logo">
</p>

# openvpn - AI CODED

**OpenVPN 2.6.23 for AmigaOS 68k.**

AmiOpenVPN brings a full OpenVPN client to classic AmigaOS. It establishes authenticated, encrypted VPN sessions, exchanges ordinary IPv4 packets with the Amiga's own TCP/IP stack, and can route everyday application traffic through a remote OpenVPN server on the Internet.

The client runs **as a native Motorola 68k AmigaOS process**, using **AmiSSL v5** for cryptography, **MiamiDX** for networking, and a **SANA-II `vpn.device`** to connect the TCP/IP stack to OpenVPN's TUN data path. Nothing is proxied through a VPN daemon on the PiStorm Raspberry Pi: OpenVPN's protocol engine, TLS processing, and data-channel cryptography all execute in the AmigaOS environment.

The current implementation has been exercised on a real **Amiga 600 with PiStorm Classic, Raspberry Pi 3A+, Emu68, and MiamiDX**, against both a Linux laboratory server and an Internet-hosted AWS OpenVPN server. It supports IPv4 full-tunnel routing through native AmigaDOS lifecycle scripts and can obtain the physical network's default gateway directly from MiamiDX.

## Validated environment

| Component | Implementation |
| --- | --- |
| Computer | Commodore Amiga 600 |
| Acceleration | PiStorm Classic, Raspberry Pi 3A+, Emu68 |
| Operating system | AmigaOS 3.1 |
| TCP/IP stack | MiamiDX |
| Client | OpenVPN 2.6.23, native Motorola 68k executable |
| Cryptography | AmiSSL v5, OpenSSL-compatible interface |
| Virtual network interface | `vpn.device`, SANA-II point-to-point IPv4 |
| MiamiDX interface | `ptp0` (device unit 0) |
| OpenVPN TUN interface | `vpn.device` unit 1 |
| Validated servers | OpenVPN on Linux and AWS (OpenVPN 2.6.14 in the documented tests) |

## Architecture

**MiamiDX remains the system TCP/IP stack.** It manages sockets, interfaces, routes, and DNS. The SANA-II driver transports packets but performs no encryption; OpenVPN handles the VPN protocol and cryptography. The remote server is a standard OpenVPN peer, not a custom Amiga gateway.

The device provides two complementary interfaces:

- **Unit 0:** used by MiamiDX as `ptp0`; its point-to-point packet format includes the PPP IPv4 protocol identifier `0x0021`.
- **Unit 1:** used by AmiOpenVPN's TUN backend to exchange raw IPv4 packets (protocol type `0x0800`).

This is an **IPv4 layer-3 tunnel**. There is no Ethernet emulation, ARP bridge, TAP interface, or PPP session negotiation inside the VPN.

## Current capabilities

### Standards-based encrypted VPN sessions

- Native OpenVPN 2.6.23 client with conventional `.ovpn` configurations.
- UDP transport over MiamiDX `bsdsocket.library`.
- Certificate-based server authentication and TLS 1.3 in the validated sessions.
- `tls-crypt`, certificate/key files, and inline credential blocks in the tested configurations.
- Negotiated AES-GCM data channels: AES-128-GCM in the laboratory test and AES-256-GCM on AWS.
- Hostname resolution through the TCP/IP stack's `gethostbyname()` implementation, as well as numeric IPv4 server addresses.
- Usable standard log verbosity, including `verb 4` and `verb 5`; optional Amiga-specific diagnostics via `aovpn-diagnostics`.

### Native AmigaDOS integration

AmiOpenVPN supports OpenVPN's familiar lifecycle hooks through a native AmigaDOS script executor. The validated workflow uses:

- `up` to configure the address of MiamiDX `ptp0` from VPN-provided values;
- `route-up` to install tunnel routing, maintain a physical route to the VPN server, and apply DNS policy;
- `route-pre-down` to remove the installed VPN routes and DNS firewall rule during teardown.

OpenVPN variables are made available to the scripts as **local AmigaDOS environment variables**, then the calling process's previous variables are restored. Useful values include `ifconfig_local`, `route_vpn_gateway`, `trusted_ip`, `route_net_gateway`, and pushed DNS options such as `foreign_option_1`. Scripts run synchronously using `C:Execute`, subject to `script-security 2` and restricted script-path/argument validation.

This preserves the original OpenVPN approach: networking policy belongs in configurable hooks rather than in hardcoded application-specific behavior.

### DNS over the VPN

MiamiDX can change its resolver configuration while the tunnel remains active. The validated integration uses the DNS information delivered through the OpenVPN connection, updates MiamiDX's DNS settings through its native change mechanism, and installs a `MiamiIPFW` rule restricting **outbound IPv4 UDP/53** queries to the selected VPN resolver.

Hardware testing confirmed name resolution through the VPN DNS server. In the observed packet captures, the physical interface carried OpenVPN UDP transport and no plaintext DNS leakage was observed. These tests are evidence of the tested configuration, **not a guarantee against every DNS leak path**. In particular, the supplied firewall rule does not cover TCP/53, IPv6, or arbitrary encrypted DNS protocols, and automatic restoration of the previous DNS configuration is not yet complete.

## Real-hardware results

### HTTP throughput across the encrypted Internet tunnel

An unmodified native AmigaOS `usbnethttp` server delivered a **16 MiB** in-memory file to an AWS client over the VPN. Only the HTTP server's send-chunk size changed between runs; these are **end-to-end application payload measurements**, not synthetic cipher benchmarks.

| HTTP send chunk | AWS `curl` receive speed | Payload rate | Transfer time |
| --- | ---: | ---: | ---: |
| 8 KiB | 294,055 B/s | 2.35 Mbit/s | ~57 s |
| 32 KiB | 327,130 B/s | 2.62 Mbit/s | ~51 s |
| **64 KiB** | **357,107 B/s** | **2.86 Mbit/s** | **~47 s** |

The 64 KiB run improved measured throughput by approximately **21.4%** over the 8 KiB run. The Amiga remained usable during the transfer, although Emu68 CPU utilization was close to saturation. The figures do not establish the maximum possible throughput of the VPN engine.

## Running AmiOpenVPN

The following illustrates the validated **MiamiDX-based** workflow. It assumes `vpn.device` is installed, MiamiDX already has a functioning physical Internet connection, AmiSSL is available, and your OpenVPN certificate/key material and trusted server configuration have been supplied.

1. Configure a MiamiDX point-to-point interface named **`ptp0`** using `vpn.device` **unit 0**. Leave the physical Internet interface enabled.
2. Place the AmiOpenVPN executable and required `.ovpn` configuration in an AmigaDOS directory (for example, one assigned as `OPENVPN:`).
3. Provide the appropriate `up`, `route-up`, and `route-pre-down` scripts for your network and review **every route and firewall command** before enabling full-tunnel operation.
4. Use the normal OpenVPN configuration directives to connect. For example:

```text
client
proto udp
dev tun
remote your.vpn.server.example 1194
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

Add **your own trusted CA, client certificate, private key, and any required `tls-crypt` material** in the form expected by the server. The example above is deliberately incomplete: it must not be treated as a ready-to-connect or secure standalone profile.

Start from an AmigaDOS Shell with sufficient stack space:

```text
Stack 262144
OPENVPN:openvpn --config OPENVPN:client.ovpn
```

## Compatibility and known limitations

- **Tested stack:** MiamiDX. SANA-II is a portable Amiga driver interface, but the complete integration and routing-socket backend have not yet been demonstrated on Roadshow, AmiTCP/Genesis, or other stacks.
- **IP family / device mode:** IPv4 TUN only. No IPv6 tunnel, TAP bridging, or OpenVPN DCO acceleration is claimed.
- **Routing automation:** the full-tunnel scripts work on the validated MiamiDX setup. Dynamic use of the newly discovered physical gateway throughout those scripts, edge cases with pre-existing routes, and robust rollback on partial failures are not yet validated.
- **DNS:** pushed resolver configuration and the UDP/53 firewall policy have been tested; comprehensive leak prevention, TCP/53 and IPv6 filtering, and restoration of pre-VPN DNS settings are not yet complete.
- **Lifecycle:** normal script-driven startup and teardown have been tested. Long-duration unattended reconnects, every provider's PUSH options, and exhaustive multi-provider compatibility remain open validation work.
- **Performance / stability:** the measured HTTP transfer is real, but it is not a long-duration stress test or proof of production readiness.
- **Security:** this is an experimental port running on a legacy OS. Keep key files protected, verify server certificates, review privileged networking scripts, and do not interpret hardware validation as a security audit.

## Why this matters

An Amiga 600 running a decades-old operating system is a **first-class endpoint of a modern encrypted network**. Its own TCP/IP stack routes the packets, its own 68k application negotiates TLS and performs encryption, and its own native devices and scripts manage the VPN interface. The PiStorm supplies the processing speed; it does not replace AmigaOS with a modern networking host.

The project demonstrates something larger than raw acceleration: **an extensible operating-system architecture, a portable network-device model, and a deliberately scriptable application can remain useful across extraordinary changes in hardware and software generations.**

---

*Based on OpenVPN 2.6.23, adapted for native AmigaOS 68k with AmiSSL and SANA-II. Hardware-validation status as of October 27, 2026.*

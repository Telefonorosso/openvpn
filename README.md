# openvpn
A real OpenVPN 2.6.23 client running as a native AmigaOS 68k application

Openvpn has reached the point this project was created to reach: **a real 2.6.23 client, running as a native AmigaOS 68k application, transporting ordinary IP packets through a modern encrypted tunnel.**

This is no longer merely a successful cross-compilation, a working `openvpn --version`, a UDP probe, a TLS handshake, or a simulated SANA-II interface. On real Amiga hardware, MiamiDX exchanges IPv4 packets with a native OpenVPN process through `vpn.device`. OpenVPN encrypts those packets and carries them over UDP to a Linux peer. The reverse path also works.

The same POC4 executable and FIX4 device were then connected to an **AWS-hosted OpenVPN server without recompiling or patching either binary**. Bidirectional ICMP succeeded, and a native AmigaOS HTTP server delivered a **16 MiB file across the encrypted Internet tunnel**.

For a machine whose original operating system predates modern VPN protocols by decades, this is a particularly meaningful boundary to cross: the Amiga is a genuine endpoint in the secure network, not a terminal attached to a VPN running somewhere else.

### The validated platform

| Component | Implementation |
| --- | --- |
| Physical computer | Commodore Amiga 600 |
| Accelerator / execution environment | PiStorm Classic, Raspberry Pi 3A+, Emu68 |
| Operating system / TCP/IP stack | AmigaOS 68k, MiamiDX |
| VPN client | OpenVPN 2.6.23, native Motorola 68k build |
| Cryptographic integration | AmiSSL v5 / OpenSSL-compatible API |
| Virtual IP interface | `vpn.device` FIX4, SANA-II, point-to-point IPv4 |
| Native TUN bridge | openVPN POC4, using `vpn.device` unit 1 |
| Validated remote peers | Linux laboratory server; AWS-hosted OpenVPN server |

**No Linux VPN daemon runs on the PiStorm Raspberry Pi on behalf of the Amiga.** The OpenVPN application, its protocol state machine, and its cryptographic processing run in the AmigaOS 68k environment, accelerated by Emu68.

### Architecture: one IP path, two SANA-II units

The design deliberately keeps the responsibilities separate:

1. **MiamiDX** owns the Amiga's network stack and exposes a point-to-point interface, `ptp0`.
2. **`vpn.device` FIX4** provides a genuine bidirectional SANA-II packet transport: MiamiDX opens **unit 0**, while openVPN opens **unit 1**.
3. **openVPN POC4** implements the Amiga-specific TUN backend. It reads plaintext IPv4 datagrams from unit 1 and supplies decrypted datagrams in the opposite direction.
4. **OpenVPN 2.6.23 and AmiSSL** handle the standard TLS and data-channel cryptography. The encrypted outer connection uses MiamiDX's normal BSD socket networking.
5. **The Linux or AWS OpenVPN server** terminates the UDP tunnel and delivers packets to its native TUN interface.

Only **raw IPv4 datagrams** cross the virtual device boundary: there is no Ethernet emulation, ARP layer, or artificial PPP negotiation. The SANA-II interface retains point-to-point semantics; FIX4 handles MiamiDX's PPP IPv4 packet type `0x0021`, while the native OpenVPN bridge uses IPv4 type `0x0800`.

This separation is one of the project's most important engineering decisions. `vpn.device` does not implement encryption. OpenVPN does not replace MiamiDX. The PiStorm does not masquerade as a separate network host. Each component has one clearly defined role.


### Hardware validation I — encrypted Linux laboratory tunnel

The first complete POC4 tunnel connected the Amiga's MiamiDX `ptp0` endpoint at **10.77.0.1** to Linux `tun0` at **10.77.0.2**, using an OpenVPN UDP session on port 1194.

The OpenVPN session completed authenticated TLS negotiation. The observed laboratory configuration used:

| Layer | Verified result |
| --- | --- |
| TLS control channel | TLS 1.3 |
| TLS cipher suite | `TLS_AES_256_GCM_SHA384` |
| Key agreement | X25519 |
| Authentication | X.509 certificate verification |
| VPN data channel | AES-128-GCM in the laboratory session |
| TUN payload | IPv4, 1400-byte interface MTU |

Linux `tcpdump` captured ICMP Echo Requests and Echo Replies on `tun0`, and the physical network carried OpenVPN UDP traffic between the endpoints. Bidirectional ping **10.77.0.1 ↔ 10.77.0.2** succeeded with no reported loss.

The essential result was not simply `Initialization Sequence Completed`. **Real AmigaOS-generated IP traffic reached the remote Linux TUN endpoint through OpenVPN and received real replies.**

### Hardware validation II — the same binaries, now over the Internet

The next test was more consequential. The already validated POC4 OpenVPN executable and `vpn.device` FIX4 were used against an **AWS OpenVPN server**. This was a configuration/integration exercise, **not a new build**.

The AWS connection authenticated successfully, negotiated a **TLS 1.3** session and an **AES-256-GCM VPN data channel**, and established the SANA-II-backed tunnel. The AWS configuration assigned the client **10.8.0.15/24**, with a tunnel gateway at **10.8.0.1**. Bidirectional ICMP between the Amiga and the AWS endpoint succeeded.

This matters because it moves the validation beyond a controlled LAN with a directly attached test server. The same AmigaOS implementation interoperated with a remote Internet-hosted OpenVPN endpoint using normal OpenVPN protocols and credentials.

### Hardware validation III — real encrypted HTTP traffic

Finally, the tunnel carried application data rather than just diagnostic ICMP.

The original native AmigaOS **`usbnethttp` HTTP server** served **16 MiB from Amiga to AWS**, where `curl` measured the received HTTP payload throughput. The VPN executable and driver were left unchanged; the variable in this comparison was the HTTP server's application send-chunk size.

| HTTP send chunk | AWS `curl` download speed | Effective payload rate | Approx. time for 16 MiB |
| --- | ---: | ---: | ---: |
| 8,192 bytes | 294,055 B/s | 2.35 Mbit/s | 57 s |
| 32,768 bytes | 327,130 B/s | 2.62 Mbit/s | 51 s |
| **65,536 bytes** | **357,107 B/s** | **2.86 Mbit/s** | **47 s** |

The best measured run delivered approximately **2.86 Mbit/s of end-to-end HTTP payload**, **21.4% above** the 8 KiB baseline. This is **not** a measurement of maximum VPN link capacity or cryptographic throughput in isolation; it is the observed throughput of an actual AmigaOS HTTP application across the encrypted tunnel.

During the transfer, Emu68's CPU load approached saturation, yet **iBrowse remained usable**. This is an encouraging practical observation, not a substitute for long-duration concurrency or stability testing.

### What is proven — and what is not

**Hardware-validated in this milestone:**

- Native AmigaOS 68k OpenVPN execution and authenticated encrypted sessions.
- Functioning bidirectional MiamiDX ↔ `vpn.device` ↔ OpenVPN IPv4 data path.
- Linux laboratory tunnel with successful two-way ICMP.
- AWS-hosted tunnel using the same application and driver binaries.
- Real, encrypted Amiga-to-AWS HTTP transfer of 16 MiB, with measured payload throughput.
- Preservation of the ordinary MiamiDX networking model instead of replacing the system's TCP/IP stack.

**Still outside the validated scope:**

- Generalized automatic address, route, DNS and interface lifecycle management. The original POC4 bridge has fixed laboratory assumptions.
- IPv6, layer-2 TAP bridging, OpenVPN DCO, and a production GUI or background service.
- Stress-tested long-running reconnect/recovery behavior, heavy concurrent traffic, and exhaustive resource-lifetime testing.
- Full optimization of the SANA-II receive path, packet queues, and high-load performance.

These limitations do not diminish the milestone; they define a stable boundary from which the next work can proceed without unnecessarily changing the already working tunnel.

### Why this milestone matters

openVPN began with an ambitious question: **could a classic Amiga participate directly in today's encrypted networks without outsourcing the VPN to a modern operating system?**

An Amiga 600 running AmigaOS and MiamiDX now establishes a standards-based, authenticated OpenVPN connection, exchanges real IP packets through a native SANA-II interface, reaches a server across the Internet, and serves files through that encrypted path. The work bridges a classic operating system's device and networking model with a contemporary security protocol — while keeping both sides recognizably themselves.


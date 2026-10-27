## Installation

Copy the following files and directories to their respective AmigaOS locations:

| File / Directory | Destination |
|---|---|
| `vpn.startup` | `S:` |
| `vpn.shutdown` | `S:` |
| `vpn.device` | `DEVS:Networks/` |
| `openvpn.miami` | `MIAMI:` |
| `PTP-Toggle.rexx` and its `.info` icon | Any directory of your choice |
| `OpenVPN/` directory | Any location of your choice |

Add the following assignment to `S:User-Startup`, replacing the placeholder with the actual location of the `OpenVPN` directory:

```amigados
Assign OPENVPN: <path-to-OpenVPN-directory>
```

### Starting and Stopping the VPN

`PTP-Toggle.rexx` provides a convenient way to **start and stop the VPN** directly from Workbench.

The same result can be achieved manually through MiamiDX by bringing the `ptp0` interface **ONLINE** to start the VPN or **OFFLINE** to stop it.

### Important Configuration Notes

**OpenVPN Server Configuration**

The supplied `server.conf` is designed for an **AWS-hosted Debian OpenVPN server**. If you are using a different server environment or network configuration, you will need to adapt both the server and client configurations accordingly.

You will also need the following certificates and cryptographic keys (or their equivalents), matching your OpenVPN server configuration:

- `ca.crt` — Certificate Authority certificate.
- `amiga1.crt` — Client certificate.
- `amiga1.key` — Client private key.
- `tls-crypt.key` — TLS control-channel encryption key.

**MiamiDX Network Configuration**

The supplied `openvpn.miami` configuration assumes that an **EtherLink III PCMCIA Ethernet adapter** is used as the physical Internet interface.

If your Amiga uses a different network adapter, you must adjust the MiamiDX configuration to match your hardware.

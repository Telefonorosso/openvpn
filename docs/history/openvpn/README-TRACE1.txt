AmiOpenVPN FIX27 / POC3 startup crash isolation (TRACE1)
=========================================================
DIAGNOSTIC ONLY. No networking/crypto algorithm changes. Does not touch vpn.device.

Purpose: mark the next startup stages after the existing O14 marker.
The last complete `AOVPN22 Oxx` marker in the new log narrows down the crash.

Prerequisite: Use an ISOLATED, KNOWN FIX27 source tree, not the experimental POC3 overlay.
The file to instrument must match the FIX27 milestone source exactly:
  src/openvpn/openvpn.c
  SHA256: 556a9efa5faf704998c0b3be37aef551a1161947e7a33f9d6e65c73d8abb1b55

From the root of the isolated OpenVPN 2.6.23 tree:

  sha256sum src/openvpn/openvpn.c

If the hash does not match, STOP. Do not overwrite or force the patch.
Copy poc3-startup-trace1.patch into that tree, then:

  patch --dry-run -p1 < poc3-startup-trace1.patch
  cp src/openvpn/openvpn.c src/openvpn/openvpn.c.FIX27_BACKUP
  patch -p1 < poc3-startup-trace1.patch

Build with the same FIX27 toolchain and script as before.
Do not run apply_overlay.py after patching: it could undo TRACE1.

If using the FIX27 script from the milestone directory:

  AMISSL_SDK=/root/openvpn/AmiSSL/Developer \
    ./AmiOpenVPN-2.6.23-68K-NATIVE-STARTUP-HARDWARE-VALIDATED-MILESTONE/tools/build_milestone.sh \
    ./openvpn-2.6.23

Transfer the new executable under a DIFFERENT name, keep validated FIX27 unchanged.
Use verb 3, temp-dir T: and the same lab files. Log the complete Guru ADDRESS as
well as the FIRST error code if possible (xxxxxxxx.yyyyyyyy).

A successful crossbuild is NOT hardware validation.

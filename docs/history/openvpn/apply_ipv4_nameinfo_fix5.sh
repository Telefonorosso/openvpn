#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
FILE='src/openvpn/socket.c'
PATCH='./poc3-ipv4-nameinfo-fix5.patch'
EXPECT='781f40ed429cf784b934f37b64be7442d774e22a895ee36f3fc16dd7550609e8'
[[ -f "$FILE" && -f "$PATCH" ]] || { echo 'ERROR: extract at OpenVPN source root'; exit 1; }
[[ $(sha256sum "$FILE" | cut -d' ' -f1) == "$EXPECT" ]] || { echo 'ERROR: socket.c does not match IPv4 FIX4; no changes made'; exit 1; }
grep -q 'N12 before socket phase2' src/openvpn/init.c || { echo 'ERROR: TRACE2 init.c missing'; exit 1; }
patch --batch --dry-run -p1 < "$PATCH"
cp -p "$FILE" "${FILE}.IPV4FIX4_BACKUP"
patch --batch -p1 < "$PATCH"
echo 'POC3 FIX5: IPv4 formatting workaround + R09-R11 markers installed; only socket.c changed.'

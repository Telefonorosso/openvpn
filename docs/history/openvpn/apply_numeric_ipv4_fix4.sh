#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
FILE1='src/openvpn/socket.c'
FILE2='src/openvpn/syshead.h'
PATCH='./poc3-numeric-ipv4-fix4.patch'
EXPECT1='6d4a44520a1db14dfe5c2d8fbca9140ab50aae53e496af29603a543b140aba15'
EXPECT2='cd75ddbb726afe0c0178ccd76ec6aca782b620a1a67721aa5badca30c8b22f0e'
[[ -f "$FILE1" && -f "$FILE2" && -f "$PATCH" ]] || { echo 'ERROR: extract at OpenVPN source root'; exit 1; }
[[ $(sha256sum "$FILE1" | cut -d' ' -f1) == "$EXPECT1" ]] || { echo 'ERROR: socket.c differs from TRACE3; no changes'; exit 1; }
[[ $(sha256sum "$FILE2" | cut -d' ' -f1) == "$EXPECT2" ]] || { echo 'ERROR: syshead.h differs from FIX27; no changes'; exit 1; }
grep -q 'N09 frame and buffers ready; before socket phase1' src/openvpn/init.c || { echo 'ERROR: TRACE2 init.c missing'; exit 1; }
patch --batch --dry-run -p1 < "$PATCH"
cp -p "$FILE1" "${FILE1}.TRACE3_BACKUP"
cp -p "$FILE2" "${FILE2}.FIX27_BACKUP"
patch --batch -p1 < "$PATCH"
echo 'POC3 numeric IPv4 FIX4 applied to socket.c + syshead.h only.'

#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
FILE='src/openvpn/socket.c'
PATCH='./poc3-startup-trace3.patch'
EXPECTED='d3cfbbf253d147b0368910307bada96458e27b73e46cf7792026b52f61c085e6'
if [ ! -f "$FILE" ] || [ ! -f "$PATCH" ]; then
  echo 'ERROR: unpack TRACE3 in the OpenVPN source tree root.' >&2
  exit 1
fi
if ! grep -q 'N09 frame and buffers ready; before socket phase1' src/openvpn/init.c; then
  echo 'ERROR: TRACE2 is not present in init.c; refusing to apply TRACE3.' >&2
  exit 1
fi
ACTUAL=$(sha256sum "$FILE" | cut -d' ' -f1)
if [ "$ACTUAL" != "$EXPECTED" ]; then
  echo "ERROR: unexpected socket.c hash: $ACTUAL" >&2
  echo 'No files changed. Do not force the patch.' >&2
  exit 1
fi
patch --batch --dry-run -p1 < "$PATCH"
cp -p "$FILE" "${FILE}.FIX27_BEFORE_TRACE3"
patch --batch -p1 < "$PATCH"
echo 'TRACE3 APPLIED: modified only src/openvpn/socket.c; saved original backup.'

#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
for file in src/openvpn/openvpn.c src/openvpn/init.c; do
  case "$file" in
    src/openvpn/openvpn.c) expected=f40dc402907fc71d46b60a45498f18bac86708e5ab92045354e8a66e01a165b7 ;;
    src/openvpn/init.c) expected=d200fa76a570de21857224058e059c2ef640c9dcdffcad9690e2f6e818cae84e ;;
  esac
  actual=$(sha256sum "$file" | cut -d ' ' -f 1)
  if [[ "$actual" != "$expected" ]]; then
    printf 'STOP: %s differs from the expected FIX27 + TRACE1 state.\n' "$file" >&2
    printf 'Expected: %s\nActual:   %s\n' "$expected" "$actual" >&2
    exit 3
  fi
done
patch --batch --dry-run -p1 < poc3-startup-trace2.patch
cp -p src/openvpn/openvpn.c src/openvpn/openvpn.c.TRACE1_BAK
cp -p src/openvpn/init.c src/openvpn/init.c.FIX27_BAK
patch --batch -p1 < poc3-startup-trace2.patch
echo 'TRACE2 APPLIED: only openvpn.c and init.c modified.'

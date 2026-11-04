#!/usr/bin/env python3
"""Fail if a BSD-licensed, literally reused sana2loop helper was edited."""
from pathlib import Path
import hashlib, re
root = Path(__file__).resolve().parents[1]
source = root / 'src/sana2_core.inc'
original = root / 'third_party/sana2loop/loopback_device.c'
names = ['safe_add_tail', 'safe_rem_head', 'safe_unlink_if_queued',
         'default_copy', 'list_init']
def extract(s, name):
    m = re.search(r'static\s+(?:(?:void|BOOL)\s+|struct Node\s*\*\s*)'+name+r'\s*\(',s)
    if not m: raise AssertionError('missing helper '+name)
    a=s.index('{',m.end()); depth=0
    for i in range(a,len(s)):
        if s[i]=='{': depth+=1
        elif s[i]=='}':
            depth-=1
            if depth==0: return s[m.start():i+1]
    raise AssertionError('unbalanced '+name)
a=source.read_text(); b=original.read_text()
for n in names:
    assert extract(a,n) == extract(b,n), 'modified reused helper: '+n
    print('PASS: exact sana2loop helper',n)
print('PASS: 1:1 function-level reuse integrity')

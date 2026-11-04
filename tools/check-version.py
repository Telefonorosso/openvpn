#!/usr/bin/env python3
"""Check the Amiga port version, upstream version and optional release tag."""
import argparse
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tag', help='Release tag, exactly v followed by VERSION')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    version = (root / 'VERSION').read_text().strip()
    match = re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)-amiga\.([1-9][0-9]*)', version)
    if not match:
        parser.error('VERSION must be MAJOR.MINOR.PATCH-amiga.REVISION (positive revision)')
    source = (root / 'src/openvpn/version.m4').read_text()
    parts = []
    for name in ('MAJOR', 'MINOR', 'PATCH'):
        part = re.search(r'define\(\[PRODUCT_VERSION_' + name + r'\], \[([^]]+)\]\)', source)
        if not part:
            parser.error('Cannot parse upstream PRODUCT_VERSION_' + name)
        parts.append(part.group(1).lstrip('.'))
    if '.'.join(parts) != '.'.join(match.groups()[:3]):
        parser.error('VERSION upstream component does not match src/openvpn/version.m4')
    if args.tag is not None and args.tag != 'v' + version:
        parser.error('Tag must be v' + version)
    print(version)


if __name__ == '__main__':
    main()

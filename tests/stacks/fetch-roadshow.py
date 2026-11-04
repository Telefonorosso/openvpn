#!/usr/bin/env python3
"""Fetch the pinned, byte-identical public Roadshow demo from its distribution snapshot."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, default=Path('build/stack-dependencies'))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    pin = json.loads((root / 'ci/stack-dependencies.json').read_text())['roadshow']
    args.cache.mkdir(parents=True, exist_ok=True)
    archive = args.cache / pin['archive']
    if not archive.exists():
        partial = archive.with_suffix(archive.suffix + '.part')
        with urllib.request.urlopen(pin['url'], timeout=60) as response, partial.open('wb') as out:
            shutil.copyfileobj(response, out)
        if hashlib.sha256(partial.read_bytes()).hexdigest() != pin['sha256']:
            raise SystemExit('Roadshow demo SHA256 mismatch; no test input accepted')
        partial.replace(archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != pin['sha256']:
        raise SystemExit('Cached Roadshow demo SHA256 mismatch')
    print(archive.resolve())


if __name__ == '__main__':
    main()

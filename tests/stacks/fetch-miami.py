#!/usr/bin/env python3
"""Fetch only the pinned public MiamiDX demo inputs; never keys or OS images."""
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
    pins = json.loads((root / 'ci/stack-dependencies.json').read_text())['miami']
    args.cache.mkdir(parents=True, exist_ok=True)
    for kind in ('main', 'gui'):
        pin = pins[kind]
        path = args.cache / pin['archive']
        if not path.exists():
            partial = path.with_suffix(path.suffix + '.part')
            with urllib.request.urlopen(pin['url'], timeout=60) as response, partial.open('wb') as out:
                shutil.copyfileobj(response, out)
            partial.replace(path)
        if hashlib.sha256(path.read_bytes()).hexdigest() != pin['sha256']:
            raise SystemExit('Miami ' + kind + ' SHA256 mismatch; test input refused')
        print(path.resolve())


if __name__ == '__main__':
    main()

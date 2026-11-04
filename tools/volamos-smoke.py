#!/usr/bin/env python3
"""Execute the real target FIFO regression with volamos memory checking."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--program', type=Path, required=True)
    parser.add_argument('--results', type=Path, required=True)
    args = parser.parse_args()
    args.results.mkdir(parents=True, exist_ok=True)
    command = [str(args.runtime.resolve()), '--cpu', '68020', '--stack', '256K',
        '--ram', '16M', '--sanitize', '--dirty-heap']
    started = time.monotonic()
    with tempfile.TemporaryDirectory(prefix='volamos-', dir=args.results.resolve().parent) as temp:
        command += ['--volumes-dir', str(Path(temp) / 'volumes'), str(args.program.resolve())]
        with (args.results / 'guest.log').open('w') as log:
            try:
                code = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                    env=dict(os.environ, TMPDIR=temp), timeout=120).returncode
            except subprocess.TimeoutExpired:
                code = None
    text = (args.results / 'guest.log').read_text(errors='replace')
    result = {'test': 'target FIFO under volamos sanitizer and dirty heap',
        'returncode': code, 'passed': code == 0 and 'PASS: bounded FIFO' in text,
        'elapsed_seconds': time.monotonic() - started,
        'program_sha256': hashlib.sha256(args.program.read_bytes()).hexdigest(),
        'runtime_sha256': hashlib.sha256(args.runtime.read_bytes()).hexdigest(),
        'command': command}
    (args.results / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    print('volamos FIFO: ' + ('PASS' if result['passed'] else 'FAIL'))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

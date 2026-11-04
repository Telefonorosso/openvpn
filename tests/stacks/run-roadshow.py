#!/usr/bin/env python3
"""Test the fresh driver against a real, isolated Roadshow demo; retain no demo binaries."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator', type=Path, required=True)
    parser.add_argument('--bin-dir', type=Path, required=True)
    parser.add_argument('--demo-archive', type=Path, required=True)
    parser.add_argument('--results', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    pin = json.loads((root / 'ci/stack-dependencies.json').read_text())['roadshow']
    if digest(args.demo_archive) != pin['sha256']:
        parser.error('The demo archive does not match the pinned public test input')
    if not shutil.which('lha'):
        parser.error('Install lhasa (Linux) or lha (Homebrew)')
    args.results.mkdir(parents=True, exist_ok=True)
    result = {'stack': pin['version'], 'archive_sha256': pin['sha256'], 'passed': False}
    with tempfile.TemporaryDirectory(prefix='roadshow-', dir=args.results.resolve().parent) as temp:
        stage = Path(temp)
        subprocess.run(['lha', 'xq', str(args.demo_archive.resolve())], cwd=stage, check=True)
        library_dir = stage / 'Roadshow-Demo-1.15/Workbench/Libs'
        (stage / 'Libs').mkdir()
        (stage / 'Env').mkdir()
        for name in ('bsdsocket.library', 'usergroup.library'):
            shutil.copy2(library_dir / name, stage / 'Libs' / name)
        for name in ('roadshow_smoke', 'vpn.device'):
            source = args.bin_dir / name
            if source.read_bytes()[:4] != b'\0\0\x03\xf3':
                parser.error('Invalid Amiga HUNK input: ' + str(source))
            shutil.copy2(source, stage / name)
            result[name + '_sha256'] = digest(source)
        result['library_sha256'] = digest(stage / 'Libs/bsdsocket.library')
        result['emulator_sha256'] = digest(args.emulator)
        command = [str(args.emulator.resolve()), '--factory', '--model', 'A1200',
            '--cpu', '68020', '--fast', '8M', '--run', str(stage / 'roadshow_smoke'),
            '--run-stack', '262144', '--exit-on-return', '--noaudio',
            '--screenshot-after', '60', str((args.results / 'timeout.png').resolve())]
        with (args.results / 'emulator.log').open('w') as log:
            try:
                code = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=180).returncode
            except subprocess.TimeoutExpired:
                code = None
        result['returncode'] = code
        guest_log = stage / 'roadshow-guest.log'
        text = guest_log.read_text(errors='replace') if guest_log.is_file() else ''
        (args.results / 'guest.log').write_text(text)
        result['passed'] = code == 0 and 'PASS: Roadshow stack integration complete' in text and 'FAIL:' not in text
    (args.results / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    print('Roadshow: ' + ('PASS' if result['passed'] else 'FAIL'))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())

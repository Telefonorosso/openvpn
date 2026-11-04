#!/usr/bin/env python3
"""Run real MiamiDX in an isolated licensed AmigaOS/MUI runtime; no private artifacts."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


from miami_runtime import digest, stage_miami


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator', type=Path, required=True)
    parser.add_argument('--bin-dir', type=Path, required=True)
    parser.add_argument('--main-archive', type=Path, required=True)
    parser.add_argument('--gui-archive', type=Path, required=True)
    parser.add_argument('--guest-system', type=Path, required=True, help='Authorized AmigaOS tree with Libs, Fonts and System/MUI')
    parser.add_argument('--rom', type=Path, required=True, help='Authorized matching Kickstart ROM, not uploaded')
    parser.add_argument('--results', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    pin = json.loads((root / 'ci/stack-dependencies.json').read_text())['miami']
    for kind, archive in [('main', args.main_archive), ('gui', args.gui_archive)]:
        if digest(archive) != pin[kind]['sha256']:
            parser.error('Miami ' + kind + ' archive does not match the pinned public test input')
    args.results.mkdir(parents=True, exist_ok=True)
    result = {'stack': 'MiamiDX ' + pin['version'], 'passed': False,
        'main_sha256': digest(args.main_archive), 'gui_sha256': digest(args.gui_archive),
        'rom_sha256': digest(args.rom), 'emulator_sha256': digest(args.emulator)}
    with tempfile.TemporaryDirectory(prefix='miami-', dir=args.results.resolve().parent) as temp:
        stage = Path(temp)
        stage_miami(stage, args, root, result)
        command = [str(args.emulator.resolve()), '--factory', '--model', 'A1200', '--cpu', '68020',
            '--fast', '8M', str(args.rom.resolve()), '--run', str(stage / 'miami_smoke'),
            '--run-stack', '262144', '--exit-on-return', '--noaudio', '--screenshot-after', '60',
            str((args.results / 'timeout.png').resolve())]
        with (args.results / 'emulator.log').open('w') as log:
            try: code = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=180).returncode
            except subprocess.TimeoutExpired: code = None
        result['returncode'] = code
        guest = stage / 'miami-guest.log'
        text = guest.read_text(errors='replace') if guest.is_file() else ''
        (args.results / 'guest.log').write_text(text)
        child = stage / 'miami-child.log'
        if child.is_file(): shutil.copy2(child, args.results / 'miami-child.log')
        result['passed'] = (code == 0 and 'PASS: Miami stack roundtrip and reconnect complete' in text
            and text.count('PASS: UDP socket roundtrip through both vpn.device units') == 2 and 'FAIL:' not in text)
    (args.results / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    print('Miami: ' + ('PASS' if result['passed'] else 'FAIL'))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())

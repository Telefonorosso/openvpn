#!/usr/bin/env python3
"""Run real MiamiDX in an isolated licensed AmigaOS/MUI runtime; no private artifacts."""
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
        for archive in (args.main_archive, args.gui_archive):
            subprocess.run(['lha', 'xq', str(archive.resolve())], cwd=stage, check=True)
        source = stage / 'MiamiDx10_Install'
        for name in ('MiamiDx', 'MiamiIfConfig'):
            shutil.copy2(source / 'main' / name, stage / name)
        for name in ('Env', 'Scripts', 'Devs/Networks', 'Runtime/Libs', 'Runtime/System/MUI'):
            (stage / name).mkdir(parents=True, exist_ok=True)
        libraries = ('asl', 'iffparse', 'rexxsyslib', 'mathieeedoubbas', 'mathieeedoubtrans',
            'locale', 'commodities', 'datatypes', 'diskfont', 'realtime', 'reqtools')
        result['runtime_sha256'] = {}
        for name in libraries:
            file = args.guest_system / 'Libs' / (name + '.library')
            if file.is_file():
                shutil.copy2(file, stage / 'Runtime/Libs' / file.name)
                result['runtime_sha256']['Libs/' + file.name] = digest(file)
        for name in ('Libs', 'Images', 'Locale'):
            folder = args.guest_system / 'System/MUI' / name
            if folder.is_dir():
                shutil.copytree(folder, stage / 'Runtime/System/MUI' / name,
                    ignore=shutil.ignore_patterns('*.key', '*.uaem'))
        shutil.copytree(args.guest_system / 'System/MUI/Libs', stage / 'Runtime/Libs', dirs_exist_ok=True,
            ignore=shutil.ignore_patterns('*.key', '*.uaem'))
        result['runtime_sha256']['System/MUI/Libs/muimaster.library'] = digest(stage / 'Runtime/Libs/muimaster.library')
        shutil.copytree(args.guest_system / 'Fonts', stage / 'Runtime/Fonts')
        for name in ('MUI.MiamiDxGui', 'MUI.MiamiToolsGui'):
            shutil.copy2(source / 'MUI' / name, stage / 'Runtime/Libs' / name)
        for name in ('miami_smoke', 'vpn.device'):
            file = args.bin_dir / name
            if file.read_bytes()[:4] != b'\0\0\x03\xf3': parser.error('Invalid Amiga HUNK: ' + str(file))
            shutil.copy2(file, stage / name)
            result[name + '_sha256'] = digest(file)
        shutil.copy2(stage / 'vpn.device', stage / 'Devs/Networks/vpn.device')
        shutil.copy2(root / 'stacks/miamidx/openvpn.miami', stage / 'openvpn.miami')
        for name in ('vpn.startup', 'vpn.shutdown'):
            (stage / 'Scripts' / name).write_text('; test-only no-op hook: no provider connection\n')
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

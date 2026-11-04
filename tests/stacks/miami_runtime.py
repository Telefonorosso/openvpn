"""Private disposable Miami runtime shared by the two real-stack checks."""
import hashlib
from pathlib import Path
import shutil
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def stage_miami(stage, args, root, result, probe='miami_smoke'):
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
    for name in (probe, 'vpn.device'):
        file = args.bin_dir / name
        if file.read_bytes()[:4] != b'\0\0\x03\xf3': raise ValueError('Invalid Amiga HUNK: ' + str(file))
        shutil.copy2(file, stage / name)
        result[name + '_sha256'] = digest(file)
    shutil.copy2(stage / 'vpn.device', stage / 'Devs/Networks/vpn.device')
    shutil.copy2(root / 'stacks/miamidx/openvpn.miami', stage / 'openvpn.miami')
    for name in ('vpn.startup', 'vpn.shutdown'):
        (stage / 'Scripts' / name).write_text('; test-only no-op hook: no provider connection\n')

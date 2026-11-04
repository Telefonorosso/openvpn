#!/usr/bin/env python3
"""Fetch the exact public GCC and AmiSSL SDK assets named in ci/dependencies.json."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import urllib.request

def download(asset, cache):
    target = cache / asset['url'].rsplit('/', 1)[1]
    if not target.exists():
        partial = target.with_suffix(target.suffix + '.part')
        with urllib.request.urlopen(asset['url'], timeout=60) as response, partial.open('wb') as out:
            shutil.copyfileobj(response, out)
        partial.replace(target)
    with target.open('rb') as archive_file:
        digest = hashlib.file_digest(archive_file, 'sha256').hexdigest()
    if digest != asset['sha256']:
        raise SystemExit('SHA256 mismatch: ' + str(target))
    return target

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, default=Path('build/dependencies'))
    parser.add_argument('--github-env', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    pins = json.loads((root / 'ci/dependencies.json').read_text())
    machine = {'aarch64': 'arm64', 'AMD64': 'x86_64'}.get(platform.machine(), platform.machine())
    key = platform.system() + '-' + machine
    if key not in pins['gcc']:
        raise SystemExit('No pinned AmigaPorts GCC asset for ' + key)
    cache = args.cache.resolve()
    cache.mkdir(parents=True, exist_ok=True)
    gcc_archive = download(pins['gcc'][key], cache)
    sdk_archive = download(pins['amissl'], cache)
    gcc_dir = cache / pins['gcc'][key]['sha256'][:12]
    sdk_dir = cache / pins['amissl']['sha256'][:12]
    gcc_root = gcc_dir / 'm68k-amigaos-gcc-16.2'
    sdk_root = sdk_dir / 'AmiSSL/Developer'
    if not (gcc_root / 'bin/m68k-amigaos-gcc').is_file():
        gcc_dir.mkdir(exist_ok=True)
        with tarfile.open(gcc_archive) as archive:
            archive.extractall(gcc_dir, filter='data')
    if not (sdk_root / 'include/proto/amissl.h').is_file():
        if not shutil.which('lha'):
            raise SystemExit('Install lhasa (Linux) or lha (Homebrew) to unpack the pinned SDK')
        sdk_dir.mkdir(exist_ok=True)
        subprocess.run(['lha', 'xq', str(sdk_archive)], cwd=sdk_dir, check=True)
    compiler = gcc_root / 'bin/m68k-amigaos-gcc'
    version = subprocess.check_output([str(compiler), '--version'], text=True).splitlines()[0]
    result = {'AMIGA_GCC_ROOT': str(gcc_root), 'AMISSL_SDK': str(sdk_root)}
    if args.github_env:
        with args.github_env.open('a') as out:
            for name, value in result.items():
                if '\n' in value or '\r' in value:
                    raise SystemExit('Invalid dependency path')
                out.write(name + '=' + value + '\n')
    (cache / 'provenance.json').write_text(json.dumps({'gcc': pins['gcc'][key],
        'compiler': version, 'amissl': pins['amissl'], 'paths': result}, indent=2) + '\n')
    print(json.dumps(dict(result, compiler=version), indent=2))

if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Build/test/package the Amiga port using only the repository's pinned inputs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def run(*command, env=None):
    print('+ ' + ' '.join(map(str, command)), flush=True)
    subprocess.run(list(map(str, command)), cwd=ROOT, env=env, check=True)


def source_identity():
    # A source archive may live inside an unrelated Git checkout. Never
    # accidentally identify that enclosing repository as this project's SHA.
    if not shutil.which('git'):
        return None, None
    def git(*args):
        return subprocess.check_output(['git', *args], cwd=ROOT,
            stderr=subprocess.DEVNULL, text=True).strip()
    try:
        if Path(git('rev-parse', '--show-toplevel')).resolve() != ROOT:
            return None, None
        return git('rev-parse', 'HEAD'), not bool(git('status', '--porcelain'))
    except subprocess.CalledProcessError:
        return None, None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--host-only', action='store_true', help='Run the three native host regressions only')
    mode.add_argument('--cross-only', action='store_true', help='Build/package Amiga targets and run volamos only')
    parser.add_argument('--require-clean-source', action='store_true',
        help='Refuse archives and dirty checkouts (CI/release preparation)')
    parser.add_argument('--jobs', type=int, default=2, help='Parallel compiler jobs (default: 2)')
    args = parser.parse_args()
    if sys.version_info < (3, 12):
        parser.error('Python 3.12 or later is required')
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    if args.require_clean_source and source_identity()[1] is not True:
        parser.error('A clean Git checkout of this project is required; archive/dirty identity is not release evidence')
    tools = ['cmake'] + ([] if args.cross_only else ['ctest'])
    if not args.host_only:
        tools += ['cpack', 'lha']
    missing = [name for name in tools if not shutil.which(name)]
    if missing:
        parser.error('Install the documented prerequisites first: ' + ', '.join(missing))
    run(sys.executable, ROOT / 'tools/check-version.py')
    if not args.cross_only:
        build = ROOT / 'build/host'
        run('cmake', '--fresh', '-S', ROOT, '-B', build,
            '-DAOVPN_HOST_TESTS_ONLY=ON', '-DCMAKE_BUILD_TYPE=Release')
        run('cmake', '--build', build, '--parallel', args.jobs)
        run('ctest', '--test-dir', build, '--output-on-failure', '--output-junit', 'host-results.xml')
    if args.host_only:
        print('PASS: host regressions; no Amiga build or emulator coverage requested')
        return 0
    # Fetcher checks every archive hash and returns the resolved SDK/compiler
    # paths. No copied shell exports or developer-specific paths are needed.
    output = subprocess.check_output([sys.executable, str(ROOT / 'tools/fetch-deps.py'),
        '--cache', str(ROOT / 'build/dependencies'), '--with-volamos'], cwd=ROOT, text=True)
    dependencies = json.loads(output)
    env = dict(os.environ)
    for name in ('AMIGA_GCC_ROOT', 'AMISSL_SDK'):
        env[name] = dependencies[name]
    build = ROOT / 'build/amiga'
    run('cmake', '--fresh', '-S', ROOT, '-B', build,
        '-DCMAKE_TOOLCHAIN_FILE=' + str(ROOT / 'cmake/amigaports.cmake'),
        '-DAMISSL_SDK=' + dependencies['AMISSL_SDK'], '-DCMAKE_BUILD_TYPE=Release', env=env)
    run('cmake', '--build', build, '--parallel', args.jobs, env=env)
    run(sys.executable, ROOT / 'tools/volamos-smoke.py', '--runtime', dependencies['VOLAMOS_EXECUTABLE'],
        '--program', build / 'bin/ring_smoke', '--results', ROOT / 'build/volamos-results')
    run('cmake', '--install', build, '--prefix', ROOT / 'build/package')
    packages = ROOT / 'build/packages'
    run('cpack', '--config', build / 'CPackConfig.cmake', '-B', packages)
    shutil.copy2(ROOT / 'build/dependencies/provenance.json', packages / 'dependency-provenance.json')
    commit, clean = source_identity()
    if args.require_clean_source and clean is not True:
        raise SystemExit('Build changed the source tree; refusing verified candidate metadata')
    identity = commit if clean else ((commit + '+dirty') if commit else 'UNVERIFIED-SOURCE-ARCHIVE')
    (packages / 'SOURCE_COMMIT').write_text(identity + '\n')
    (build / 'bin/SOURCE_COMMIT').write_text(identity + '\n')
    version = (ROOT / 'VERSION').read_text().strip()
    (packages / 'source-provenance.json').write_text(json.dumps({
        'source_commit': commit, 'source_clean': clean, 'version': version}, indent=2) + '\n')
    lines = []
    for suffix in ('.zip', '.tar.gz'):
        file = packages / ('AmiOpenVPN-' + version + '-AmigaOS-m68k' + suffix)
        lines.append(hashlib.sha256(file.read_bytes()).hexdigest() + '  ' + file.name)
    (packages / 'SHA256SUMS').write_text('\n'.join(lines) + '\n')
    print('PASS: Amiga build, volamos FIFO and packages: ' + str(packages))
    print('Source identity: ' + identity)
    print('Full-system Roadshow/Miami tests are separate; see tests/stacks/README.md')
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as error:
        print('FAILED: command exited ' + str(error.returncode) + '; stop and read the first error above.', file=sys.stderr)
        raise SystemExit(error.returncode if error.returncode > 0 else 1)

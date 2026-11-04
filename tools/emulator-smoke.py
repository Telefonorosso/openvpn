#!/usr/bin/env python3
"""Run the compiled driver/FIFO tests in pinned Copperline; no private ROM or network."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import shutil

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--emulator', type=Path, required=True)
    parser.add_argument('--bin-dir', type=Path, required=True)
    parser.add_argument('--results', type=Path, required=True)
    args = parser.parse_args()
    args.results.mkdir(parents=True, exist_ok=True)
    emulator = args.emulator.resolve()
    provenance = {'emulator_sha256': hashlib.sha256(emulator.read_bytes()).hexdigest(),
        'pins': json.loads((Path(__file__).resolve().parents[1] / 'ci/dependencies.json').read_text())['copperline'],
        'rom_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted((emulator.parent / 'aros').glob('*.bin'))}}
    (args.results / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
    results = []
    failures = False
    for name in ('ring_smoke', 'vpn_device_smoke'):
        binary = (args.bin_dir / name).resolve()
        device = (args.bin_dir / 'vpn.device').resolve()
        for file in (binary, device):
            if not file.is_file() or file.read_bytes()[:4] != b'\0\0\x03\xf3':
                raise SystemExit('Missing or invalid Amiga hunk executable: ' + str(file))
        command = [str(args.emulator.resolve()), '--factory', '--model', 'A1200',
            '--cpu', '68020', '--fast', '8M', '--run', str(binary),
            '--run-stack', '262144', '--exit-on-return', '--noaudio',
            '--screenshot-after', '60', str((args.results / (name + '-timeout.png')).resolve())]
        log = args.results / (name + '.log')
        guest_log = args.bin_dir / 'vpn-device-guest.log'
        if name == 'vpn_device_smoke':
            guest_log.unlink(missing_ok=True)
        timed_out = False
        with log.open('w') as output:
            try:
                outcome = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT, timeout=180)
                code = outcome.returncode
            except subprocess.TimeoutExpired:
                code = None
                timed_out = True
        guest_ok = True
        if name == 'vpn_device_smoke':
            guest_ok = guest_log.is_file()
            if guest_ok:
                text = guest_log.read_text(errors='replace')
                guest_ok = ('PASS: vpn.device SANA-II bidirectional IP transport' in text
                            and 'FAIL:' not in text)
                shutil.copy2(guest_log, args.results / guest_log.name)
        result = {'test': name, 'returncode': code, 'wall_timeout': timed_out,
            'sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
            'device_sha256': hashlib.sha256(device.read_bytes()).hexdigest(),
            'passed': code == 0 and not timed_out and guest_ok, 'command': command}
        results.append(result)
        failures |= not result['passed']
        print(name + ': ' + ('PASS' if result['passed'] else 'FAIL'), flush=True)
    (args.results / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    return 1 if failures else 0

if __name__ == '__main__':
    sys.exit(main())

"""Drive the pinned emulator and approve only Miami's known lab DNS requester."""
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import time

# Visually checked stock Kickstart 3.1 requester, not a generic "press Yes".
# RGBA FNV-1a digest of the complete security-warning text, excluding pointer
# and title bar. PAL captures alternate between 284 and 285 lines.
# Use control captures, not the CLI's separately scaled screenshots. A
# different dialog/theme must fail rather than be accepted.
DNS_RECT = {'x': 94, 'y': 36, 'w': 418, 'h': 90}
DNS_DIGEST = '481575eb466daa2d'


def run_guest(command, stage, results, timeout=420):
    info_file = stage / 'control.json'
    command = command + ['--control', '127.0.0.1:0', '--control-info', str(info_file)]
    raw_log = stage / 'emulator-private.log'
    metadata = {'dns_confirmations': 0, 'guest_status_source': 'fresh Copperline Done numeric line'}
    code, connection, stream, rpc = None, None, None, None
    started = time.monotonic()
    with raw_log.open('w') as log:
        process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
            env=dict(os.environ, COPPERLINE_DBG_RR_BUDGET_MB='64',
                COPPERLINE_DBG_RR_INTERVAL='1000000'))
        marker = Path(command[0]).parent / 'run' / ('boot-' + str(process.pid)) / 'done'
        try:
            for _ in range(200):
                if process.poll() is not None:
                    raise RuntimeError('Emulator exited before control was ready')
                try:
                    info = json.loads(info_file.read_text())
                    break
                except (FileNotFoundError, json.JSONDecodeError):
                    time.sleep(.05)
            else:
                raise RuntimeError('Emulator control startup timeout')
            # The headless control server starts paused; discard any old PID's
            # marker before the first instruction, never after execution starts.
            marker.unlink(missing_ok=True)
            host, port = info['listen'].rsplit(':', 1)
            connection = socket.create_connection((host, int(port)), timeout=30)
            connection.settimeout(30)
            stream = connection.makefile('rwb')
            sequence = 0
            def rpc(method, params=None):
                nonlocal sequence
                sequence += 1
                stream.write((json.dumps({'jsonrpc': '2.0', 'id': sequence,
                    'method': method, 'params': params or {}}) + '\n').encode())
                stream.flush()
                while True:
                    line = stream.readline()
                    if not line:
                        raise RuntimeError('Emulator control connection ended')
                    reply = json.loads(line)
                    if reply.get('id') != sequence:
                        continue
                    if 'error' in reply:
                        raise RuntimeError(method + ': ' + reply['error']['message'])
                    return reply['result']
            hello = rpc('hello', {'token': info['token']})
            if not hello.get('authed') or hello.get('proto') != 1:
                raise RuntimeError('Unexpected emulator control protocol')
            was_dialog = False
            while time.monotonic() - started < timeout:
                if marker.is_file():
                    text = marker.read_text(errors='replace')
                    if '\n' in text and re.fullmatch(r'[+-]?\d+', text.split('\n', 1)[0].strip()):
                        code = int(text.split('\n', 1)[0])
                        break
                guest = stage / 'miami-vpn-guest.log'
                if guest.is_file() and 'FAIL:' in guest.read_text(errors='replace'):
                    raise RuntimeError('Guest reported a failure; see its retained log')
                status = rpc('status')
                (stage / 'control-status.json').write_text(json.dumps(status))
                metadata['last_emulated_seconds'] = status['seconds']
                if status.get('double_faulted'):
                    raise RuntimeError('Guest CPU double fault')
                rpc('run_until', {'seconds': status['seconds'] + 5})
                region = rpc('capture.region_digest', DNS_RECT)
                is_dialog = (region['width'] == 716 and region['height'] in (284, 285)
                    and region['digest'] == DNS_DIGEST)
                if is_dialog and not was_dialog:
                    if metadata['dns_confirmations'] >= 2:
                        raise RuntimeError('Unexpected extra DNS confirmation')
                    # Standard Intuition leftmost-requester shortcut. The
                    # exact warning guard above is mandatory before input.
                    rpc('input.key', {'rawkey': 0x66, 'action': 'press'})  # left Amiga
                    rpc('step_frame', {'n': 4})
                    rpc('input.key', {'rawkey': 0x34, 'action': 'press'})  # V
                    rpc('step_frame', {'n': 4})
                    rpc('input.key', {'rawkey': 0x34, 'action': 'release'})
                    rpc('input.key', {'rawkey': 0x66, 'action': 'release'})
                    rpc('step_frame', {'n': 8})
                    metadata['dns_confirmations'] += 1
                was_dialog = is_dialog
            else:
                raise RuntimeError('Guest wall-clock deadline exceeded')
            rpc('shutdown')
        except (OSError, RuntimeError, ValueError) as error:
            metadata['error'] = str(error)
            # GUI diagnostics can include registration identity. Keep them in a
            # private sibling directory that the workflow never uploads.
            if rpc:
                try:
                    private = results.parent / 'miami-vpn-private-diagnostics'
                    private.mkdir(mode=0o700, exist_ok=True)
                    rpc('capture.screenshot', {'path': str(private / 'failure.png')})
                except (OSError, RuntimeError, ValueError):
                    pass
        finally:
            if stream:
                try:
                    stream.close()
                except OSError:
                    pass
            if connection:
                connection.close()
            if process.poll() is None:
                process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
    text = raw_log.read_text(errors='replace')
    text = re.sub(r'token=[0-9a-fA-F]{32}', 'token=[REDACTED]', text)
    (results / 'emulator.log').write_text(text)
    # The portable boot directory was created for this exact process/probe.
    boot = marker.parent
    startup = boot / 'S/Startup-Sequence'
    if startup.is_file() and Path(command[command.index('--run') + 1]).name in startup.read_text(errors='replace'):
        shutil.rmtree(boot)
    return code, metadata

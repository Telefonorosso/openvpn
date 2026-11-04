#!/usr/bin/env python3
"""Real encrypted OpenVPN/Miami test in a disposable Linux network namespace."""
import argparse
import datetime
import json
import os
from pathlib import Path
import platform
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
import urllib.request
from miami_runtime import digest, stage_miami
from miami_ui import run_guest


def fetch(asset, cache):
    file = cache / asset['url'].rsplit('/', 1)[1]
    if not file.is_file():
        part = file.with_suffix(file.suffix + '.part')
        with urllib.request.urlopen(asset['url'], timeout=60) as source, part.open('wb') as out:
            shutil.copyfileobj(source, out)
        part.replace(file)
    if digest(file) != asset['sha256']:
        raise ValueError('Checksum mismatch: ' + file.name)
    return file


def credentials(folder):
    folder.mkdir(mode=0o700)
    def ssl(*args):
        subprocess.run(['openssl', *args], cwd=folder, check=True,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    ssl('req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-keyout', 'ca.key',
        '-out', 'ca.crt', '-days', '2', '-subj', '/CN=OpenVPN isolated CI CA')
    for name, usage in [('server', 'serverAuth'), ('client', 'clientAuth')]:
        ssl('req', '-new', '-newkey', 'rsa:2048', '-nodes', '-keyout', name + '.key',
            '-out', name + '.csr', '-subj', '/CN=' + name)
        (folder / (name + '.ext')).write_text('basicConstraints=critical,CA:FALSE\n'
            'keyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=' + usage + '\n')
        ssl('x509', '-req', '-in', name + '.csr', '-CA', 'ca.crt', '-CAkey', 'ca.key',
            '-CAcreateserial', '-out', name + '.crt', '-days', '2', '-extfile', name + '.ext')


def prepare(stage, args, root, pin, cache, result):
    stage_miami(stage, args, root, result, 'miami-vpn_smoke')
    original = (stage / 'openvpn.miami').read_bytes()
    # Adapt only the published physical Ethernet device/address/gateway to NAT.
    # These are equal-length replacements inside the original IFF records.
    changes = [(b'3c589.device', b'a2065.device', 2),
        (bytes([192, 168, 1, 29]), bytes([10, 0, 2, 15]), 2),
        (bytes([192, 168, 1, 1]), bytes([10, 0, 2, 2]), 2)]
    prefs = original
    for old, new, count in changes:
        if len(old) != len(new) or prefs.count(old) != count:
            raise ValueError('Published Miami fixture changed; review its IFF records')
        prefs = prefs.replace(old, new)
    (stage / 'openvpn.miami').write_bytes(prefs)
    for name in ('MIAMI.KEY1', 'MIAMI.KEY2', 'MIAMIDX.KEY'):
        files = [p for p in args.keys_dir.iterdir() if p.name.upper() == name]
        if len(files) != 1:
            raise ValueError('Supply the authorized MiamiDX registration files in --keys-dir')
        shutil.copy2(files[0], stage / name)
    shutil.copy2(args.network_device, stage / 'Devs/Networks/a2065.device')
    result['network_device_sha256'] = digest(args.network_device)
    result['registered_runtime'] = True  # Never record key bytes or identifying hashes.
    source = stage / 'MiamiDx10_Install/main'
    for name in ('MiamiRoute', 'MiamiIPFW', 'MiamiNetStat'):
        shutil.copy2(source / name, stage / name)
    (stage / 'Runtime/C').mkdir()
    for name in ('Execute', 'Echo', 'If', 'Else', 'EndIf', 'FailAt', 'SetEnv', 'UnsetEnv',
        'Delete', 'Type', 'Break', 'Status', 'Run', 'Wait', 'Stack', 'Assign', 'Date'):
        file = args.guest_system / 'C' / name
        if file.is_file():
            shutil.copy2(file, stage / 'Runtime/C' / name)
    (stage / 'Runtime/Rexxc').mkdir()
    shutil.copy2(args.guest_system / 'Rexxc/RX', stage / 'Runtime/C/rx')
    shutil.copy2(args.guest_system / 'Rexxc/RXC', stage / 'Runtime/C/rxc')
    shutil.copy2(args.guest_system / 'System/RexxMast', stage / 'Runtime/Rexxc/RexxMast')
    (stage / 'Tmp').mkdir()
    (stage / 'Scripts/miami.startup').write_text('; probe sets up isolated NAT Ethernet\n')
    for name in ('vpn.startup', 'vpn.shutdown'):
        shutil.copy2(root / 'stacks/miamidx' / name, stage / 'Scripts' / name)
    (stage / 'OpenVPN').mkdir()
    for name in ('Up.script', 'RouteUp.script', 'RoutePreDown.script'):
        shutil.copy2(root / 'stacks/miamidx' / name, stage / 'OpenVPN' / name)
    shutil.copy2(args.bin_dir / 'openvpn', stage / 'OpenVPN/openvpn')
    result['openvpn_sha256'] = digest(args.bin_dir / 'openvpn')
    cred = stage / 'credentials'
    credentials(cred)
    # Date is second-granularity and guest clocks may lag host generation.
    # Set a bounded time inside the fresh certificates' two-day validity.
    clock = datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(minutes=10)
    (stage / 'set-clock.script').write_text(clock.strftime('Date %d-%b-%y %H:%M:%S') + '\n')
    server_root = stage / 'server-root'
    server_root.mkdir()
    for package in pin['openvpn_server']['packages']:
        subprocess.run(['dpkg-deb', '-x', str(fetch(package, cache)), str(server_root)], check=True)
    server = server_root / 'usr/sbin/openvpn'
    server_env = dict(os.environ, LD_LIBRARY_PATH=str(server_root / 'usr/lib/x86_64-linux-gnu'))
    server_version = subprocess.check_output([str(server), '--version'], env=server_env, text=True).splitlines()
    result['server_version'] = server_version[0]
    result['server_libraries'] = server_version[1]
    subprocess.run([str(server), '--genkey', 'secret', str(cred / 'tls-crypt.key')], env=server_env, check=True)
    for name in ('ca.crt', 'client.crt', 'client.key', 'tls-crypt.key'):
        shutil.copy2(cred / name, stage / 'OpenVPN' / name)
    client = (root / 'examples/AWS.ovpn').read_text().replace(
        'blah.eu-west-3.compute.amazonaws.com 1194', '203.0.113.10 11940').replace(
        'amiga1.crt', 'client.crt').replace('amiga1.key', 'client.key')
    # Interpreted 68020 RSA signing can exceed OpenVPN's default 60s window.
    # Extend timing only; retain CA, EKU, TLS-crypt and cipher verification.
    client += '\nhand-window 180\n'
    (stage / 'OpenVPN/AWS.ovpn').write_text(client)
    (stage / 'server.conf').write_text(f'''local 203.0.113.10
port 11940
proto udp
dev tun
server 10.8.0.0 255.255.255.0
topology subnet
dh none
ca {cred}/ca.crt
cert {cred}/server.crt
key {cred}/server.key
tls-crypt {cred}/tls-crypt.key
auth SHA512
data-ciphers AES-256-GCM:AES-128-GCM
tun-mtu 1400
push "dhcp-option DNS 10.8.0.1"
push "redirect-gateway def1 bypass-dhcp"
keepalive 10 120
hand-window 180
verb 4
''')
    ssl = stage / 'ssl-extract'
    ssl.mkdir()
    subprocess.run(['lha', 'xq', str(fetch(pin['amissl_runtime'], cache))], cwd=ssl, check=True)
    lib = ssl / 'AmiSSL/Libs/AmigaOS3'
    shutil.copy2(lib / 'amisslmaster.library', stage / 'Runtime/Libs/amisslmaster.library')
    (stage / 'Runtime/Libs/AmiSSL').mkdir()
    shutil.copy2(lib / 'AmiSSL/68020-40/amissl_v362.library', stage / 'Runtime/Libs/AmiSSL/amissl_v362.library')
    (stage / 'Runtime/SSL/Certs').mkdir(parents=True)
    shutil.copy2(ssl / 'AmiSSL/C/openssl.cnf', stage / 'Runtime/SSL/openssl.cnf')
    result['amissl_runtime_sha256'] = pin['amissl_runtime']['sha256']
    return server, server_env


def peers(folder):
    stop = threading.Event()
    sockets = []
    threads = []
    def listen(address, port, callback):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind((address, port))
        sock.settimeout(.2)
        sockets.append(sock)
        def loop():
            while not stop.is_set():
                try:
                    data, peer = sock.recvfrom(65535)
                except socket.timeout:
                    continue
                callback(sock, data, peer)
        thread = threading.Thread(target=loop, daemon=True)
        thread.start()
        threads.append(thread)
    def echo(sock, data, peer):
        # A cleartext NAT shortcut arrives with the host's address, not the
        # assigned tunnel-client address. Never answer or count that traffic.
        if peer[0] != '10.8.0.2':
            return
        sock.sendto(data, peer)
        with (folder / 'echo.log').open('a') as log:
            log.write(str(peer) + ' ' + repr(data) + '\n')
    listen('10.8.0.1', 41195, echo)
    for address in ('192.168.1.1', '8.8.8.8', '193.206.165.32', '10.8.0.1'):
        def dns(sock, data, peer, address=address):
            if len(data) < 17 or (address == '10.8.0.1' and peer[0] != '10.8.0.2'):
                return
            end = 12
            while end < len(data) and data[end]:
                end += data[end] + 1
            end += 5
            if end > len(data):
                return
            answer = '10.8.0.1' if address == '10.8.0.1' else '10.0.2.2'
            question = data[12:end]
            reply = data[:2] + struct.pack('!HHHHH', 0x8180, 1, 1, 0, 0) + question
            reply += bytes.fromhex('c00c00010001000000000004') + socket.inet_aton(answer)
            sock.sendto(reply, peer)
            with (folder / 'dns.log').open('a') as log:
                log.write(address + ' ' + repr(question) + ' -> ' + answer + '\n')
        listen(address, 53, dns)
    return stop, sockets, threads



def check_routes(folder):
    def rows(name):
        text = (folder / name).read_text()
        if 'Destination' not in text or 'Gateway' not in text:
            raise ValueError('Missing route table: ' + name)
        return {(p[0], p[1], p[5]) for line in text.splitlines()
            if len(p := line.split()) >= 6 and p[0] != 'Destination'}
    physical = ('default', '10.0.2.2', 'eth0')
    if physical not in rows('routes-before.log'):
        raise ValueError('Initial physical default route missing')
    for cycle in (1, 2):
        connected = rows(f'routes-connected-{cycle}.log')
        # Miami abbreviates the zero /1 network as "default" in netstat.
        expected = {physical, ('default', '10.8.0.1', 'ptp0'),
            ('128/1', '10.8.0.1', 'ptp0'), ('203.0.113.10', '10.0.2.2', 'eth0')}
        if not expected <= connected:
            raise ValueError('VPN/endpoint route missing during cycle ' + str(cycle))
        after = rows(f'routes-after-{cycle}.log')
        if physical not in after or any(p[2] == 'ptp0' or p[0] == '203.0.113.10' for p in after):
            raise ValueError('Disconnect did not restore routes in cycle ' + str(cycle))


def run(args):
    root = Path(__file__).resolve().parents[2]
    pin = json.loads((root / 'ci/stack-dependencies.json').read_text())
    for name, file in [('main', args.main_archive), ('gui', args.gui_archive)]:
        if digest(file) != pin['miami'][name]['sha256']:
            raise ValueError('Miami public archive checksum mismatch')
    args.results.mkdir(parents=True, exist_ok=True)
    args.cache.mkdir(parents=True, exist_ok=True)
    result = {'passed': False, 'test': 'Miami encrypted VPN with production hooks',
        'rom_sha256': digest(args.rom), 'emulator_sha256': digest(args.emulator),
        'namespace_isolated': True}
    if args.source_commit:
        actual = (args.bin_dir / 'SOURCE_COMMIT').read_text().strip()
        if actual != args.source_commit:
            raise ValueError('Cross-build source commit mismatch')
        result['source_commit'] = actual
    with tempfile.TemporaryDirectory(prefix='miami-vpn-', dir=args.results.resolve().parent) as temp:
        stage = Path(temp)
        server, env = prepare(stage, args, root, pin, args.cache.resolve(), result)
        subprocess.run(['ip', 'link', 'set', 'lo', 'up'], check=True)
        for address in ('192.168.1.1', '8.8.8.8', '193.206.165.32', '203.0.113.10'):
            subprocess.run(['ip', 'addr', 'add', address + '/32', 'dev', 'lo'], check=True)
        stop, sockets, threads = None, [], []
        with (args.results / 'server.log').open('w') as log:
            process = subprocess.Popen([str(server), '--config', str(stage / 'server.conf')],
                env=env, stdout=log, stderr=subprocess.STDOUT)
            try:
                for _ in range(100):
                    if process.poll() is not None:
                        raise RuntimeError('OpenVPN server failed to start')
                    if 'Initialization Sequence Completed' in (args.results / 'server.log').read_text():
                        break
                    time.sleep(.1)
                else:
                    raise RuntimeError('OpenVPN server startup timeout')
                stop, sockets, threads = peers(args.results)
                command = [str(args.emulator.resolve()), '--factory', '--model', 'A1200', '--cpu', '68020',
                    '--fast', '8M', '--accelerator', '32M', '--a2065-net', 'nat', str(args.rom.resolve()),
                    '--run', str(stage / 'miami-vpn_smoke'), '--run-stack', '262144',
                    '--noaudio']
                code, ui_result = run_guest(command, stage, args.results)
                result.update(ui_result)
                result['returncode'] = code
                # Whitelist diagnostics: never collect registration files, private
                # certificates, the vendor's license banner or its GUI screenshot.
                files = ['miami-vpn-guest.log', 'client-live.log', 'client-1.log', 'client-2.log', 'client-final.log',
                    'routes-before.log', 'routes-connected-1.log', 'routes-connected-2.log',
                    'routes-after-1.log', 'routes-after-2.log']
                for name in files:
                    if (stage / name).is_file():
                        shutil.copy2(stage / name, args.results / name)
                text = (stage / 'miami-vpn-guest.log').read_text(errors='replace') if (stage / 'miami-vpn-guest.log').is_file() else ''
                result['passed'] = (code == 0 and 'error' not in ui_result and 'PASS: Miami encrypted VPN and production reconnect complete' in text
                    and text.count('PASS: application UDP reply through encrypted OpenVPN session') == 2
                    and 'FAIL:' not in text)
                if result['passed']:
                    try:
                        check_routes(args.results)
                        result['routes_verified'] = True
                    except (OSError, ValueError) as error:
                        result['passed'] = False
                        result['route_error'] = str(error)
            finally:
                if stop:
                    stop.set()
                    for thread in threads:
                        thread.join(timeout=1)
                    for sock in sockets:
                        sock.close()
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
    (args.results / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    print('Miami encrypted VPN: ' + ('PASS' if result['passed'] else 'FAIL'))
    return 0 if result['passed'] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('emulator', 'bin-dir', 'main-archive', 'gui-archive', 'guest-system', 'rom', 'keys-dir', 'network-device', 'results'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--cache', type=Path, default=Path('build/stack-dependencies'))
    parser.add_argument('--source-commit')
    parser.add_argument('--inside-namespace', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    if platform.system() != 'Linux' or platform.machine() not in ('x86_64', 'amd64'):
        parser.error('This isolated server lane currently requires Linux x86_64 and unprivileged user/network namespaces')
    if not args.inside_namespace:
        # Download and verify public inputs before removing external networking.
        args.cache.mkdir(parents=True, exist_ok=True)
        pin = json.loads((Path(__file__).resolve().parents[2] / 'ci/stack-dependencies.json').read_text())
        for asset in [*pin['openvpn_server']['packages'], pin['amissl_runtime']]:
            fetch(asset, args.cache)
        environment = dict(os.environ, AOVPN_PARENT_NETNS=str(os.stat('/proc/self/ns/net').st_ino))
        return subprocess.run(['unshare', '-Urn', sys.executable, str(Path(__file__).resolve()),
            *sys.argv[1:], '--inside-namespace'], env=environment).returncode
    parent_namespace = os.environ.get('AOVPN_PARENT_NETNS')
    if not parent_namespace or parent_namespace == str(os.stat('/proc/self/ns/net').st_ino):
        parser.error('Refusing to configure the host network namespace')
    return run(args)


if __name__ == '__main__':
    raise SystemExit(main())

#!/usr/bin/env python3
"""Check a controller binary or runtime image without starting real nodes."""
import argparse
import base64
import http.client
import os
import socket
import subprocess
import tempfile
import time
import uuid

parser = argparse.ArgumentParser()
parser.add_argument('binary', nargs='?')
parser.add_argument('--image')
args = parser.parse_args()
if bool(args.binary) == bool(args.image):
    parser.error('provide a binary path or --image')

env = dict(os.environ)
env.pop('CONTROLLER_PASSWORD', None)
command = (['docker', 'run', '--rm', '--network', 'none', args.image]
           if args.image else [os.path.abspath(args.binary)])
result = subprocess.run(command + ['--require-auth'], env=env, capture_output=True,
                        text=True, timeout=60)
assert result.returncode == 1 and 'CONTROLLER_PASSWORD must be set' in result.stderr, result.stderr

with tempfile.TemporaryDirectory() as directory:
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]
    password = 'local-review-password'
    name = 'controller-auth-' + uuid.uuid4().hex[:12]
    if args.image:
        command = ['docker', 'run', '--rm', '--name', name, '--user', '1000:1000',
                   '-p', f'127.0.0.1:{port}:8080', '-e', 'CONTROLLER_PASSWORD=' + password,
                   '-e', 'GRIN_RUST_BIN=/nonexistent', '-e', 'GRINPP_BIN=/nonexistent',
                   args.image, '--require-auth']
    else:
        env.update(CONTROLLER_PASSWORD=password, HOME=directory,
                   GRIN_RUST_DATADIR=directory, GRINPP_DATADIR=directory,
                   GRIN_RUST_BIN='/nonexistent', GRINPP_BIN='/nonexistent')
        command += ['--require-auth', '--port', str(port)]
    process = subprocess.Popen(command, env=env, cwd=directory,
                               stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)

    def request(method, path, secret=None):
        headers = {}
        if secret is not None:
            token = base64.b64encode(('grin:' + secret).encode()).decode()
            headers['Authorization'] = 'Basic ' + token
        conn = http.client.HTTPConnection('127.0.0.1', port, timeout=3)
        try:
            conn.request(method, path, headers=headers)
            response = conn.getresponse()
            response.read()
            return response.status
        finally:
            conn.close()

    try:
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise AssertionError(process.stderr.read())
            try:
                assert request('GET', '/status') == 401
                break
            except (ConnectionRefusedError, ConnectionResetError, TimeoutError):
                time.sleep(0.1)
        else:
            raise AssertionError('Controller did not start')
        for method, path in [('GET', '/status'), ('GET', '/logs/rust'),
                             ('POST', '/start/rust'), ('POST', '/stop/rust'),
                             ('POST', '/restart/rust'), ('POST', '/delete/rust'),
                             ('POST', '/v2/owner'), ('POST', '/v2/foreign')]:
            assert request(method, path) == 401, path
            assert request(method, path, 'wrong') == 401, path
        assert request('GET', '/status', password) == 200
        assert request('OPTIONS', '/v2/owner') == 204
    finally:
        if args.image:
            subprocess.run(['docker', 'rm', '-f', name], stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL, timeout=30)
        else:
            process.terminate()
        process.wait(timeout=10)
        process.stderr.close()
print('Controller authentication checks passed')

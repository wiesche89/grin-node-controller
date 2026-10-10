#!/usr/bin/env python3
"""Check wallet credential forwarding against a local node stub."""
import base64
import http.client
import http.server
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading
import time

key = 'node-test-key'
expected = 'Basic ' + base64.b64encode(('grin:' + key).encode()).decode()
received = []


class Node(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        self.rfile.read(int(self.headers.get('Content-Length', 0)))
        auth = self.headers.get('Authorization')
        received.append((self.path, auth))
        self.send_response(200 if auth == expected else 401)
        self.end_headers()
        self.wfile.write(b'{}')

    def log_message(self, *_):
        pass


with tempfile.TemporaryDirectory() as directory:
    Path(directory, '.foreign_api_secret').write_text(key)
    node = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Node)
    threading.Thread(target=node.serve_forever, daemon=True).start()
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]
    env = dict(os.environ, HOME=directory, GRIN_RUST_DATADIR=directory,
               GRINPP_DATADIR=directory)
    env.pop('CONTROLLER_PASSWORD', None)
    process = subprocess.Popen([
        os.path.abspath(sys.argv[1]), '--port', str(port),
        '--node-port', str(node.server_port), '--rust-bin', '/bin/sleep',
        '--rust-args', '60', '--grinpp-bin', '/nonexistent',
    ], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def request(method, path, auth=None):
        conn = http.client.HTTPConnection('127.0.0.1', port, timeout=5)
        try:
            conn.request(method, path, '{}', {'Authorization': auth} if auth else {})
            response = conn.getresponse()
            response.read()
            return response.status
        finally:
            conn.close()

    try:
        for _ in range(50):
            try:
                assert request('GET', '/status') == 200
                break
            except ConnectionRefusedError:
                time.sleep(0.1)
        else:
            raise AssertionError('Controller did not start')
        assert request('POST', '/start/rust') == 200
        for auth in [None, 'Basic invalid', expected]:
            assert request('POST', '/wallet/v2/foreign', auth) == (200 if auth == expected else 401)
            assert received[-1] == ('/v2/foreign', auth)
        assert request('POST', '/v2/foreign') == 200
        assert received[-1] == ('/v2/foreign', expected)
        count = len(received)
        for method, path in [('GET', '/wallet/v2/foreign'),
                             ('POST', '/wallet/v2/owner'), ('POST', '/wallet/delete/rust')]:
            assert request(method, path) == 404
        assert len(received) == count
    finally:
        process.terminate()
        process.wait(timeout=10)
        node.shutdown()
        node.server_close()
print('Wallet forwarding checks passed')

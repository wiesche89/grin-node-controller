#!/usr/bin/env python3
"""Exercise node controls while idle clients and delayed RPCs are connected."""
import concurrent.futures
import http.client
import http.server
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading
import time

entered = threading.Event()
release = threading.Event()
fast_entered = threading.Event()

class Node(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        body = self.rfile.read(int(self.headers.get('Content-Length', 0)))
        if body == b'slow':
            entered.set()
            release.wait(10)
        elif body == b'fast':
            fast_entered.set()
            entered.wait(2)
        self.send_response(200)
        self.end_headers()
        self.wfile.write(b'{}')
    def log_message(self, *_):
        pass

with tempfile.TemporaryDirectory() as directory:
    node = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Node)
    threading.Thread(target=node.serve_forever, daemon=True).start()
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]
    env = dict(os.environ, HOME=directory, GRIN_RUST_DATADIR=directory,
               GRINPP_DATADIR=directory)
    env.pop('CONTROLLER_PASSWORD', None)
    process = subprocess.Popen([
        os.path.abspath(sys.argv[1]), '--port', str(port), '--node-port', str(node.server_port),
        '--rust-bin', '/bin/sleep', '--rust-args', '60',
        '--grinpp-bin', '/bin/sleep', '--grinpp-args', '60',
    ], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    def request(method, path, body='{}', timeout=3):
        conn = http.client.HTTPConnection('127.0.0.1', port, timeout=timeout)
        try:
            conn.request(method, path, body)
            reply = conn.getresponse()
            payload = reply.read()
            assert reply.status == 200, (path, reply.status, payload)
            return json.loads(payload)
        finally:
            conn.close()
    try:
        for _ in range(50):
            try:
                request('GET', '/status')
                break
            except ConnectionRefusedError:
                time.sleep(.1)
        # An idle connection must not block other clients.
        with socket.create_connection(('127.0.0.1', port)) as idle:
            time.sleep(.1)
            request('GET', '/status', timeout=1)
        # Proxy replies must finish independently, not unwind nested event loops.
        assert request('POST', '/start/rust')['ok']
        with concurrent.futures.ThreadPoolExecutor() as pool:
            fast = pool.submit(request, 'POST', '/v2/owner', 'fast', 2)
            assert fast_entered.wait(2)
            slow = pool.submit(request, 'POST', '/v2/owner', 'slow', 12)
            try:
                assert entered.wait(2)
                fast.result()
                request('GET', '/status', timeout=1)
            finally:
                release.set()
            slow.result()
        for kind in ('rust', 'grinpp'):
            for _ in range(2):
                assert request('POST', '/start/' + kind)['ok']
                assert request('GET', '/status')['nodes'][kind]['running']
                assert request('POST', '/stop/' + kind, timeout=2)['ok']
                assert not request('GET', '/status')['nodes'][kind]['running']
        print('Lifecycle checks passed: idle clients, concurrent RPC, repeated start/stop.')
    finally:
        release.set()
        process.terminate()
        process.wait(timeout=10)
        node.shutdown()
        node.server_close()

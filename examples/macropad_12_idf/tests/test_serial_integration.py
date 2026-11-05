"""Test the real serial reader with a local pseudo-terminal, never a physical port."""
import json
import os
from pathlib import Path
import pty
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
import unittest
import urllib.error
import urllib.request

PROJECT = Path(__file__).resolve().parents[1]

class SerialIntegrationTest(unittest.TestCase):
    def test_explicit_port_and_legacy_packets(self):
        master, slave = pty.openpty()
        port = os.ttyname(slave)
        other_master, other_slave = pty.openpty()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            shutil.copy(PROJECT / 'server.py', root / 'server.py')
            (root / 'main').mkdir()
            for name in ('index.html', 'studio.html'):
                shutil.copy(PROJECT / 'main' / name, root / 'main' / name)
            with socket.socket() as sock:
                sock.bind(('127.0.0.1', 0))
                webport = sock.getsockname()[1]
            process = subprocess.Popen([sys.executable, '-u', 'server.py', '--port', port,
                                        '--http-port', str(webport)], cwd=root,
                                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            try:
                deadline = time.monotonic() + 8
                result = None
                while time.monotonic() < deadline:
                    # A second attached port must not be consumed when an
                    # explicit sensor port was chosen.
                    os.write(other_master, b'K,99,9,9,9,9,9,9,9,9,9,9,9,9\n')
                    os.write(master, b'K,42,100,101,102,103,104,105,106,107,108,109,110,111\n')
                    try:
                        with urllib.request.urlopen(f'http://127.0.0.1:{webport}/state', timeout=0.3) as reply:
                            state = json.load(reply)
                        if state.get('connected'):
                            result = state
                            break
                    except (urllib.error.URLError, TimeoutError, ConnectionError):
                        pass
                    time.sleep(0.05)
                self.assertIsNotNone(result, 'server did not publish the emulated sensor packet')
                self.assertEqual(result['values'], list(range(100, 112)))
                self.assertEqual(result['port'], port)
                self.assertIsInstance(result['sample_id'], int)
                # Historical unsequenced format must continue to work.
                deadline = time.monotonic() + 4
                result = None
                while time.monotonic() < deadline:
                    os.write(master, b'K,200,201,202,203,204,205,206,207,208,209,210,211\n')
                    try:
                        with urllib.request.urlopen(f'http://127.0.0.1:{webport}/state', timeout=0.3) as reply:
                            state = json.load(reply)
                        if state.get('values') == list(range(200, 212)):
                            result = state
                            break
                    except (urllib.error.URLError, TimeoutError, ConnectionError):
                        pass
                    time.sleep(0.05)
                self.assertIsNotNone(result, 'legacy packet was not published')
                self.assertEqual(result['port'], port)
            finally:
                process.send_signal(signal.SIGINT)
                try:
                    process.wait(timeout=4)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
                os.close(master)
                os.close(slave)
                os.close(other_master)
                os.close(other_slave)

if __name__ == '__main__':
    unittest.main()

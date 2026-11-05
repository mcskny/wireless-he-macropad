#!/usr/bin/env python3
"""USB sensör verilerini yalnızca bu bilgisayardaki tarayıcıya aktarır."""
import argparse
import glob
import json
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import serial

PAGE = (Path(__file__).parent / 'main' / 'index.html').read_bytes()
condition = threading.Condition()
state = {'connected': False}
version = 0

def publish(data):
    global state, version
    with condition:
        state = data
        version += 1
        condition.notify_all()

def read_serial(port):
    while True:
        ports = [port] if port else sorted(glob.glob('/dev/cu.usbserial*') + glob.glob('/dev/cu.usbmodem*'))
        if not ports:
            publish({'connected': False})
            time.sleep(1)
            continue
        try:
            with serial.Serial(ports[0], 115200, timeout=1) as device:
                print(f'Seri port açık: {ports[0]}', flush=True)
                last = time.monotonic()
                while True:
                    line = device.readline().decode('ascii', errors='ignore').strip().split(',')
                    if len(line) == 3 and line[0] == 'S':
                        try:
                            raw, tcs = int(line[1]), int(line[2])
                            if 0 <= raw <= 4095 and tcs in (0, 1):
                                publish({'connected': True, 'raw': raw, 'tcs': tcs})
                                last = time.monotonic()
                        except ValueError:
                            pass
                    if time.monotonic() - last > 2:
                        raise TimeoutError('ESP32 verisi gelmiyor')
        except (serial.SerialException, OSError, TimeoutError) as e:
            print(f'Seri bağlantı bekleniyor: {e}', flush=True)
            publish({'connected': False})
            time.sleep(1)

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/':
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.send_header('Content-Length', str(len(PAGE)))
            self.end_headers()
            self.wfile.write(PAGE)
        elif self.path == '/events':
            self.send_response(200)
            self.send_header('Content-Type', 'text/event-stream')
            self.send_header('Cache-Control', 'no-cache')
            self.end_headers()
            seen = -1
            try:
                while True:
                    with condition:
                        condition.wait_for(lambda: version != seen, timeout=1)
                        seen = version
                        payload = json.dumps(state)
                    self.wfile.write(f'data: {payload}\n\n'.encode())
                    self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError, OSError):
                pass
        else:
            self.send_error(404)
    def log_message(self, format, *args):
        pass

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', help='ESP32 seri portu (varsayılan: otomatik)')
    parser.add_argument('--http-port', type=int, default=8765)
    args = parser.parse_args()
    threading.Thread(target=read_serial, args=(args.port,), daemon=True).start()
    server = ThreadingHTTPServer(('127.0.0.1', args.http_port), Handler)
    print(f'Tarayıcı: http://127.0.0.1:{args.http_port}', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()

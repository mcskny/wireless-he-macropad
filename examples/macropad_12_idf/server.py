#!/usr/bin/env python3
"""USB verilerini yerel arayüze aktarır ve oturumları JSONL olarak kaydeder."""
import argparse
import binascii
import secrets
import glob
import json
import threading
import time
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import serial

ROOT = Path(__file__).resolve().parent
PAGE = (ROOT / 'main' / 'index.html').read_bytes()
DEMO_PAGE = (ROOT / 'main' / 'studio.html').read_bytes()
condition = threading.Condition()
stop = threading.Event()
state = {'connected': False}
rate_status = None
rate_status_at = 0.0
bridge_status = None
bridge_status_at = 0.0
layer_status = None
layer_status_at = 0.0
lighting_profiles = None
pending_layer = None
pending_layer_until = 0.0
pending_events = []
hid_write = None
hid_write_status = {'state':'idle'}
version = 0

def publish(data):
    global state, version
    with condition:
        version += 1
        # Monotonic sample ID lets optional game output detect stale sensor data.
        # Existing clients ignore this additional field.
        if data.get('connected') and 'values' in data:
            data = {**data, 'sample_id': version}
        state = data
        condition.notify_all()

def timestamp():
    return datetime.now().astimezone().isoformat(timespec='milliseconds')

class SessionLog:
    def __init__(self, port):
        logdir = ROOT / 'logs'
        logdir.mkdir(exist_ok=True)
        self.path = logdir / (datetime.now().astimezone().strftime('%Y%m%d_%H%M%S_%f') + '.jsonl')
        self.file = self.path.open('x', encoding='utf-8', buffering=1)
        self.started = time.monotonic()
        self.last_valid = self.started
        self.samples = self.invalid = self.missing = self.resets = 0
        self.last_seq = None
        self.sequenced = self.unsequenced = 0
        self.write({'event': 'start', 'port': port})
        print(f'Kayıt başladı: {self.path}', flush=True)

    def write(self, data):
        data['time'] = timestamp()
        data['elapsed_s'] = round(time.monotonic() - self.started, 3)
        self.file.write(json.dumps(data, ensure_ascii=False, separators=(',', ':')) + '\n')

    def sample(self, seq, values):
        if seq is not None:
            self.sequenced += 1
            if self.last_seq is not None:
                delta = (seq - self.last_seq) & 0xffffffff
                if 1 < delta < 0x80000000:
                    self.missing += delta - 1
                    self.write({'event': 'gap', 'after': self.last_seq, 'before': seq, 'missing': delta - 1})
                elif delta == 0 or delta >= 0x80000000:
                    self.resets += 1
                    self.write({'event': 'sequence_restart_or_duplicate', 'previous': self.last_seq, 'sequence': seq})
            self.last_seq = seq
        else:
            self.unsequenced += 1
            self.last_seq = None
        self.samples += 1
        self.last_valid = time.monotonic()
        self.write({'event': 'sample', 'sequence': seq, 'values': values})

    def bad(self, line):
        self.invalid += 1
        self.write({'event': 'invalid_packet', 'line': line[:500]})

    def close(self, reason):
        duration = max(0, self.last_valid - self.started)
        self.write({'event': 'end', 'reason': reason, 'signal_duration_s': round(duration, 3),
                    'valid_packets': self.samples, 'invalid_packets': self.invalid,
                    'sequenced_packets': self.sequenced, 'unsequenced_packets': self.unsequenced,
                    'missing_packets_detected': self.missing, 'sequence_restarts_or_duplicates': self.resets,
                    'completeness': 'unknown' if self.unsequenced else 'sequence_gaps_only'})
        self.file.close()
        print(f'Kayıt bitti: {self.path} · {duration:.2f} s · {self.samples} paket · {self.missing} sıra boşluğu', flush=True)

def parse_bridge_diagnostics(line):
    """Parse optional per-second S3 diagnostics; malformed lines are ignored."""
    allowed = {'rx', 'invalid', 'sequence_gaps', 'restarts', 'pending', 'usb', 'usb_drop'}
    fields = line.split(',')
    if not fields or fields[0] != 'D':
        return None
    result = {}
    for field in fields[1:]:
        key, sep, value = field.partition('=')
        if not sep or key not in allowed or key in result or not value.isascii() or not value.isdecimal():
            return None
        number = int(value)
        if number > 0xffffffff:
            return None
        result[key] = number
    return result if result.keys() == allowed else None

def parse_rate_status(line):
    parts = line.split(',')
    if len(parts) not in (11,12) or parts[0] != 'P':
        return None
    try:
        nums = [int(value) for value in parts[1:]]
    except ValueError:
        return None
    epoch, active, app, gate, valid, scans, sent, skipped, replaced, interval = nums[:10]
    if not 0 <= epoch <= 0xffffffff or any(v not in (0, 1) for v in (active, app, gate, valid)):
        return None
    if any(v < 0 or v > 65535 for v in (scans, sent, skipped, replaced, interval)):
        return None
    if valid and not 100 <= interval <= 10000:
        return None
    gate_on_us=nums[10] if len(nums)==11 else None
    if gate_on_us is not None and (gate_on_us<0 or gate_on_us>10002000 or (valid and gate_on_us>interval*1000+2000)):
        return None
    return {'epoch': epoch, 'active': bool(active), 'app_open': bool(app),
            'gate_enabled': bool(gate), 'stats_valid': bool(valid), 'scans': scans,
            'submitted': sent, 'skipped': skipped, 'replaced': replaced, 'interval_ms': interval,'gate_on_us':gate_on_us}

def read_serial(port, studio_heartbeat=False):
    global rate_status, rate_status_at, bridge_status, bridge_status_at
    global layer_status, layer_status_at
    global pending_layer, pending_events, hid_write, hid_write_status
    while not stop.is_set():
        ports = [port] if port else sorted(glob.glob('/dev/cu.usbserial*') + glob.glob('/dev/cu.usbmodem*'))
        if not ports:
            publish({'connected': False})
            stop.wait(1)
            continue
        log = None
        reason = 'sunucu durduruldu'
        try:
            with serial.Serial(ports[0], 115200, timeout=0.5) as device:
                print(f'Seri port açık: {ports[0]}', flush=True)
                last = time.monotonic()
                next_heartbeat = 0.0
                next_lighting = 0.0
                next_control = 0.0
                while not stop.is_set():
                    now = time.monotonic()
                    with condition:
                        if hid_write is not None and now>=hid_write['next']:
                            if hid_write['attempts']>=8:
                                hid_write_status={'state':'error','message':'S3 did not acknowledge the write.'};hid_write=None
                            else:
                                device.write((hid_write['commands'][hid_write['index']][0]+'\n').encode('ascii'))
                                hid_write['attempts']+=1;hid_write['next']=now+0.4
                    if studio_heartbeat and now >= next_heartbeat:
                        device.write(b'APP,1\n')
                        next_heartbeat = now + 0.25
                    if studio_heartbeat and now >= next_lighting:
                        with condition:
                            profiles=lighting_profiles
                        if profiles is not None:
                            reference_ms=int((time.time()-978307200)*1000)
                            device.write(('LIGHT,'+profiles.hex()+','+str(reference_ms)+'\n').encode('ascii'))
                        next_lighting=now+1.0
                    if studio_heartbeat and now >= next_control:
                        with condition:
                            control=None
                            if pending_layer is not None and now>pending_layer_until:pending_layer=None
                            if pending_layer is not None:
                                control=(1,pending_layer,pending_layer,0)
                            elif pending_events:
                                item=pending_events[0];control=(item[4],item[0],item[1],item[2]);item[3]-=1
                                if item[3]<=0:pending_events.pop(0)
                        if control is not None:device.write(('CTRL,%d,%d,%d,%d\n'%control).encode('ascii'))
                        next_control=now+0.025
                    raw_line = device.readline().decode('ascii', errors='replace').strip()
                    if raw_line.startswith('K,'):
                        if log is None:
                            log = SessionLog(ports[0])
                        parts = raw_line.split(',')
                        try:
                            if len(parts) == 14:
                                seq = int(parts[1])
                                if not 0 <= seq <= 0xffffffff:
                                    raise ValueError('sequence out of range')
                                values = [int(x) for x in parts[2:]]
                            elif len(parts) == 13:  # önceki kart yazılımı
                                seq = None
                                values = [int(x) for x in parts[1:]]
                            else:
                                raise ValueError('wrong channel count')
                            if not all(0 <= value <= 4095 for value in values):
                                raise ValueError('ADC out of range')
                            log.sample(seq, values)
                            # Optional metadata: existing clients ignore this field.
                            publish({'connected': True, 'values': values, 'port': ports[0]})
                            last = time.monotonic()
                        except ValueError:
                            log.bad(raw_line)
                    elif raw_line.startswith('R,CFG,'):
                        parts=raw_line.split(',')
                        with condition:
                            if hid_write is not None and len(parts)==6:
                                expected=hid_write['commands'][hid_write['index']][1]
                                if parts[2:5]==expected:
                                    if parts[5]!='OK':hid_write_status={'state':'error','message':'S3 rejected the profile.'};hid_write=None
                                    else:
                                        hid_write['index']+=1;hid_write['attempts']=0;hid_write['next']=0
                                        if hid_write['index']==len(hid_write['commands']):
                                            saved_layer=hid_write['layer'];hid_write_status={'state':'saved','message':('All layers saved to S3.' if saved_layer is None else f'Layer {saved_layer+1} saved to S3.')};hid_write=None
                                        else:hid_write_status={'state':'writing','message':f"{hid_write['index']}/{len(hid_write['commands'])}"}
                    elif raw_line.startswith('L,'):
                        parts=raw_line.split(',')
                        if len(parts)==3 and parts[1] in ('0','1','2','3') and parts[2] in ('0','1'):
                            with condition:
                                layer_status={'layer':int(parts[1]),'pressed':parts[2]=='1'};layer_status_at=time.monotonic()
                                if pending_layer==layer_status['layer']:pending_layer=None
                    elif raw_line.startswith('P,'):
                        status = parse_rate_status(raw_line)
                        if status is not None:
                            with condition:
                                rate_status=status; rate_status_at=time.monotonic()
                            if log is not None: log.write({'event': 'rate_status', **status})
                    elif raw_line.startswith('D,'):
                        diagnostics = parse_bridge_diagnostics(raw_line)
                        if diagnostics is not None:
                            with condition:
                                bridge_status=diagnostics; bridge_status_at=time.monotonic()
                            if log is not None: log.write({'event': 'bridge_diagnostics', **diagnostics})
                    if time.monotonic() - last > 2:
                        raise TimeoutError('2 saniyedir geçerli sensör paketi gelmiyor')
        except (serial.SerialException, OSError, TimeoutError) as e:
            reason = str(e)
            print(f'Seri bağlantı bekleniyor: {e}', flush=True)
        finally:
            with condition:
                rate_status=None; bridge_status=None; layer_status=None; pending_layer=None; pending_events.clear()
                if hid_write is not None:hid_write_status={'state':'error','message':'Connection lost while writing.'};hid_write=None
            publish({'connected': False})
            if log:
                log.close(reason)
        stop.wait(1)

class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        global lighting_profiles,pending_layer,pending_layer_until,pending_events,hid_write,hid_write_status
        if self.path not in ('/lighting','/light-control','/hid-write') or self.headers.get('Content-Type','').split(';')[0]!='application/json':
            self.send_error(404);return
        try:
            length=int(self.headers.get('Content-Length','0'))
            if length<1 or length>4096:raise ValueError('size')
            payload=json.loads(self.rfile.read(length))
            if self.path=='/hid-write':
                if not isinstance(payload,dict) or not isinstance(payload.get('data'),str):raise ValueError('profile')
                blob=bytes.fromhex(payload['data'])
                layer=payload.get('layer')
                if layer is None and len(blob)==768:chunk_start=0;count=16
                elif type(layer) is int and 0<=layer<4 and len(blob)==192:chunk_start=layer*4;count=4
                else:raise ValueError('profile length')
                tx=secrets.randbelow(2147483646)+1
                commands=[(f'CFG,B2,{tx}',['B',str(tx),'0'])] if layer is None else [(f'CFG,L2,{tx},{layer}',['L',str(tx),str(layer)])]
                for part in range(count):
                    index=chunk_start+part;commands.append((f'CFG,D,{tx},{index},{blob[part*48:(part+1)*48].hex()}',['D',str(tx),str(index)]))
                commands.append((f'CFG,C,{tx},{binascii.crc_hqx(blob,0xffff)}',['C',str(tx),'0']))
                with condition:
                    if hid_write is not None or not state.get('connected'):self.send_error(409);return
                    hid_write={'commands':commands,'index':0,'attempts':0,'next':0,'layer':layer};hid_write_status={'state':'writing','message':f'0/{len(commands)}'}
                self.send_response(202);self.end_headers();return
            if self.path=='/light-control':
                if not isinstance(payload,dict) or payload.get('kind') not in ('layer','hit','physical_hit'):raise ValueError('kind')
                if type(payload.get('layer')) is not int or not 0<=payload['layer']<4:raise ValueError('layer')
                with condition:
                    if payload['kind']=='layer':pending_layer=payload['layer'];pending_layer_until=time.monotonic()+3.0
                    else:
                        key=payload.get('key');event_id=payload.get('id')
                        if type(key) is not int or not 0<=key<12 or type(event_id) is not int or not 0<event_id<=0xffffffff:raise ValueError('hit')
                        if len(pending_events)>=64:pending_events.pop(0)
                        pending_events.append([key,payload['layer'],event_id,2,3 if payload['kind']=='physical_hit' else 2])
                self.send_response(204);self.end_headers();return
            if not isinstance(payload,list) or len(payload)!=4:raise ValueError('profiles')
            for row in payload:
                if not isinstance(row,list) or len(row)!=7 or any(type(x) is not int or x<0 or x>255 for x in row):raise ValueError('row')
                effect,r,g,b,brightness,speed,flags=row
                if effect>9 or brightness>100 or speed<1 or speed>100 or flags & ~5 or (flags & 4 and effect!=5):raise ValueError('value')
            with condition:lighting_profiles=bytes(v for row in payload for v in row)
        except (ValueError,TypeError,json.JSONDecodeError):
            self.send_error(400);return
        self.send_response(204);self.end_headers()
    def do_GET(self):
        if self.path in ('/', '/demo'):
            page = PAGE if self.path == '/' else DEMO_PAGE
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.send_header('Content-Length', str(len(page)))
            self.end_headers()
            self.wfile.write(page)
        elif self.path == '/state':
            with condition:
                now=time.monotonic()
                payload = json.dumps({**state,
                    'rate_status': rate_status if state.get('connected') and now-rate_status_at<3 else None,
                    'bridge_status': bridge_status if state.get('connected') and now-bridge_status_at<3 else None,
                    'layer_status': layer_status if state.get('connected') and now-layer_status_at<3 else None,
                    'hid_write_status':hid_write_status}).encode('utf-8')
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Cache-Control', 'no-store')
            self.send_header('Content-Length', str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
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
    parser.add_argument('--studio-heartbeat', action='store_true', help='Report that the native Studio process is running')
    args = parser.parse_args()
    server = ThreadingHTTPServer(('127.0.0.1', args.http_port), Handler)
    worker = threading.Thread(target=read_serial, args=(args.port, args.studio_heartbeat), daemon=True)
    worker.start()
    print(f'Tarayıcı: http://127.0.0.1:{args.http_port}', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        worker.join(timeout=2)
        server.server_close()

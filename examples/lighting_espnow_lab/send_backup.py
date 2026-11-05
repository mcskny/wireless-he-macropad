#!/usr/bin/env python3
"""Translate a native Studio full backup layer into Lighting Lab S3 console commands."""
import argparse
import json
import math
import re
import sys
import time
from pathlib import Path

EFFECTS = {'Kapalı': 'off', 'Sabit': 'static', 'Nefes': 'breathe', 'Dalga': 'flow',
           'Gökkuşağı': 'rainbow', 'Kayan ışık': 'sweep', 'Tepkisel': 'reactive',
           'Basılan yerden dalga': 'ripple'}
RAINBOW = {'Nefes': 'rainbowBreath', 'Dalga': 'rainbowFlow', 'Kayan ışık': 'rainbowSweep',
           'Tepkisel': 'rainbowReactive', 'Basılan yerden dalga': 'rainbowWave'}

def studio_command(backup, layer, target=None):
    if not isinstance(backup, dict) or not 0 <= layer < 4 or (target is not None and not 0 <= target < 4):
        raise ValueError('Tam Studio yedeği ve 0–3 arası katman gerekir.')
    try:
        light = backup['config']['layers'][layer]['light']
    except (KeyError, IndexError, TypeError) as exc:
        raise ValueError('Tam Studio yedeği ve 0–3 arası katman gerekir.') from exc
    effect = light.get('effect', 'Sabit')
    legacy_depth = effect in ('Derinliğe duyarlı', 'Derinliğe duyarlı dalga')
    if effect == 'Derinliğe duyarlı': effect = 'Tepkisel'
    elif effect == 'Derinliğe duyarlı dalga': effect = 'Basılan yerden dalga'
    if effect not in EFFECTS: raise ValueError(f'Desteklenmeyen efekt: {effect}')
    color = light.get('color', '#A6F0CA')
    if not isinstance(color, str) or not re.fullmatch(r'#[0-9a-fA-F]{6}', color):
        raise ValueError('Renk #RRGGBB biçiminde olmalı.')
    def percent(key, default, lo):
        n = light.get(key, default)
        if isinstance(n, bool) or not isinstance(n, (int,float)) or not math.isfinite(n) or not lo <= n <= 100:
            raise ValueError(f'{key} geçersiz')
        return int(n + 0.5)
    def boolean(key, default=False):
        value = light.get(key)
        if value is None: return default
        if not isinstance(value, bool): raise ValueError(f'{key} true/false olmalı')
        return value
    flags = 1 if boolean(RAINBOW.get(effect, '')) else 0
    if effect in ('Tepkisel', 'Basılan yerden dalga') and boolean('depthSensitive', legacy_depth): flags |= 2
    if effect == 'Kayan ışık' and light.get('sweepColorStyle') == 'tur': flags |= 4
    r, g, b = (int(color[i:i+2], 16) for i in (1,3,5))
    output_layer = layer if target is None else target
    return f'set {output_layer} {EFFECTS[effect]} {r} {g} {b} {percent("brightness",60,0)} {percent("speed",50,1)} {flags}'

def exchange(port, command):
    port.reset_input_buffer()
    port.write((command+'\n').encode('ascii')); port.flush()
    deadline = time.monotonic() + 12
    while time.monotonic() < deadline:
        line = port.readline().decode('utf-8', errors='replace').strip()
        if line: print(line)
        if line.startswith('ERR '): raise RuntimeError(line)
        if line.startswith('RESULT '):
            if line.endswith(' OK'): return
            raise RuntimeError(line)
    raise TimeoutError('S3 yanıt vermedi; doğru portu ve konsolu kontrol et.')

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('backup', type=Path, help='Studio Dosya → Yedeği Dışa Aktar JSON dosyası')
    p.add_argument('--layer', type=int, choices=range(4), required=True, help='Yedekteki katman: 0–3')
    p.add_argument('--target', type=int, choices=range(4), help='C6 hedef katman: varsayılan kaynakla aynı')
    p.add_argument('--port', help='S3 seri konsol portu')
    p.add_argument('--activate', action='store_true', help='Profil onaylandıktan sonra hedef katmanı etkinleştir')
    p.add_argument('--dry-run', action='store_true', help='Sadece komutu göster, bağlantı açma')
    args = p.parse_args()
    backup = json.loads(args.backup.read_text(encoding='utf-8'))
    if backup.get('version') != 1: raise ValueError('Desteklenmeyen yedek sürümü.')
    command = studio_command(backup, args.layer, args.target)
    active = args.layer if args.target is None else args.target
    print(command)
    if args.dry_run:
        if args.activate: print(f'active {active}')
        return
    if not args.port: p.error('--port veya --dry-run gerekli')
    try: import serial
    except ImportError as exc: raise RuntimeError('pyserial gerekli: pip install pyserial') from exc
    dev=serial.Serial(port=None, baudrate=115200, timeout=0.25, write_timeout=2)
    dev.dtr=False;dev.rts=False;dev.port=args.port
    with dev:
        exchange(dev, command)
        if args.activate: exchange(dev, f'active {active}')

if __name__ == '__main__':
    try: main()
    except (OSError, ValueError, RuntimeError, TimeoutError, json.JSONDecodeError) as exc:
        print(f'Hata: {exc}', file=sys.stderr);sys.exit(1)

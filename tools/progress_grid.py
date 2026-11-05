#!/usr/bin/env python3
"""A separate compact grid based on the existing manually reviewed statuses."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1]
source = (root / 'tools/progress.py').read_text().split('\ndef audit():', 1)[0]
namespace = {'__file__': str(root / 'tools/progress.py')}
exec(compile(source, 'progress.py', 'exec'), namespace)
groups = namespace['groups']
items = [(group, name, state) for group, entries in groups.items() for name, state in entries]
items.sort(key=lambda item: {'tested': 0, 'partial': 1, 'missing': 2}[item[2]])
counts = {s: sum(state == s for _, _, state in items) for s in ('tested', 'partial', 'missing')}
score = round(100 * (counts['tested'] + .5 * counts['partial']) / len(items))

im = Image.new('RGB', (1600, 3650), '#0b1118')
d = ImageDraw.Draw(im)
fontpath = '/System/Library/Fonts/Supplemental/Arial.ttf'
boldpath = '/System/Library/Fonts/Supplemental/Arial Bold.ttf'
def font(n, bold=False):
    return ImageFont.truetype(boldpath if bold else fontpath, n)
def text(x, y, value, n, color='#eef5f3', bold=False):
    d.text((x, y), value, font=font(n, bold), fill=color)
white, mute = '#eff8f4', '#a9bab9'
green, gold, gray = '#58df98', '#ffc768', '#8999a6'
text(72, 52, 'WIRELESS HE', 30, mute, True)
text(72, 97, 'Proje durum haritası', 53, white, True)
text(74, 167, 'Her kare bir değerlendirme · 06 Kasım 2026', 23, mute)
text(1190, 79, f'~%{score}', 82, green, True)
text(1195, 175, 'GENEL DURUM', 18, mute, True)
d.rounded_rectangle((55, 235, 1545, 895), radius=25, fill='#15212a', outline='#354a50', width=2)
cols = 20
size, spacing = 43, 8
width = cols * size + (cols - 1) * spacing
x0 = (1600 - width)//2
y0 = 270
for i, (_, _, state) in enumerate(items):
    x, y = x0 + (i % cols) * (size + spacing), y0 + (i // cols) * (size + spacing)
    d.rounded_rectangle((x, y, x + size, y + size), radius=5,
                        fill={'tested': green, 'partial': gold, 'missing': gray}[state])
for x, color, label, value in [(86, green, 'Tamamlandı / denendi', counts['tested']), (535, gold, 'Kısmi / test eksik', counts['partial']), (1120, gray, 'Eksik', counts['missing'])]:
    d.rounded_rectangle((x, 937, x + 21, 958), radius=4, fill=color)
    text(x + 34, 933, f'{label}  {value}', 22, white, True)
text(75, 1005, 'Yaklaşık oran: yeşil 1, sarı 0,5, gri 0 puan. Kareler eşit ağırlıklı değerlendirilir.', 20, mute)
text(75, 1036, 'Bu oran gerçek iş yükünü veya ürünün kesin tamamlanma düzeyini ölçmez.', 20, mute)
text(75, 1107, 'KARELERİN AÇIKLAMALARI', 28, white, True)
text(75, 1150, 'Numaralar soldan sağa, yukarıdan aşağıya sıralanır.', 19, mute)
# Three vertical columns keep the full list readable without truncating labels.
rows_per_column = (len(items) + 2) // 3
column_width = 500
for index, (group, name, state) in enumerate(items):
    col, row = divmod(index, rows_per_column)
    x, y = 75 + col * column_width, 1195 + row * 31
    color = {'tested': green, 'partial': gold, 'missing': gray}[state]
    d.rounded_rectangle((x, y+4, x+12, y+16), radius=2, fill=color)
    text(x+20, y, f'{index+1:03}  {name}', 17, white)
out = root / 'Docs/progress/proje-durumu-kareler.png'
out.parent.mkdir(parents=True, exist_ok=True)
im.save(out, optimize=True)
print(f'{out}: {len(items)} kare, yaklaşık %{score}')

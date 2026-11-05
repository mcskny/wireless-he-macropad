#!/usr/bin/env python3
"""Generate English progress charts from the reviewed Turkish status data."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
root=Path(__file__).resolve().parents[1]
source=(root/'tools/progress.py').read_text().split('\ndef audit():',1)[0]
ns={'__file__':str(root/'tools/progress.py')};exec(compile(source,'progress.py','exec'),ns)
translations=[
('HARDWARE & SENSORS','12 Hall sensors|ADC + multiplexer|Channel mapping|Per-key calibration|All-key stability|Simultaneous key test|PCB electrical checks|Battery / charge measurement|12-sensor mapping|Released measurement|Fully pressed measurement|ADC range comparison|Zero / saturation|Neighbor sensor effects|USB-C mechanical fit|Physical encoder input'),
('USB TEST PLATFORM','ESP32 serial stream|12-channel live read|Raw ADC chart|Session logging|Packet loss analysis|Channel diagnostics|Long-duration test|Physical disconnect test|Serial sequence numbers|Legacy frame support|Invalid frame log|Session end log|Automatic port selection|Disconnect / reconnect|Channel min / max|Log viewer'),
('MAC APPLICATION','Native interface|2D view|3D model and motion|3D component inspection|Profile editor|Layer editor|Lighting studio|Hall settings screen|Theme / Liquid Glass|Menu bar|Window layout|Layer preview|Simulation interface|3D camera limits|Application icon|Language selection'),
('MAC INPUT','Keyboard output|Two-stage key|Media and volume|macOS shortcuts|App-specific actions|Native analog gamepad|Automatic profile switching|Physical encoder input|Letter key assignment|Modifier shortcuts|Accessibility permission|Key-release protection|Spotify shortcuts|Discord shortcuts|Rapid Trigger output|Write actions to device'),
('GAMES & ANALOG','Assetto steering|Assetto throttle / brake|CrossOver bridge|ETS2 in-game test|Flight Simulator test|Farming Simulator test|Analog response curves|All-combinations test|Steer left|Steer right|Throttle axis|Brake axis|Four-key calibration|Game deadzone|Native Mac gamepad|In-game latency measurement'),
('LIGHTING','2D / 3D preview|Static / breathing|Wave / rainbow|Sweep / snake|Starlight / reactive|Live color / effect|C6 live LED engine|12 physical LEDs lit|Off|Static|Breathing|Wave|Rainbow|Sweep|Starlight|Snake'),
('C6 TRANSMITTER','C6 1 kHz scan test|C6 wireless transmit|Encoder / power code|Battery reading|C6 sensor hardware test|D7 layer switch|C6 LED engine|Wireless security|MCP3202 selection|12-channel scan|5 / 23-byte frame|Power mode code|BLE LED settings saved|Physical layer LEDs|Pairing verification|Packet encryption'),
('S3 RECEIVER & CONFIG','S3 wireless sensor RX|USB keyboard 6KRO|USB media / gamepad|Hall processing|NKRO|Mac to C6 live lighting|ACK / readback|C6 to S3 to Mac stream|Single-key HID test|Consumer HID report|Gamepad HID report|Connection watchdog|USB lighting settings|Versioned NVS storage|Device identity / capabilities|Settings readback test'),
('CALIBRATION & MEASUREMENT','Released ADC capture|Fully pressed ADC capture|12-key calibration|Percentage calculation|Reverse polarity sensor|ADC min / max|ADC endpoint warning|Approximate 3D depth|Calibration persistence|12-key wizard|Physical travel in mm|Magnetic field in mT|Saturation detection|Noise profile|Nonlinear response curve|Cross-channel effects'),
('LAYERS, HALL & ACTIONS','Four layer drafts|Layer preview|Single key assignment|Second actuation stage|Actuation threshold|Hysteresis|Rapid Trigger settings|Layer SOCD settings|Custom profile storage|Encoder assignment|Hold layer + encoder|Physical layer LED|Two-way layer state|Physical Rapid Trigger test|Physical SOCD test|Encoder HID integration'),
('COMMUNICATION & CONFIG','12 ADC serial frame|Serial server status|Channel remapping|Sample sequence number|Sequence gap logging|Version / CRC frame|ESP-NOW sensor TX|ESP-NOW sensor RX|Four-layer live lighting|Device identification|Mac to S3 lighting|S3 to C6 lighting|Config transaction confirmation|Device readback|BLE persistent profile write|Secure pairing'),
('TEST & RELIABILITY','Assetto axis test|Assetto gameplay test|Four-key calibration|Live serial stream|HID unit tests|Encoder unit tests|Protocol unit tests|Profile mapping|12-key sensor stream|Long-duration stream|Noise / drift|Power consumption|Wireless loss measurement|HID timeout unit test|C6 sensor test board|S3 USB serial on Mac'),
('POWER & DUTY CYCLE','MOSFET gate control|100 Hz idle scan|10 Hz idle transmission|Wake on movement|1 kHz active target|Idle after 2 s|Startup delay|Threshold / hysteresis|Power policy unit tests|Sensor supply measurement|MOSFET circuit test|Backfeed measurement|Wake latency|Noise / wake threshold|Total current measurement|Battery life measurement'),
('BLUETOOTH LE','BLE HID / Mac|Layer + KEY 0 switch|ESP-NOW / BLE switching|Mac BLE pairing|Encrypted BLE link|Bond persistence|BLE key assignment / H|BLE RT / SOCD test|BLE configuration transfer|BLE profile persistence|Separate transport layers|Three-host switching|BLE live lighting|Separate lighting persistence|BLE sensor stream|Latency / battery measurement'),
]
translations = [(name, labels.replace('Keyboard output|', 'Letter HID output|').replace('Media / audio|', 'Volume HID output|').replace('Write actions to device', 'Write S3 actions').replace('Consumer HID report', 'Volume HID report').replace('Versioned NVS storage', 'Persistent HID settings').replace('Settings readback test', 'Settings restart test').replace('Single key assignment', 'Layered key assignment').replace('Device readback', 'S3 profile readback').replace('Startup delay', '600 µs startup delay').replace('Noise / wake threshold', 'Commanded gate on-time').replace('Profile mapping', 'H / J layer test')) for name, labels in translations]
groups=[]
for (turkish,items),(name,labels) in zip(ns['groups'].items(),translations):
    words=labels.split('|'); assert len(words)==len(items),(turkish,len(words),len(items))
    groups.append((name,[(label,old[1]) for label,old in zip(words,items)]))
assert len(groups)==len(ns['groups'])
states=[s for _,a in groups for _,s in a];score=round(100*(states.count('tested')+.5*states.count('partial'))/len(states))
colors={'tested':'#58df98','partial':'#ffc768','missing':'#8999a6'}; fills={'tested':'#174b34','partial':'#5a401d','missing':'#364654'}
regular='/System/Library/Fonts/Supplemental/Arial.ttf';bold='/System/Library/Fonts/Supplemental/Arial Bold.ttf'
def f(n,b=False):return ImageFont.truetype(bold if b else regular,n)
def title(d,xy,t,n=20,c='#ffffff',b=False): d.text(xy,t,font=f(n,b),fill=c)
def wrap(d,text,w,n=17):
    line='';lines=[]
    for word in text.split():
        t=(line+' '+word).strip()
        if line and d.textlength(t,font=f(n))>w: lines.append(line);line=word
        else:line=t
    if line:lines.append(line)
    return lines
out=root/'Docs/progress';out.mkdir(parents=True,exist_ok=True)
im=Image.new('RGB',(2600,2085),'#080d12');d=ImageDraw.Draw(im)
title(d,(38,25),'WIRELESS HE  /  PROJECT PROGRESS MAP',39,b=True)
title(d,(40,80),'Hardware · sensor platform · macOS · firmware · power · 06 Nov 2026',20,'#becbd0')
title(d,(1960,126),f'OVERALL  ~{score}%',25,colors['tested'],True)
title(d,(38,128),f'PROJECT FEATURES  /  {len(states)} STATUS TILES',24,b=True)
title(d,(40,164),'Estimate: tested = 1, partial = 0.5, missing = 0; tiles are equally weighted and can overlap.',18,'#becbd0')
for idx,(name,items) in enumerate(groups):
 x=38+(idx%4)*634;y=199+(idx//4)*350
 d.rounded_rectangle((x,y,x+619,y+335),6,fill='#17222c',outline='#455966')
 title(d,(x+15,y+12),name,20,b=True)
 for i,(label,state) in enumerate(items):
  xx=x+13+(i%4)*149;yy=y+48+(i//4)*69
  d.rectangle((xx,yy,xx+142,yy+63),fill=fills[state],outline='#5a6b74');d.rectangle((xx,yy,xx+7,yy+63),fill=colors[state])
  lines=wrap(d,label,120,16)
  for j,line in enumerate(lines):title(d,(xx+12,yy+8+j*19),line,16)
x,y=1306,1249
d.rounded_rectangle((x,y,x+619,y+335),6,fill='#17222c',outline='#455966')
title(d,(x+15,y+12),'STATUS LEGEND',20,b=True)
for i,(key,label) in enumerate([('tested','Complete / tested'),('partial','Implemented or planned; testing incomplete'),('missing','Missing / not verified')]):
 yy=y+60+i*60;d.rectangle((x+20,yy,x+41,yy+21),fill=colors[key]);title(d,(x+58,yy),f'{label}: {states.count(key)}',18)
title(d,(x+20,y+277),'The score is a rough checklist index, not a product-completion metric.',15,'#becbd0')
x,y=38,1599;d.rounded_rectangle((x,y,x+2521,y+335),6,fill='#17222c',outline='#455966');title(d,(x+20,y+15),'CURRENT PRIORITIES',23,b=True)
for i,t in enumerate(['BLE stability, RT / SOCD and sensor stream','Wireless throughput, reliability and secure pairing','MOSFET rail, backfeed and current measurements','Physical encoder, gamepad and battery tests']):title(d,(x+25,y+62+i*47),f'{i+1:02}   {t}',20)
title(d,(x+20,y+270),'Separate BLE / ESP-NOW key assignments tested; power savings unmeasured.',16,'#becbd0')
title(d,(38,1990),'Status reflects evidence from the test platform and code review. It does not guarantee product readiness.',18,'#becbd0')
im.save(out/'project-progress-en.png',optimize=True)
# Compact sorted grid and numbered explanatory list.
entries=[(g,n,s) for g,items in groups for n,s in items];entries.sort(key=lambda x:{'tested':0,'partial':1,'missing':2}[x[2]])
im=Image.new('RGB',(1600,3650),'#0b1118');d=ImageDraw.Draw(im)
title(d,(72,50),'WIRELESS HE',30,'#a9bab9',True);title(d,(72,95),'Project progress overview',53,b=True)
title(d,(74,167),'One square per review item · 06 Nov 2026',23,'#a9bab9');title(d,(1190,78),f'~{score}%',82,colors['tested'],True)
d.rounded_rectangle((55,235,1545,895),25,fill='#15212a',outline='#354a50',width=2)
for i,(_,_,state) in enumerate(entries):
 x=294+(i%20)*51;y=270+(i//20)*51;d.rounded_rectangle((x,y,x+43,y+43),5,fill=colors[state])
for x,state,label in [(85,'tested','Complete / tested'),(510,'partial','Partial / untested'),(1100,'missing','Missing')]:
 d.rounded_rectangle((x,937,x+21,958),4,fill=colors[state]);title(d,(x+34,933),f'{label}  {states.count(state)}',22,b=True)
title(d,(75,1005),'Estimate: tested = 1, partial = 0.5, missing = 0. Items have equal weight.',20,'#a9bab9')
title(d,(75,1036),'This is not a measurement of actual workload or product completeness.',20,'#a9bab9')
title(d,(75,1107),'SQUARE INDEX',28,b=True);title(d,(75,1150),'Read squares from left to right, top to bottom.',19,'#a9bab9')
rows=(len(entries)+2)//3
for i,(_,name,state) in enumerate(entries):
 col,row=divmod(i,rows);x=75+col*500;y=1195+row*31
 d.rounded_rectangle((x,y+4,x+12,y+16),2,fill=colors[state]);title(d,(x+20,y),f'{i+1:03}  {name}',17)
im.save(out/'project-progress-grid-en.png',optimize=True)
print(f'English charts: {len(states)} tiles, estimate ~{score}%')

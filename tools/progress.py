#!/usr/bin/env python3
"""Wireless HE proje durumunu görselleştirir; profil eşleştirmelerini Swift kodundan sayar."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import re, json, subprocess, tempfile
ROOT = Path(__file__).resolve().parents[1]
SWIFT = ROOT / 'examples/macropad_12_idf/mac_app/Native.swift'
OUT = ROOT / 'Docs/progress/proje-durumu.png'
# Elle gözden geçirilmiş durumlar, 3 Kasım 2026. Bunlar kod kapsamı değil,
# konuşmadaki test kanıtları ve repo incelemesine dayanan anlık değerlendirmelerdir.
groups = {
'DONANIM & SENSÖR': [('12 Hall sensörü','partial'),('ADC + çoklayıcı','tested'),('Kanal eşleştirme','tested'),('Tuş başına kalibrasyon','tested'),('Tüm tuşların kararlılığı','missing'),('Eşzamanlı tuş testi','missing'),('PCB elektrik kontrolü','partial'),('Pil ve şarj ölçümü','missing')],
'USB TEST PLATFORMU': [('ESP32 seri veri','tested'),('12 kanal canlı okuma','tested'),('Ham ADC grafiği','tested'),('Veri oturum günlüğü','partial'),('Paket kaybı analizi','partial'),('Kanal tanılama','tested'),('Uzun süreli test','missing'),('Fiziksel bağlantı kesilmesi','missing')],
'MAC UYGULAMASI': [('Native arayüz','tested'),('2B görünüm','tested'),('3B model ve hareket','tested'),('3B parça inceleme','tested'),('Profil ve katman kaydı','partial'),('Yedekle / geri yükle','partial'),('Menü çubuğu','partial'),('Performans doğrulaması','missing')],
'MAC GİRDİLERİ': [('Klavye çıktısı','partial'),('İki kademeli tuş','partial'),('Medya ve ses','partial'),('Mac sistem kısayolları','partial'),('Uygulamaya özel görevler','partial'),('Yerel analog gamepad','missing'),('Otomatik profil seçimi','missing'),('Gerçek encoder girdisi','missing')],
'OYUN & ANALOG': [('Assetto direksiyon','tested'),('Assetto gaz ve fren','tested'),('CrossOver köprüsü','tested'),('ETS2 oyun testi','missing'),('Flight Simulator testi','missing'),('Farming Simulator testi','missing'),('Analog tepki eğrileri','missing'),('Tüm kombinasyonların testi','missing')],
'AYDINLATMA': [('2B / 3B önizleme','tested'),('Sabit ve nefes','tested'),('Dalga ve gökkuşağı','tested'),('Kayan ışık / yılan','tested'),('Starlight ve tepkisel','tested'),('Özel renkler','partial'),('LED motoru laboratuvarı','partial'),('Gerçek LED entegrasyonu','missing')],
'C6 GÖNDERİCİ': [('Sensör tarama kodu','partial'),('ESP-NOW gönderici','partial'),('Encoder / güç kodu','partial'),('Pil okuma','partial'),('Gerçek C6 donanım testi','missing'),('Yeni layer/encoder entegrasyonu','missing'),('LED motoru ana entegrasyonu','missing'),('Kablosuz güvenlik','missing')],
'S3 ALICI & YAPILANDIRMA': [('ESP-NOW alıcı','partial'),('USB klavye 6KRO','partial'),('USB medya / gamepad','partial'),('Hall işleme','partial'),('NKRO','missing'),('Mac → cihaz ayarı','missing'),('ACK / geri okuma','missing'),('C6 + S3 fiziksel test','missing')],
}

extra = {'DONANIM & SENSÖR': [('12 sensör eşleşmesi', 'partial'), ('Serbest değer ölçümü', 'tested'), ('Tam basış ölçümü', 'tested'), ('ADC aralık karşılaştırma', 'partial'), ('0 değeri / doyma', 'partial'), ('Komşu sensör etkisi', 'missing'), ('USB-C mekanik bağlantı', 'partial'), ('Encoder fiziksel okuma', 'missing')], 'USB TEST PLATFORMU': [('Seri sıra numarası', 'partial'), ('Eski paket desteği', 'partial'), ('Hatalı paket kaydı', 'partial'), ('Oturum bitiş kaydı', 'partial'), ('Otomatik port seçimi', 'partial'), ('Kopma / yeniden bağlanma', 'partial'), ('Min / maks kanallar', 'partial'), ('Günlük arayüzü', 'partial')], 'MAC UYGULAMASI': [('Tema / Liquid Glass', 'partial'), ('Menü / kısayollar', 'partial'), ('Pencere boyutu', 'tested'), ('Katman önizleme', 'tested'), ('Simülasyon modu', 'partial'), ('3B kamera sınırı', 'tested'), ('Uygulama simgesi', 'tested'), ('VoiceOver testi', 'missing')], 'MAC GİRDİLERİ': [('Harf tuşu atama', 'partial'), ('Modifier kısayolları', 'partial'), ('Klavye erişim izni', 'partial'), ('Tuş bırakma koruması', 'partial'), ('Spotify kısayolları', 'partial'), ('Discord kısayolları', 'partial'), ('Rapid Trigger çıkışı', 'partial'), ('Cihaza görev yazma', 'missing')], 'OYUN & ANALOG': [('Direksiyon sol', 'tested'), ('Direksiyon sağ', 'tested'), ('Gaz ekseni', 'tested'), ('Fren ekseni', 'tested'), ('4 tuş kalibrasyonu', 'tested'), ('Oyun ölü bölgesi', 'partial'), ('Mac native gamepad', 'missing'), ('Oyun içi gecikme ölçümü', 'missing')], 'AYDINLATMA': [('Kapalı', 'tested'), ('Sabit', 'tested'), ('Nefes', 'tested'), ('Dalga', 'tested'), ('Gökkuşağı', 'tested'), ('Kayan ışık', 'tested'), ('Starlight', 'tested'), ('Yılan', 'tested')], 'C6 GÖNDERİCİ': [('MCP3202 seçimi', 'partial'), ('12 kanal tarama', 'partial'), ('5 / 23 bayt paket', 'partial'), ('Güç modu kodu', 'partial'), ('LED ayarı kaydı', 'missing'), ('Layer LED gerçek durumu', 'missing'), ('Eşleşme doğrulama', 'missing'), ('Paket şifreleme', 'missing')], 'S3 ALICI & YAPILANDIRMA': [('Klavye HID raporu', 'partial'), ('Consumer HID raporu', 'partial'), ('Gamepad HID raporu', 'partial'), ('Bağlantı watchdog', 'partial'), ('USB yapılandırma akışı', 'missing'), ('Kalıcı NVS sürümü', 'missing'), ('Cihaz kimliği / yetenek', 'missing'), ('Ayar geri okuma testi', 'missing')]}
for group, items in extra.items(): groups[group].extend(items)
groups.update({'KALİBRASYON & ÖLÇÜM': [('Serbest ADC kaydı', 'tested'), ('Tam basış ADC kaydı', 'tested'), ('12 ayrı kalibrasyon', 'partial'), ('Yüzde hesabı', 'tested'), ('Ters yönlü sensör', 'partial'), ('ADC min / maks', 'partial'), ('ADC uç değer uyarısı', 'partial'), ('Yaklaşık 3B derinlik', 'partial'), ('Kalibrasyon kalıcılığı', 'partial'), ('12 tuş sihirbazı', 'partial'), ('Fiziksel mm ölçümü', 'missing'), ('Manyetik alan mT', 'missing'), ('Doyma tespiti', 'missing'), ('Gürültü profili', 'partial'), ('Doğrusal olmayan eğri', 'partial'), ('Çapraz etki ölçümü', 'missing')], 'LAYER, HALL & GÖREVLER': [('4 katman taslağı', 'tested'), ('Katman önizleme', 'tested'), ('Tek tuş atama', 'partial'), ('İkinci kademe', 'partial'), ('Basma eşiği', 'partial'), ('Histerezis ayarı', 'partial'), ('Rapid Trigger ayarı', 'partial'), ('Katman SOCD ayarı', 'partial'), ('Özel profil kaydı', 'partial'), ('Encoder görev atama', 'partial'), ('Basılı layer + encoder', 'partial'), ('Gerçek layer LED', 'missing'), ('Canlı layer durumu', 'missing'), ('Gerçek RT testi', 'missing'), ('Gerçek SOCD testi', 'missing'), ('Encoder HID bağlantısı', 'missing')], 'İLETİŞİM & YAPILANDIRMA': [('12 ADC seri paketi', 'tested'), ('Seri sunucu durumu', 'tested'), ('Kanal yeniden eşleme', 'tested'), ('Örnek sıra numarası', 'partial'), ('Paket boşluğu kaydı', 'partial'), ('Sürüm / CRC çerçeve', 'partial'), ('ESP-NOW gönderim', 'partial'), ('ESP-NOW alım', 'partial'), ('Dört katman ayarı', 'partial'), ('Cihaz tanımlama', 'missing'), ('Mac → S3 ayar', 'missing'), ('S3 → C6 ayar', 'missing'), ('ACK / tekrar gönder', 'missing'), ('Cihazdan geri okuma', 'missing'), ('Atomik kalıcı yazma', 'missing'), ('Güvenli eşleştirme', 'missing')], 'TEST & GÜVENİLİRLİK': [('Assetto eksen testi', 'tested'), ('Assetto oyun testi', 'tested'), ('4 tuş kalibrasyonu', 'tested'), ('Seri canlı akış', 'tested'), ('HID birim testleri', 'partial'), ('Encoder birim testi', 'partial'), ('Protokol birim testi', 'partial'), ('Profil eşleştirme', 'partial'), ('12 tuş uçtan uca', 'missing'), ('Uzun süreli akış', 'missing'), ('Gürültü / kayma', 'missing'), ('Gerçek güç tüketimi', 'missing'), ('Kablosuz gecikme', 'missing'), ('Kopma sonrası nötr', 'missing'), ('C6 / PCB fiziksel', 'missing'), ('S3 USB gerçek host', 'missing')]})


groups['GÜÇ & DUTY CYCLE'] = [
    ('MOSFET kontrol kodu','partial'), ('100 Hz boşta tarama','partial'),
    ('10 Hz boşta gönderim','partial'), ('Hareketle uyanma','partial'),
    ('1 kHz aktif hedefi','partial'), ('2 sn sonra boşta','partial'),
    ('Başlangıç beklemesi','partial'), ('Eşik ve histerezis','partial'),
    ('Güç politikası testi','tested'), ('Sensör besleme ölçümü','missing'),
    ('MOSFET devre testi','missing'), ('Ters besleme ölçümü','missing'),
    ('Uyanma gecikmesi','missing'), ('Gürültü ile eşik','missing'),
    ('Toplam akım ölçümü','missing'), ('Pil ömrü ölçümü','missing'),
]

groups['BLUETOOTH LE'] = [
    ('BLE HID lab derleme','partial'), ('Layer + KEY 1 geçiş','partial'),
    ('ESP-NOW ↔ BLE geçiş','missing'), ('BLE eşleşme','missing'),
    ('Güvenli bağlantı','missing'), ('Bağlı cihaz kaydı','missing'),
    ('HID rapor kodlama','partial'), ('Katman / RT / SOCD','missing'),
    ('Profil aktarım kontrolü','partial'), ('Profil kalıcı kayıt','missing'),
    ('Kopunca tuş bırakma','missing'), ('Yeniden bağlanma','missing'),
    ('Mac fiziksel test','missing'), ('Diğer cihaz testleri','missing'),
    ('Gecikme ölçümü','missing'), ('BLE pil ölçümü','missing'),
]

# Son donanım denemeleri: sadece kanıtlanan davranışlar yeşil.
updates = {
    'DONANIM & SENSÖR': {'12 Hall sensörü': ('12 Hall sensörü', 'tested')},
    'AYDINLATMA': {'Gerçek LED entegrasyonu': ('LED zinciri: ilk LED', 'partial')},
    'C6 GÖNDERİCİ': {'Sensör tarama kodu': ('C6 1 kHz tarama hedefi', 'partial'), 'ESP-NOW gönderici': ('C6 kablosuz gönderim', 'tested'), 'Gerçek C6 donanım testi': ('C6 sensör donanım testi', 'tested'), '12 kanal tarama': ('12 kanal tarama', 'tested')},
    'S3 ALICI & YAPILANDIRMA': {'ESP-NOW alıcı': ('S3 kablosuz sensör alımı', 'tested'), 'C6 + S3 fiziksel test': ('C6 → S3 → Mac akışı', 'tested'), 'Klavye HID raporu': ('Tek tuş HID testi', 'tested'), 'USB yapılandırma akışı': ('USB test ayar yolu', 'partial')},
    'İLETİŞİM & YAPILANDIRMA': {'ESP-NOW gönderim': ('ESP-NOW sensör gönderim', 'tested'), 'ESP-NOW alım': ('ESP-NOW sensör alım', 'tested'), 'ACK / tekrar gönder': ('Ayar işlem onayı', 'missing')},
    'TEST & GÜVENİLİRLİK': {'C6 / PCB fiziksel': ('C6 sensör test kartı', 'tested'), 'S3 USB gerçek host': ('S3 USB seri gerçek Mac', 'tested'), 'Kablosuz gecikme': ('Kablosuz kayıp ölçümü', 'tested'), '12 tuş uçtan uca': ('12 tuş sensör akışı', 'tested'), 'HID birim testleri': ('HID birim testleri', 'tested'), 'Kopma sonrası nötr': ('HID zaman aşımı testi', 'partial')},
}
for group, changes in updates.items():
    groups[group] = [changes.get(label, (label, state)) for label, state in groups[group]]


# The owner considers the app UI complete. Green here denotes implemented UI,
# not a claim that device integration, performance, or accessibility is verified.
ui_items = ['Native arayüz','2B görünüm','3B model ve hareket','3B parça inceleme',
            'Profil düzenleyici','Katman düzenleyici','Aydınlatma stüdyosu','Hall ayar ekranı',
            'Tema / Liquid Glass','Menü çubuğu','Pencere düzeni','Katman önizleme',
            'Simülasyon arayüzü','3B kamera sınırı','Uygulama simgesi','Dil seçimi']
groups['MAC UYGULAMASI'] = [(label, 'tested') for label in ui_items]

# 4 Kasım: fiziksel denemeler ve canlı ışık eşleştirmesi; kalıcı kayıt ile
# güç tüketimi henüz ölçülmediği için tamamlanmış sayılmıyor.
recent = {
 'AYDINLATMA': {'LED zinciri: ilk LED':('12 LED fiziksel yanma','tested'), 'LED motoru laboratuvarı':('C6 canlı ışık motoru','tested'), 'Özel renkler':('Canlı renk / efekt','tested')},
 'C6 GÖNDERİCİ': {'C6 1 kHz tarama hedefi':('C6 1 kHz tarama testi','tested'), 'Yeni layer/encoder entegrasyonu':('D7 layer tuşu','tested'), 'LED motoru ana entegrasyonu':('C6 LED motoru','tested')},
 'S3 ALICI & YAPILANDIRMA': {'Mac → cihaz ayarı':('Mac → C6 canlı ışık','tested'), 'USB test ayar yolu':('USB ışık ayar yolu','tested')},
 'LAYER, HALL & GÖREVLER': {'Canlı layer durumu':('Çift yönlü layer','tested')},
 'İLETİŞİM & YAPILANDIRMA': {'Mac → S3 ayar':('Mac → S3 ışık','tested'), 'S3 → C6 ayar':('S3 → C6 ışık','tested'), 'Dört katman ayarı':('4 katman canlı ışık','tested')},
 'GÜÇ & DUTY CYCLE': {'MOSFET kontrol kodu':('MOSFET kontrol kodu','tested'), '100 Hz boşta tarama':('100 Hz boşta tarama','tested'), '10 Hz boşta gönderim':('10 Hz boşta gönderim','tested'), 'Hareketle uyanma':('Hareketle uyanma','tested'), '1 kHz aktif hedefi':('1 kHz aktif tarama','tested'), '2 sn sonra boşta':('Boşta moda dönüş','tested')},
}
for group, changes in recent.items():
    groups[group] = [changes.get(label, (label, state)) for label, state in groups[group]]

# 4 Kasım: gerçek HID ve duty-cycle denemeleri. Besleme ve akım ölçülmedi.
verified = {
 'KALİBRASYON & ÖLÇÜM': {'12 ayrı kalibrasyon': ('12 ayrı kalibrasyon', 'tested'), 'Kalibrasyon kalıcılığı': ('Mac kalibrasyon kaydı', 'tested'), 'ADC min / maks': ('ADC min / maks', 'tested'), 'ADC uç değer uyarısı': ('ADC uç değer uyarısı', 'tested'), '12 tuş sihirbazı': ('12 tuş sihirbazı', 'tested'), 'Yaklaşık 3B derinlik': ('Yaklaşık 3B derinlik', 'tested'), 'Gürültü profili': ('Boşta gürültü ölçümü', 'tested'), 'Ters yönlü sensör': ('Azalan ADC yönü', 'tested'), 'Doğrusal olmayan eğri': ('Doğrusal olmayan eğri', 'tested')},
 'MAC GİRDİLERİ': {'Klavye çıktısı': ('Harf HID çıkışı', 'tested'), 'Medya ve ses': ('Ses HID çıkışı', 'tested'), 'Harf tuşu atama': ('Harf tuşu atama', 'tested'), 'Cihaza görev yazma': ('S3 görev yazma', 'tested')},
 'S3 ALICI & YAPILANDIRMA': {'USB klavye 6KRO': ('USB klavye HID', 'partial'), 'Consumer HID raporu': ('Ses HID raporu', 'tested'), 'Kalıcı NVS sürümü': ('HID kalıcı kaydı', 'tested'), 'Ayar geri okuma testi': ('Kayıt yeniden başlatma', 'tested')},
 'LAYER, HALL & GÖREVLER': {'Tek tuş atama': ('Katmanlı tuş atama', 'tested')},
 'İLETİŞİM & YAPILANDIRMA': {'Cihazdan geri okuma': ('S3 profil geri okuma', 'tested')},
 'GÜÇ & DUTY CYCLE': {'Başlangıç beklemesi': ('600 µs bekleme kodu', 'partial'), 'Gürültü ile eşik': ('Komutlanan açık süre', 'tested')},
 'TEST & GÜVENİLİRLİK': {'Profil eşleştirme': ('H / J katman testi', 'tested')},
}
for group, changes in verified.items():
    groups[group] = [changes.get(label, (label, state)) for label, state in groups[group]]

# Fiziksel olarak sınanan belirli tuş davranışları; kapsamlı dayanıklılık testi değil.
latest = {
 'MAC GİRDİLERİ': {'Rapid Trigger çıkışı': ('Rapid Trigger HID', 'tested')},
 'S3 ALICI & YAPILANDIRMA': {'Hall işleme': ('Hall HID işleme', 'tested')},
 'LAYER, HALL & GÖREVLER': {'Rapid Trigger ayarı': ('Rapid Trigger ayarı', 'tested'), 'Katman SOCD ayarı': ('Katman SOCD ayarı', 'tested'), 'Gerçek RT testi': ('Gerçek RT testi', 'tested'), 'Gerçek SOCD testi': ('Gerçek SOCD testi', 'tested')},
}
for group, changes in latest.items():
    groups[group] = [changes.get(label, (label, state)) for label, state in groups[group]]

# 5 Nov: only user-confirmed BLE behaviour is marked tested.
groups['BLUETOOTH LE'] = [
 ('BLE HID derleme','tested'), ('Layer + KEY 0 geçiş','tested'),
 ('ESP-NOW / BLE geçiş','tested'), ('Mac BLE eşleşme','tested'),
 ('Şifreli BLE bağlantı','tested'), ('Kalıcı bağ kaydı','partial'),
 ('BLE harf tuşu','tested'), ('BLE katman / RT / SOCD','partial'),
 ('Profil aktarım kontrolü','partial'), ('C6 profil kaydı','partial'),
 ('Kopunca tuş bırakma','missing'), ('Otomatik yeniden bağlanma','partial'),
 ('Mac fiziksel test','tested'), ('Diğer cihaz testleri','missing'),
 ('BLE gecikme ölçümü','missing'), ('BLE pil ölçümü','missing'),
]

def audit():
    s=SWIFT.read_text(); a=s.index('final class MacKeyOutput {'); b=s.index('\nfinal class StudioModel:',a)
    h='import AppKit\nimport ApplicationServices\nstruct KeyboardAssignment { var keyCode:UInt8; var modifiers:UInt8; var label:String }\n'
    lines=[]
    for m in re.finditer(r'\.init\(id:"([^"]+)"[^\n]*?rows:(\[[^\]]+\]),encoder:',s):
        for value in json.loads(m.group(2)):
            name=json.dumps(m.group(1),ensure_ascii=False); label=json.dumps(value,ensure_ascii=False)
            lines.append(f'do {{ let name={name}; let value={label}; let custom=MacKeyOutput.customPresetShortcut(value,profile:name) != nil; let supported=MacKeyOutput.resolve(nil,value) != nil || MacKeyOutput.media[value] != nil || MacKeyOutput.presetKey(value,profile:name) != nil; print(name + "|" + value + "|" + (custom ? "setup" : supported ? "ready" : "missing")) }}')
    with tempfile.TemporaryDirectory() as tmp:
        path=Path(tmp)/'audit.swift';path.write_text(h+s[a:b]+'\n'+'\n'.join(lines))
        result=subprocess.run(['swift',str(path)],capture_output=True,text=True,check=True)
    counts={}
    for line in result.stdout.splitlines():
        name, label, state=line.split('|',2);counts.setdefault(name,[]).append((label,state))
    return counts

profiles=audit()
W,H=2600,2085
im=Image.new("RGB",(W,H),"#080d12"); d=ImageDraw.Draw(im)
font="/System/Library/Fonts/Supplemental/Arial.ttf"; bold="/System/Library/Fonts/Supplemental/Arial Bold.ttf"
def f(size,heavy=False): return ImageFont.truetype(bold if heavy else font,size)
white="#ffffff"; muted="#c0ccd1"; green="#55e195"; yellow="#ffd36b"; grey="#9aa8b5"; panel="#17222c"; border="#455966"
def t(x,y,text,size=18,fill=white,heavy=False): d.text((x,y),text,font=f(size,heavy),fill=fill,stroke_width=0)
def box(x,y,w,h): d.rounded_rectangle((x,y,x+w,y+h),radius=6,fill=panel,outline=border,width=1)
def linesfit(text,width,size=19):
    words=text.split(); lines=[]; line=""
    for word in words:
        test=(line+" "+word).strip()
        if d.textlength(test,font=f(size))>width and line: lines.append(line); line=word
        else: line=test
    if line: lines.append(line)
    if len(lines)>2: return linesfit(text,width,size-1) if size>13 else [lines[0],lines[1][:13]+"…"]
    return lines,size

def tile(x,y,w,h,label,color,bg):
    d.rectangle((x,y,x+w,y+h),fill=bg,outline="#5a6b74",width=1)
    d.rectangle((x,y,x+7,y+h),fill=color)
    result=linesfit(label,w-20,19)
    if isinstance(result,tuple): parts,size=result
    else: parts,size=result,13
    off=(h-len(parts)*(size+3))//2
    for i,line in enumerate(parts): t(x+12,y+off+i*(size+3),line,size,white)

states = [state for items in groups.values() for _, state in items]
tested_count = states.count("tested")
partial_count = states.count("partial")
estimated_progress = 100 * (tested_count + 0.5 * partial_count) / len(states)
t(38,25,"WIRELESS HE  /  PROJE İLERLEME HARİTASI",39,white,True)
t(2080,27,f"GENEL TAHMİN  %{estimated_progress:.0f}",26,green,True)
t(40,76,"Donanım · test platformu · macOS · aydınlatma · firmware · profiller  /  05 Kasım 2026",19,muted)
for x,col,label in [(1460,green,"Tamamlandı / denendi"),(1735,yellow,"Kod / taslak · test eksik"),(2250,grey,"Eksik")]:
    d.rectangle((x,78,x+18,96),fill=col);t(x+27,75,label,18)
states=[state for items in groups.values() for _,state in items]
score=round(100*(states.count("tested") + 0.5*states.count("partial"))/len(states))
t(38,128,f"PROJE ÖZELLİKLERİ  /  {len(states)} DURUM KARESİ",24,white,True)
t(2010,128,f"GENEL DURUM  ~%{score}",24,green,True)
t(40,162,"Yaklaşık oran: denenmiş kare = 1, kısmi = 0,5, eksik = 0. Bazı genel özellikler alt kontrolleriyle birlikte sayılır.",18,muted)
cols=4;gap=15;width=619;height=335
for index,(name,items) in enumerate(groups.items()):
    col=index%cols;row=index//cols;x=38+col*(width+gap);y=199+row*(height+gap)
    box(x,y,width,height);t(x+15,y+12,name,20,white,True)
    for i,(label,state) in enumerate(items):
        xx=x+13+(i%4)*149;yy=y+48+(i//4)*69
        colors={"tested":(green,"#174b34"),"partial":(yellow,"#5a401d"),"missing":(grey,"#364654")}
        color,bg=colors[state];tile(xx,yy,142,63,label,color,bg)
y=1249
x=1306
box(x,y,619,335)
t(x+15,y+12,"HAZIR PROFİLLER",20,white,True)
entries=list(profiles.items())
total=sum(map(len,profiles.values()))
supported=sum(sum(state=='ready' for _,state in values) for values in profiles.values())
setup=sum(sum(state=='setup' for _,state in values) for values in profiles.values())
for idx,(name,values) in enumerate(entries):
    yy=y+46+idx*20
    t(x+14,yy,name,14,white)
    for j,(_,state) in enumerate(values):
        sx=x+240+j*19
        d.rectangle((sx,yy+2,sx+13,yy+15),fill={'ready':green,'setup':yellow,'missing':grey}[state])
    t(x+500,yy,f"{sum(state=='ready' for _,state in values):02d}/{len(values):02d}",14,muted)
t(x+15,y+311,f"{supported} hazır · {setup} hedefte atama gerekli · test değildir",13,muted)

x=38;y=1599
box(x,y,2521,335)
t(x+20,y+15,"ÖNCELİKLİ EKSİKLER",23,white,True)
notes=[("01","BLE yeniden bağlanma ve bağlantı güvenliği"),("02","1 kHz kablosuz aktarım ve paket kaybı"),("03","Duty cycle: voltaj, ters besleme ve akım ölçümü"),("04","Encoder, analog gamepad ve pil testi")]
for i,(n,label) in enumerate(notes):
    yy=y+56+i*49
    t(x+20,yy,n,19,yellow,True);t(x+65,yy,label,19,white,True)
t(x+20,y+271,"Harf / ses HID ve kalıcı kayıt denendi; gerçek güç tasarrufu henüz ölçülmedi.",16,muted)
t(38,1995,"Genel tahmin: yeşil 1, sarı 0,5, gri 0 puan; durum kareleri eşit ağırlıklıdır, gerçek proje tamamlanma ölçümü değildir.",17,muted)
t(38,2023,"Yeşil profil karesi kod karşılığını gösterir; gerçek uygulama testi anlamına gelmez.",17,muted)
OUT.parent.mkdir(parents=True,exist_ok=True);im.save(OUT,optimize=True)
print(f"Görsel: {OUT} | {sum(len(v) for v in groups.values())} özellik + {total} profil tuşu")

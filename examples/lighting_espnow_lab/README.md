> **Güncel durum — 5 Kasım 2026:** Bu dosyanın aşağıdaki tarihli test notları kendi dönemine aittir. Projenin güncel C6/S3, BLE, HID ve doğrulama durumu için [bütünleşik rehbere](../../Docs/Proje_Rehberi.md) bakın.

# ESP-NOW aydınlatma laboratuvarı

Bu **bağımsız örnek**, Wireless HE Studio'daki ışık profillerini ve animasyon hesaplarını gelecekteki C6 klavyeye taşımanın bir yolunu gösterir. Mevcut Mac uygulamasını, `examples/macropad_12_idf` USB testini ve `Firmware/` altındaki C6/S3 kodunu değiştirmez.

> **Durum — 1 Kasım 2026:** İki proje ESP-IDF 6.1 ile derlendi; animasyon motoru, paketleme ve Studio yedeğinden dönüşüm için bilgisayarda testler var. C6, S3, LED'ler, analog sensörler ve ESP-NOW ile gerçek donanım testi yapılmadı. Bu örnek hiçbir karta yüklenmedi. Kablosuz test ancak kullanıcı daha sonra istediğinde yapılmalı.

## Temel fikir

```text
Wireless HE Studio → tam JSON yedeği → send_backup.py
                                           │ USB seri komutu
                                           ▼
                                    S3 dongle (gönderici)
                                           │ ESP-NOW: profil / katman
                                           ▼
                                   C6 klavye (alıcı)
                             ┌─────────────┴─────────────┐
                             │ NVS: 4 profil + aktif katman│
                             │ yerel sensör okuma          │
                             │ animasyon motoru            │
                             ▼                             ▼
                      12 SK6812 LED                uygulama seviyesi ACK
```

S3 her LED karesini veya tuş basışını kablosuz göndermez. Bir profil değiştiğinde parametreleri gönderir; C6 animasyonu yerel saatine ve kendi tuş verilerine göre hesaplar. Bu, tepkisel ışığın çalışması için sürekli S3/Mac bağlantısı gerektirmez. Ağ bağlantısı kurulması, LED zincirinin eşleşmesi veya aynı görünüm henüz doğrulanmış değildir.

### Ne var, ne henüz yok?

| Konu | Bu örnekte |
|---|---|
| Sekiz efekt | Kapalı, Sabit, Nefes, Dalga, Gökkuşağı, Kayan ışık, Tepkisel, Basılan yerden dalga motoru |
| Efekt seçenekleri | Renk, parlaklık, hız; ilgili efektlerde rengârenk, derinliğe duyarlılık; kayan ışıkta tur başına tek renk |
| Basılan yerden dalga | Aynı anda en fazla 64 dalga olayı, renk ve şiddet karışımı; kapasite dolarsa en eski olayın yuvası kullanılır |
| C6 analog giriş | Ayrı örnekte isteğe bağlı MCP3202 + CD74HC4067 okuma ve tuş başına yerel kalibrasyon; varsayılan olarak kapalı, gerçek donanımda test edilmedi |
| Mac → S3 | Mevcut uygulama değiştirilmeden **tam Studio JSON yedeği** ve komut satırı yardımcısı; uygulamanın içinde doğrudan Gönder düğmesi yok |
| S3 → C6 | Doğrulamalı sürümlü ESP-NOW paketi, S3 yeniden denemesi, C6 uygulama ACK'si ve NVS profili; fiziksel test yok |
| Gerçek LED | RMT sürücüsü derlenebilir; varsayılan olarak kapalı. GPIO, güç etkinleştirme ve fiziksel LED sırası doğrulanmalı |
| Üretim firmware'i | Bu örnekle iletişim kuracak şekilde değiştirilmedi. Yerleştirme ayrı bir geliştirme ve test işidir |
| Güç/pil/güvenlik | Gerçek güç modları, pil parlaklık sınırları, uyku ve güvenli eşleştirme uygulanmış değildir |

## Görsel eşleşmenin sınırı

Motor `components/lighting_core/lighting.c`, uygulamadaki güncel `Native.swift` mantığından hareketle süre, dalga yayılımı, parlaklık, derinlik ve renk hesaplarını uygular. Parametre anlamları ve 3×4 kanal sırası aynıdır. Bununla birlikte ekran modeli ve gerçek LED'ler **piksel piksel aynı görünemez diye garanti verilmez**: ekranın renk uzayı/gaması, LED renkleri, difüzör, güç kaynağı, 2B/3B materyaller, zaman başlangıcı ve gerçek LED sırası farklıdır. C6 animasyon saati açılış zamanından, Mac önizlemesi kendi saatinden ilerler; anlık renk veya faz otomatik olarak eşzamanlanmaz. Duyarlı dalgada fiziksel basma süresi ve örnekleme de sonucu etkiler. Donanımla kıyas yapıldıktan sonra gerekirse renk/gama ve zaman düzeltmesi eklenmelidir.

## Klasörler

```text
lighting_espnow_lab/
├── keyboard_c6/              # Ayrı ESP-IDF projesi: C6 alıcı + animasyon + isteğe bağlı ADC/LED
├── dongle_s3/                # Ayrı ESP-IDF projesi: S3 gönderici, seri komut konsolu
├── components/lighting_core/ # Donanımdan bağımsız efekt motoru ve paket biçimi
├── components/lab_radio/     # ESP-NOW ve callback → kuyruk adaptörü
├── components/lab_neopixel/  # Projedeki SK6812 RMT sürücüsünün ayrı kopyası
├── components/{spi_bus,mcp3202,mux_cd74hc4067}/ # Mevcut C6 bileşenlerinin ayrı kopyaları
├── send_backup.py            # Studio tam yedek JSON → S3 seri komutu
└── tests/                    # Donanımsız C/Python testleri ve örnek yedek parçası
```

Kopyalanan sürücülerin mevcut başlıkları/lisansları korunmuştur. Kaynak dosyaların asılları değiştirilmez. Her iki proje ESP-IDF bileşenlerini ortak klasörden derler, ancak birbirinin veya mevcut firmware'in `app_main` fonksiyonunu çalıştırmaz.

## Donanımsız test ve derleme

Bu Mac'teki ESP-IDF 6.1 yolu kullanılarak, proje kökünden:

```bash
examples/lighting_espnow_lab/tests/run_tests.sh
```

Yukarıdaki testler bilgisayarda çalışır; LED sürmez, seri port açmaz ve radyo kullanmaz. C testleri temel efektleri, basınca tepkiyi, üst üste dalgaları, profil doğrulama ve CRC/paket bozulmasını; Python testleri Studio JSON → S3 komut eşlemesini sınar. Radyo/NVS/ADC/RMT donanımını doğrulamaz.

Örnek veriyi bağlantı açmadan dönüştürme:

```bash
python3 examples/lighting_espnow_lab/send_backup.py \
  examples/lighting_espnow_lab/tests/fixtures/studio-light-only.json \
  --layer 1 --dry-run --activate
```

Bu küçük fixture yalnız dönüştürücüyü sınamak içindir; uygulamaya geri yüklenecek tam Studio yedeği değildir.

C6 ve S3'ü yalnızca **derlemek** için:

```bash
source ~/.espressif/v6.1/esp-idf/export.sh
cd examples/lighting_espnow_lab/keyboard_c6
idf.py build
cd ../dongle_s3
idf.py build
```

Derleme `flash` değildir. Bu çalışmada varsayılan C6/S3 yapılandırmaları ve C6'da LED/analog/iki güç kontrolü derleme seçenekleri açık bir ek yapılandırma başarıyla derlendi. Varsayılan C6 ve S3 uygulama bölümleri 1 MB'tır; derleme çıktısındaki boş alanı takip edin. Işık ve analog bileşenlerinin açık olduğu derleme, bunların devrede çalıştığı anlamına gelmez.

## Daha sonra cihaz üzerinde denenirken

Bu bölüm gelecek bir test planıdır; otomatik olarak çalıştırılmaz ve mevcut test sistemine uygulanmış değildir.

1. **Yalnız bu bağımsız firmware'i kullanacağınız ayrı kartlar** belirleyin. Bir karta bu örneğin firmware'ini yüklemek, o kartta çalışan mevcut uygulamanın yerini alır. Önce mevcut firmware ve verilerin korunmasını planlayın.
2. Her kartın Wi-Fi **STA MAC** adresini güvenilir biçimde öğrenin. Bu laboratuvar uygulamaları açılışta kendi `Wi-Fi STA MAC` adreslerini seri konsola yazdırır. İlk kurulumda MAC alanı boş olsa da yerel adres yazdırılır; boş veya geçersiz peer ile radyo etkinleşmez.
3. Her projenin `idf.py menuconfig` → `Lighting Lab` bölümünde **karşı kartın** MAC adresini ve aynı 1–13 Wi-Fi kanalını girin: C6'ya S3 MAC, S3'e C6 MAC. `LAB_PEER_MAC` varsayılanı boştur. Biri başka ağa bağlanır veya kanal değişirse iletişim kesilebilir.
4. `Lighting Lab C6 hardware` bölümünde gerçek devre doğrulanmadan LED çıkışını veya güç kontrolünü açmayın. C6 ana projedeki varsayılan varsayım: veri **GPIO5**, 12 SK6812MINI; LED boost enable **GPIO14**, aktif seviye 1. Bunlar derlenmiş kaynak tanımlarıdır, mevcut karta fiziksel olarak doğru oldukları bu çalışmada ölçülmedi. Bu örnekte hem LED çıkışı hem boost kontrolü varsayılan kapalıdır; istenirse ayrı ayrı açılır ve pin/seviye değiştirilebilir.
5. `key_to_led` dizisi ilk hâlde `0...11` kimlik eşlemesidir ve **PCB üzerindeki gerçek LED zinciri sırası doğrulanmış değildir**. Doğru fiziksel eşleşme ölçülmeden dalga yönü veya tuş–LED eşleşmesi kesin değildir. Düşük parlaklıkla tek tek kontrol edip yalnız bu örneğin dizisini düzeltin.
6. Analog giriş kullanılacaksa `Lighting Lab analog inputs` kısmında seçin. Bu özellik ve analog güç kontrolü de varsayılan kapalıdır. Varsayılan GPIO değerleri mevcut C6 kaynaklarındaki varsayımlardır; USB testindeki ESP32 DevKit pinleri değildir. **Önemli:** kullanıcının paylaştığı şemada mux çıkışı MCP3202 **CH0**'a çizilmişken mevcut C6 ana kodu `mcp3202_read_channel(1)` kullanıyor. Laboratuvarın ADC kanal varsayılanı CH0'dır, fakat gerçek devre ve hangi yazılımın doğru olduğu donanımda doğrulanmadan kesin deneme yapılmamalıdır. Güç enable yönünü de devreden kontrol edin.
7. Kartlar, LED beslemesi, sinyal seviyesi, GND, pin çakışması, pil akımı ve bağlantı yönleri doğrulandıktan sonra her örnek için uygun kart/port seçilerek kullanıcı onayıyla yükleme ve seri izleme yapılabilir. Bu örnek mevcut pil/güç koruma algoritmalarını uygulamaz; parlaklık `%100` seçilebilir.

### Yerel C6 kalibrasyonu ve test konsolu

C6 konsolu 115200 baud'dur. Analog okuma etkinse `raw` on iki ham değeri bir kez yazdırır. Tuş başına serbest ve tam basılı değerleri, fiziksel switch ilgili konumdayken ayrı ayrı kaydedin:

```text
raw
free 4
full 4
```

`free 4` ve `full 4` sırasıyla KEY 4'ün o andaki ADC değerini NVS'e kaydeder. Diğer tuşlar için 0–11 kullanılır. İki değer de mevcut ve aralarındaki mutlak fark en az 10 ADC birimi olmadan o kanalın gerçek derinlik girdisi üretilmez. İleri/ters sensör yönü kalibrasyondaki değerlerden hesaplanır. Bu değerler C6'ya **yerel** kaydedilir; Mac uygulamasındaki kalibrasyon otomatik aktarılmaz. `p 4 30` ve `p 4 0`, analog okuma kapalıyken sırasıyla örnek %30 basma ve bırakma olayı verir; gerçek sensör değildir. `show` 12 hesaplanmış RGB değerini konsola yazar. Analog okuma açıkken `p` girdisi bir sonraki gerçek okumayla değiştirilebilir.

Bu örneğin `read_analog()` işlevi multiplexer kanalını seçer, bekler, bir ADC dönüşümünü atar, diğerini alır ve kalibrasyon geçerliyse `light_engine_input()` çağırır. Bu kod gerçek cihazda doğrulanmamıştır; kanal sırası, gürültü, ADC komut zamanlaması, kalibrasyon yöntemi ve sensör besleme durumu ayrıca sınanmalıdır.

## S3 konsolu ve Mac'ten profil gönderimi

İki test firmware'i yapılandırıldıktan ve kullanıcı donanım testini istediğinde, S3'ün 115200 baud seri konsoluna bir satır gönderilebilir:

```text
set 0 breathe 166 240 202 60 50 1
active 0
```

`set` alanları sırasıyla hedef katman **0–3**, efekt, R/G/B **0–255**, parlaklık **0–100**, hız **1–100**, bayrak bit maskesidir. `active` yalnızca hangi kayıtlı profilin gösterileceğini seçer. Kullanılabilir efekt adları:

```text
off static breathe flow rainbow sweep reactive ripple
```

Bayraklar: `1` renkli, `2` derinliğe duyarlı, `4` kayan ışıkta her tur tek renk. Birleştirilir: renkli ve derinliğe duyarlı dalga `3`, renkli ve tur başı tek renk kayan ışık `5` olur. Derinlik bayrağı yalnız `reactive`/`ripple`, `4` yalnız `sweep` için geçerlidir. Örnek `set 0 breathe ... 1` C6'daki **0. katmanı kaydeder**, kendi başına başka bir katmanı etkin yapmaz. `active 0` ayrı bir işlemdir. Uygulama arayüzünde Layer 1, protokolde **0**'dır.

Mac uygulamasında **Dosya → Yedeği Dışa Aktar** ile tam yedek alındıktan sonra, S3'e bağlı seri portu başka program kullanmıyorsa:

```bash
python3 examples/lighting_espnow_lab/send_backup.py /path/to/studio-backup.json \
  --layer 0 --port /dev/cu.usbmodemXXXX --activate
```

`/path/to/studio-backup.json` ve port örnektir; kullanıcının gerçek dosyası ve portu seçilir. `--layer` kaynak, isteğe bağlı `--target` C6 hedef katmanıdır. `--dry-run` seri bağlantı kurmadan gönderilecek komutu gösterir. `--activate`, profil için `RESULT … OK` alındıktan sonra ayrıca aktif katman komutu gönderir. Yardımcı `pyserial` gerektirir; aynı S3 seri portunda `idf.py monitor` açık olmamalıdır. Önce S3 konsolunun hazır olduğunu doğrulayın; bazı donanımlarda seri port açma reset veya bağlantı davranışını etkileyebilir.

**Kapsam sınırı:** Bu yardımcı gerçek uygulamaya yeni bir gönderim butonu eklemez. Yalnız uygulamanın **tam yedek** formatındaki `config.layers[n].light` alanını çevirir; `Gelişmiş` sayfasının yalnız `Config` üreten eski JSON dışa aktarımı aynı biçim değildir. Katman görevlerini, makroları, kalibrasyonu veya özel renk paletini C6'ya göndermez; seçilen katmandaki ışık rengi ve efekt bayraklarını gönderir.

## Paket, onay ve güvenlik

`protocol.c`, C yapısının belleğini doğrudan kabloya kopyalamak yerine sabit bayt biçimi kullanır. ESP-NOW v1 sınırının altında olan paketlerde çok baytlı sayılar little-endian'dır:

| Alan | Boyut |
|---|---:|
| `HELA` magic | 4 |
| sürüm `1`, tür `1=profil / 2=aktif / 3=ACK` | 1 + 1 |
| payload uzunluğu | 2 |
| rastgele oturum kimliği, sıra numarası | 4 + 4 |
| payload | 0–32 |
| CRC-32/IEEE | 4 |

Profil payload'u 8 bayttır: `layer,effect,r,g,b,brightness,speed,flags`. Aktif katman payload'u 1 bayt, ACK payload'u bir durum baytıdır (`0=OK`, `1=geçersiz`, `2=NVS hatası`). Alıcı uzunluk, sürüm, CRC, tür, profil ve kaynak MAC'i kontrol eder. Wi-Fi alma callback'i yalnız veriyi kuyruğa kopyalar; animasyon/NVS işleri ayrı görevde yürür. Alıcı geçerli isteği NVS'e kaydedip uyguladıktan sonra ACK kuyruğuna yazar; ayrı ACK görevi radyo gönderimini yapar. Aynı oturum ve sıra numaralı tekrar, yeniden yazmadan son ACK'yi döndürür; aynı oturumda eski sıra reddedilir. Gönderici önce MAC katmanı geri bildirimini, sonra eşleşen **uygulama ACK**'sini bekler; zaman aşımında aynı oturum/sıra ile en çok üç gönderim dener. `RESULT … OK` C6'nın uygulama düzeyinde onay verdiğini belirtir; LED'lerin görsel olarak doğru olduğunun kanıtı değildir. Uygulama ACK'si kaybolursa gönderici başarı alamasına rağmen profil C6'ya kaydedilmiş olabilir.

**Yalnız laboratuvar güvenliği:** Bu örnek ESP-NOW peer'i `encrypt=false` olarak kurar. Kaynak MAC filtresi ve CRC kimlik doğrulama veya şifreleme değildir; taklit ve yetkisiz profil değiştirmesine karşı koruma sağlamaz. Açık ortamda/üretimde kullanılmadan önce anahtar üretimi ve güvenli eşleştirme, ESP-NOW şifreli unicast ve tekrar saldırısı tasarımı ayrıca uygulanmalıdır. GPIO gücünü, parlaklık sınırlarını, NVS hata yönetimini ve cihaz uyku/alma pencerelerini de üretim sistemine uygun ele almak gerekir.

## Mevcut C6/S3 firmware'ine ileride entegrasyon

Bu örnek kendi Wi-Fi ve ESP-NOW kurulumunu yapar ve kendi callback'lerini kaydeder. Mevcut firmware'e aynen ikinci kez başlatıcı/callback olarak eklenmemelidir. Mevcut `espnow_link` yalnız gönderim tarafını kuruyor; mevcut S3 `espnow_rx` ise C6'dan gelen **5 veya 23 baytlık** verileri kabul ediyor. Bu laboratuvarın `HELA` paketleri onlarla doğrudan uyumlu değildir. Tek bir mevcut radyo yöneticisine tür/uzunluk/kimlik doğrulama tabanlı yönlendirme, çift yönlü alma penceresi ve uygulama ACK'si entegre edilmelidir. Kontrol paketi göndermenin mevcut 1 kHz hedefli tuş akışını, fail-safe veya HID raporlarını engellememesi gerçek donanımda ölçülmelidir.

Animasyon motoru mevcut sürücüye şu sözleşmeyle bağlanabilir:

1. C6 sensör katmanında her `KEY 0...11` için güvenilir kalibrasyon ve `0.0...1.0` basılma yüzdesi üret.
2. Sırasıyla `light_engine_input(engine, key, depth, monotonic_ms)` çağır; %10 basma / %5 bırakma eşiği görsel olay ve dalga üretir. Bu eşik gerçek HID aktivasyon ayarı olarak kullanılmaz.
3. Ayrı sabit hızlı LED işinde `light_engine_render(engine, monotonic_ms, frame)` çağır; 12 RGB değerini doğrulanmış kanal→fiziksel LED sırasıyla sür. Tek görev sahipliği veya uygun kilitleme kullan; Wi-Fi callback'inde NVS, RMT veya uzun işlemler yapma.
4. Profil değişikliklerini doğrula, NVS'e kaydet, motoru güncelle ve yalnız bundan sonra uygulama ACK'si gönder. Uyku ve pil durumunda LED gücü/limitini ayrı güç yöneticisine bırak.
5. Mac uygulaması için gelecekte S3 USB yapılandırma protokolü ve arayüz butonu tasarla; bu örnekteki `send_backup.py` geçici köprüdür. Gönderilen ayarın gerçekten kaydedildiği ACK ve gerektiğinde tekrar okuma ile gösterilmeli.

Üretim alıcı/sender kaynaklarında aynı ESP-NOW kanalının ve kontrol modunun veri akışı, şarj, düşük pil ve uykuya etkileri netleştirilmeden bu örnek doğrudan birleştirilmemelidir.

## Başlıca kaynaklar

- Espressif: [ESP-NOW, ESP32-S3 ve ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/network/esp_now.html) — eşler, kanal, MAC geri bildirimi ve uygulama ACK'si.
- Espressif: [ESP-NOW, ESP32-C6](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32c6/api-reference/network/esp_now.html) — C6 tarafı ESP-NOW API'si.
- Espressif: [LED strip component](https://components.espressif.com/components/espressif/led_strip) — alternatif adreslenebilir LED sürücüsü; bu laboratuvar mevcut projeden kopyalanan RMT sürücüsünü kullanır.

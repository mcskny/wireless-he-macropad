> **Güncel durum — 5 Kasım 2026:** Bu dosyanın aşağıdaki tarihli test notları kendi dönemine aittir. Projenin güncel C6/S3, BLE, HID ve doğrulama durumu için [ana README’ye](../../Readme.md) bakın.

# C6 → S3 kablosuz sensör köprüsü - bağımsız ilk aşama

> **Güncel durum — 4 Kasım:** `c6/` ve `s3_composite/` birlikte fiziksel testte kullanılıyor. S3; USB CDC, 6KRO klavye ve medya HID çıkışı sağlar. Studio seçili katmanın desteklenen görevlerini S3'e kalıcı yazabilir. İki katman, yeniden bağlanma ve Studio kapalıyken klavye çıkışı; ayrıca ses azaltma fiziksel olarak denendi. Daha eski tarihli bölümlerdeki yalnız telemetri / RAM-only ifadeleri o döneme aittir. Analog gamepad, Rapid Trigger ve encoder henüz bu kayıt yolunda tamamlanmış değildir.

> **SOCD güncellemesi:** Katman başına bir çift için son basılan tuşa öncelik veren davranış S3'e ve Studio'nun yazma yoluna eklendi. Aynı örnekte eşzamanlı başlayan iki tuş nötrlenir; baskın tuş bırakıldığında hâlâ basılı olan tuş devam eder. İki tuşa da görev atanmalıdır. Eski sıfır baytlı HID kayıtları desteklenir; yeni SOCD kayıtları eski S3 sürümlerince okunmaz. Birim testleri ve fiziksel A/D öncelik testi geçti.

> **Rapid Trigger / profil v2:** S3'te hareket yönüne dayalı basma ve bırakma eşikleri ile üst/alt sınırlar uygulanır; SOCD ile birlikte kullanılabilir. Studio ve sunucu yeni 16 baytlık kayıtları ayrı `CFG,L2` işlemiyle yazar. S3 açılırken mevcut `hid_v1` kaydı varsa bellekte v2'ye dönüştürür; ilk başarılı yeni yazma `hid_v2` olarak kaydeder ve eski kaydı korur. Eski uygulama yeni biçimi yazamaz. Birim testleri geçti; fiziksel Rapid Trigger ve yeniden açılışta profil taşıma testi henüz doğrulanmadı. Analog gamepad ve encoder eksiktir.

Bu iki ayrı ESP-IDF projesi, mevcut XIAO C6 doğrudan USB sensör testini ve `Firmware/` altındaki hedef ürün firmware'lerini değiştirmez. Amaç, sensörleri C6'dan okuyup ESP-NOW ile S3'e iletmek ve Mac'teki mevcut seri sensör sunucusunu S3 USB portundan beslemektir.

**Durum (2 Kasım 2026):** XIAO C6 ve XIAO S3 kartlarına bu laboratuvarın C6 ve `s3_composite` sürümleri yüklendi. 12 ham kanal C6 → ESP-NOW → S3 USB CDC → Mac sensör sunucusunda görüntülendi. Tuş basışları, HID çıkışı ve güç tasarrufu modu test edilmedi.

## Veri yolu (ilk aşama, tarihsel)

```
Hall → mux → MCP3202 → XIAO C6 → ESP-NOW kanal 6 → XIAO S3
                                                        │ USB Serial/JTAG
                                                        ▼
                                      server.py → Mac uygulamasındaki canlı test
```

C6'nın mevcut testteki D0–D4 ve D8–D10 bağlantıları aynıdır; `examples/xiao_c6_sensor_test/README.md` tablosuna bakın. C6 1 kHz hedefinde kendi USB portuna her sensör satırını yazmaz; yalnızca saniyelik STAT tanılama satırı verir; kablosuz testte uygulamanın sensör portu olarak **S3** seçilmelidir. Uygulamayı yeniden başlatınca seçim sunucuya uygulanır. İki proje ayrı olduğundan yanlış porta yazılmaması için cihaz kimliği ve port fiziksel bağlantıyla ayrıca doğrulanmalıdır.

C6, `WHT1` + 32-bit sıra + 12 adet 16-bit ham ADC + CRC-16 içeren 34 baytlık test paketi yayınlar. S3 uzunluk, magic, CRC ve 0–4095 aralığını doğrular; en yeni geçerli örneği tek elemanlı kuyrukla alır ve `K,sıra,12 değer` satırını USB'ye verir. RF kaybında yeni satır üretilmez; mevcut seri sunucusu zaman aşımında bağlantıyı keser. Sıra boşlukları kayıp tespitine yardımcı olur; RF gönderiminin ve sensör verilerinin bütünüyle doğru olduğunu kanıtlamaz.

## Önemli kapsam ve güvenlik sınırları (ilk aşama, tarihsel)

- Bu ilk aşama **yalnız sensör telemetrisidir**. S3'ün USB modu Serial/JTAG'dir; klavye, medya veya gamepad HID raporu üretmez. Aynı USB üzerinden uygulama yapılandırması, cihaz ACK/geri okuma ve gerçek layer/encoder akışı yoktur.
- ESP-NOW test yayını **şifresiz broadcast** kullanır; S3 aynı kanal üzerindeki biçimi doğru bütün WHT1 paketlerini kabul eder. CRC kimlik doğrulama değildir. Başka bir kaynak veriyi taklit edebilir. Bu nedenle yalnız kontrollü test ortamı içindir; kullanıcı girdileri veya üretim için güvenli kabul edilmez. Gerçek kullanım için kimlikli eşleştirme, şifreli unicast, tekrar koruması ve MAC yönetimi gereklidir.
- ESP32-S3'te USB Serial/JTAG ile USB OTG aynı PHY'yi paylaşır. Aynı anda gerçek HID ve uygulama telemetrisi için USB OTG üzerinde **kompozit HID + CDC** tasarlanıp test edilmelidir; bu örnek böyle bir kompozit aygıt değildir.
- C6 zamanlayıcısı 1 ms periyotla tam 12 kanallı tarama ve gönderim hedefler; 1 kHz fiziksel donanımda henüz ölçülmemiştir. Yetişmeyen zamanlayıcı bildirimleri, meşgul/başarısız radyo gönderimleri ve S3'ün tek örnek kuyruğu sıra boşluğuna yol açabilir. Sıra numarası cihaz yeniden açıldığında sıfırlanır. Mac bağlantı durumundaki `connected` geçerli seri verinin varlığını gösterir; güvenli eşleşmeyi veya C6 kimliğini doğrulamaz.
- Işık, güç, batarya, fiziksel layer ve encoder bu örnekte yoktur. Ürün C6 firmware'inin XIAO testinden farklı pin ve güç varsayımları vardır.

## Donanımsız doğrulama

Her iki proje `idf.py set-target esp32c6/esp32s3` ve `idf.py build` ile derlendi. Paketin yanlış uzunluk, tek bit bozulması, ADC sınırı ve sayaç sınırı senaryoları şu komutla sınanabilir:

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I examples/c6_s3_sensor_bridge/shared \
  examples/c6_s3_sensor_bridge/tests/test_sensor_packet.c \
  -o /tmp/whe-sensor-radio-test
/tmp/whe-sensor-radio-test
```

Bu test radyo veya USB donanımını doğrulamaz. Kartlara yazma için kullanıcı kartları bağladığında C6 ve S3 portlarını ayrı ayrı kimliklendir, mevcut yazılımın üzerine yazılacağını dikkate al ve önce sadece bu bağımsız örneği test et. S3 portu USB moduna göre değişebilir.

## 1 kHz hedefi ve süre bütçesi

C6 ESP timer 1000 mikrosaniyede bir tarama bildirimi üretir. Her mux kanalı için 20 µs beklenir, ilk ADC dönüşümü atılır ve bir dönüşüm alınır; 1 MHz SPI ve 24 saatlik iki dönüşümle yalnızca teorik alt sınır `12 × (20 + 2 × 24) = 816 µs` olur. GPIO, SPI sürücüsü, radyo ve zamanlayıcı işleri bunun üzerindedir. Önceki dört örnek ortalaması bu hız hedefi için kaldırılmıştır; hassasiyet ve kanallar arası bulaşma fiziksel olarak karşılaştırılmalıdır.

Sadece bir ESP-NOW gönderimi uçuşta tutulur. Gönderici meşgulken tek bir en yeni örnek saklanır; yenisi geldikçe eskisi üzerine yazılır. Tamamlanma bildirimi yerel sürücü bildirimidir, alıcı ACK değildir. 4 ms yaş sınırını aşan örnekler atılır. Zamanlayıcı taşması, üzerine yazma veya başarısız gönderimde sıra numarası ilerlediğinden alıcıda boşluk gözlemlenebilir. C6'nın düşük öncelikli ayrı görevi USB konsoluna saniyelik `STAT scans=... skipped_ticks=... radio_busy=... send_error=... submitted=... scan_max_us=...` yazar; bu değerler radyo alımını veya oyundaki HID hızını tek başına göstermez. S3 USB seri satırlarını tek yazmada üretir. Mac uygulamasının 20 ms sorgu aralığı 1 kHz görsel ekran hızı değildir.

12 tuşun her biri için 4 kHz, 250 µs'de tüm kanallar demektir. Mevcut 1 MHz / 24 bit SPI biçiminde, hiçbir yerleşme beklemesi ve ek dönüşüm olmasa bile 12 × 24 = 288 µs gerekir. Tek örnek ADC işlemi, daha hızlı uygun SPI ve gerçek sinyal yerleşme süresi üzerinde ayrı çalışma ve fiziksel ölçüm olmadan 4 kHz iddia edilemez. DRV5055 ve CD74HC4067'nin veri sayfasındaki bant genişliği tek başına bütün ADC taramasının 4 kHz olduğunu göstermez.

## Deneysel güç tasarrufu: 100 Hz tarama / 10 Hz boşta gönderim

`c6/sdkconfig.power.defaults` ayrı derleme seçeneği MOSFET denemesini açar; normal `sdkconfig.defaults` bu özelliği **kapalı** tutar. XIAO test bağlantısında ilave D5 / GPIO23, şemadaki Q1 P-MOSFET `ANALOG_PWR_EN` gate hattına gider. Şemadaki 10 kΩ pull-up gate'i 3,3 V'a çeker; kodda gate LOW = analog rail açık, HIGH = kapalı varsayılır. Bu polarite ve kablolama gerçek devrede ölçülmelidir. Üretim C6 pin haritasıyla karıştırmayın.

Kullanıcı önce `VCC_ANALOG` hattını 3,3 V'a doğrudan bağladığını belirtmiştir. Bu köprü hâlâ mevcutsa Q1 sensörleri kesemez ve güç tasarrufu gerçekleşmez; doğru Q1 bağlantısı ve güvenli besleme düzeni donanımda doğrulanmalıdır. MCP3202 beslemesi sürekli 3,3 V kalır; sensör ve CD74HC4067 rail'i kesilir. Mux beslemesi kapatılmadan önce S0–S3 LOW yapılır. MCP3202 CH0 dahil diğer bağlantılardan mux'a olası ters besleme, rail'in gerçekten düştüğü ve açılış eğrisi ölçülmelidir; yalnız kodla güvenli varsayılamaz.

- Boşta her 10 ms: Q1 açılır, örnek amaçlı 600 µs beklenir, 12 kanal taranır, ardından Q1 kapatılır. Bu bekleme; güç rail'i yükselme süresi, sensör yükü ve mux yerleşimi dahil sistemin doğrulandığı anlamına gelmez. DRV5055-Q1 veri sayfasında tON en çok 330 µs, VCC'nin 3 V'u geçmesinden itibaren 0 mT ve yüksüz testte OUT'un VQ'ya %5 yaklaşması olarak tanımlanır.
- Tuşlar ilk ölçümde serbest kabul edilir. Başlangıçta bir tuş basılıysa referans yanlış olabilir. İlk basit testte değişim 20 ham ADC veya üzerindeyse aktif moda geçilir; bırakma bölgesi 10 ADC, bütün kanallar bu bölgedeyken 2 saniye sonra boş moda dönüş olur. Bu sayılar `menuconfig` üzerinden değiştirilebilir; her tuşun kalibrasyonu değildir ve MOSFET darbelerindeki gürültü/drift henüz ölçülmemiştir.
- Boşta ESP-NOW en çok 10 Hz hedefler. 100 Hz iç taramaların kasıtlı gönderilmemesi paket kaybı olarak sayılmaz; uyanma veya aktif→boş geçiş örneği 100 ms beklenmeden hemen gönderilir. Aktifte her 1 ms tarama/gönderim hedeflenir, S3/USB üzerinden gerçek hız ve kayıp ölçülmelidir.
- 100 Hz iç taramada eşik geçişi ilk sıradaki ölçümle fark edilir: yalnız örnekleme fazından dolayı 0–10 ms bekleme olabilir, buna rail açılması, okuma ve RF/USB/HID gecikmesi eklenir. İki tarama arasında gerçekleşip biten çok kısa basış kaçabilir. Sensör parmak temasını değil, mıknatısın oluşturduğu alan değişimini algılar.
- `WIFI_PS_NONE` kullanılır, C6 uykuya geçirilmez; 10 Hz gönderim ve rail duty cycling'in toplam pil tüketimine etkisi ölçülmemiştir. Sensör rayının açık kalma oranı, tüm sistemin güç tasarrufu veya pil ömrüyle eşdeğer değildir.

Etkin örneği üretmek için proje dizininde, IDF ortamı yüklüyken:

```sh
idf.py -B build-power -D IDF_TARGET=esp32c6 -D SDKCONFIG=sdkconfig.power \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.power.defaults' build
```

Her iki yapı derlenmiştir; ancak bu mod fiziksel olarak sınanmadı veya karta yüklenmedi. MAC doğrulaması, şifreleme, gerçek USB HID ve ayar senkronizasyonu bu bağımsız denemede yoktur. ADC kalitesi ve uyandırma gecikmesi ölçülmeden dijital wake devresini nihai tasarımdan çıkarmak için yeterli kanıt yoktur.

## Ayrı S3 USB CDC + HID denemesi

`s3_composite/`, mevcut `s3/` seri köprüsünden bağımsız bir ESP32-S3 projesidir. USB OTG üzerinde tek bir bileşik aygıt olarak sensör telemetrisi için CDC seri portu ile klavye ve gamepad HID arayüzünü tanımlar. CDC satırları mevcut uygulamanın kullandığı `K,<sıra>,<12 ham ADC>` biçimindedir. Bilgisayar CDC'yi okumuyorsa veya USB tamponu doluysa eski örnekler bekletilmez; yeni bir örnekle devam edilir. USB port adı farklı olabilir. ESP32-S3'ün USB OTG ve USB Serial/JTAG işlevleri aynı fiziksel USB PHY'yi paylaştığı için CDC portu üzerinden çalışırken konsol/JTAG portunun da kullanılabileceği varsayılmamalıdır.

**HID arayüzü henüz tuş, eksen veya kumanda raporu üretmez.** Radyo paketi yayın şeklinde, kimlik doğrulamasız ve şifresizdir. Güvenilir eşleştirme, kalibrasyon, profil aktarımı ve bağlantı kopmasında nötre dönüş geliştirilip doğrulanmadan sensör hareketlerinin bilgisayara tuş olarak gönderilmesi açılmamıştır. Bu çalışma ne oyun kontrolünü ne de üretim firmware'i için tümleşik ayar aktarımını tamamlar. `s3/` ile mevcut doğrudan kablolu sensör testi değiştirilmemiştir.

Derleme (ESP-IDF 6.1):

```sh
cd examples/c6_s3_sensor_bridge/s3_composite
idf.py set-target esp32s3
idf.py build
```

ESP32-S3 için derlenmiştir. Hiçbir karta yüklenmemiştir; USB aygıtının tanınması, uygulamada seri port seçimi, RF akışı ve hız fiziksel donanımda denenmemiştir. Donanım bağlandığında önce hangi S3 kartının ve USB bağlantısının kullanıldığı doğrulanmalıdır.

## 2 Kasım 2026: gerçek kablosuz deneme

XIAO C6 `/dev/cu.usbmodem3101`, S3 CDC `/dev/cu.usbmodemWHE_S3_LAB1` olarak görüldü. Tek ADC dönüşümü ve kanal başına 20 µs mux beklemesi ile testte C6 saniyede 1000 tarama bildirdi; S3 USB CDC üzerinden 10 saniyede 9819 satır ve sıra aralığında 181 boşluk görüldü (yaklaşık %1,8). Sıra boşlukları gönderici meşguliyeti, radyo, S3 kuyruğu veya USB kaynaklı olabilir; ayrı ölçüm gerekir. Boşta ADC değerleri önceki çift dönüşümle benzerdi, ancak komşu kanal etkisi ve gerçek tuş hareketi test edilmedi. Bu sebeple 1 kHz kayıpsız ve ADC doğruluğu kanıtlanmış değildir. Eski çift dönüşümlü taramada yaklaşık 625 satır/saniye görüldü. 100 Hz MOSFET denemesi açılmadı. S3 HID henüz rapor üretmez; yalnız CDC üzerinden sensör akışı çalıştı. Test yayını şifresizdir.

### Hazırlanan S3 tanılama güncellemesi

S3 kaynak kodunda saniyelik `D,` satırı hazırlandı: geçerli/geçersiz alım, S3'te gözlenen sıra boşluğu, sıra sıfırlanması/tekrarı, kuyrukta önceki örnek varken gelen örnekler ve CDC'ye alınan/aktarılamayan örnekler. `pending` iş parçacıkları arası yarış nedeniyle kesin üzerine yazma sayısı değil, yaklaşık bir göstergedir. `sequence_gaps` C6'nın kaçırdığı zamanlayıcı fırsatlarını ve gönderim meşguliyetini de içerir; tek başına RF kaybı olarak yorumlanamaz. Tanılama satırının kendisi de USB doluysa düşebilir. Mac sunucusu geçerli `D,` satırlarını `bridge_diagnostics` olayı olarak oturum günlüğüne ekler; eski telemetriyle uyumludur. S3 güncellemesi derlenmiştir ama henüz karta yüklenip fiziksel test edilmemiştir. Karttaki önceki sürüm tanılama satırı üretmez.

### SK6812 LED ilk bağlantı testi (3 Kasım 2026)

XIAO C6 üzerinde D6 / GPIO16, şemadaki U34 seviye dönüştürücü girişi `Neopixel` için kullanılır; D5 / GPIO23 analog güç denemesine ayrılmıştır. Ortak GND gereklidir. LED zincirinin beslemesi ve U34'ün besleme/enable bağlantısı gerçek kartta ayrıca doğrulanmalıdır; GPIO güç beslemesi değildir. SK6812MINI için 12 LED, düşük parlaklıkta tüm LED'ler kırmızı → yeşil → mavi → kapalı, her adım 1,5 saniye test programı eklendi. Bu yalnızca kablo ve LED zinciri testidir, uygulamadan ayar veya reaktif aydınlatma değildir. C6'ya yüklendi; C6/S3 sensör akışı yeniden görüldü, LED'lerin fiziksel yanması gözle doğrulanmadı.

## ACK kullanmayan en güncel örnek gönderimi

C6 radyo meşgulken örneği hemen atmak yerine bir hücrede en yeni örneği tutar; yeni örnek gelince eskisi değiştirilir. Yerel ESP-NOW gönderim tamamlandı bildirimi gelince eldeki en güncel örnek gönderilir. Bu alıcıdan ACK, tekrar gönderimi veya alındı doğrulaması değildir. 4 ms'den eski örnek iletilmez. `replaced_est` yaklaşık üst yazma göstergesi, `stale` yaş sınırını aşan örnek sayısıdır. Örnek kaybı, kısa basış kaçırılması, RF/USB sıralama boşlukları ve enerji tüketimi hâlâ mümkündür. Tam durum her pakette taşınır ancak geçici durumlar kaybolabilir; bu sürüm henüz klavye olayları için uçtan uca garanti sunmaz.

Normal ve deneysel güç yapıları ESP-IDF 6.1 ile derlendi; mevcut paket ve güç politikası testleri geçti. Yeni kuyruk davranışı fiziksel kartlarda ölçülmedi, hiçbir karta yüklenmedi; önceki %1,8 veya %2,3 değerlerine göre iyileşme varsayılmamalıdır.

## 3 Kasım 2026: ACK olmadan gerçek kart ölçümü

S3 tanılama sürümü ve C6 en-güncel-örnek kuyruğu gerçek XIAO C6 ve S3 kartlarına yüklendi. Karşılaştırma yalnız yakın mesafe, değişebilen radyo ortamı ve kısa ölçüm aralıkları içindir:

| Ölçüm | Süre | S3 geçerli RX | Sıra boşluğu | USB kabul edilmedi |
|---|---:|---:|---:|---:|
| Eski C6 gönderici | 15 sn | 14.053 | 956 (%6,37) | 0 |
| En güncel örnek C6 | 20 sn | 19.837 | 174 (%0,87) | 0 |
| En güncel örnek C6 | 30 sn | 27.445 | 2.578 (%8,59) | 0 |
| En güncel örnek C6 | 20 sn | 19.789 | 222 (%1,11) | 0 |

Oranlar kararsızdır; belirli bir iyileşme veya kalıcı kayıp garantisi çıkarılamaz. S3'te geçersiz paket ve USB bırakılması ölçülen aralıklarda sıfırdı. S3'ün sıra boşluğu C6 atlamalarını ve RF kaybını ayrı ayrı belirlemez. Sol üst tuş basılıyken fiziksel kanal 1 yaklaşık 1905'ten 0'a indi ve S3 üzerinden gözlendi; bu tek bir tuş ve uç değer testidir. 2 Mbps PHY denemesi bir 30 saniyelik pencerede %2,35 sıra boşluğu gösterdi; rate ayarı başarılı olmuş mu ve menzil etkisi doğrulanmadığından değişiklik geri alındı. Son durum: varsayılan radyo hızı, ACK'siz en güncel örnek göndericisi, S3 tanılama alıcısı; LED testi devam eder.

## S3 keyboard HID bench integration — 3 November 2026 (historical)

The S3 composite test firmware now includes a **real 6KRO USB keyboard report path** while retaining the `K,...` CDC sensor stream. On the physical C6 → ESP-NOW → S3 → Mac path, a temporary mapping of physical top-left key (raw ADC channel 1) to HID usage `0x0b` (`H`) was tested: the user confirmed `H` appeared in TextEdit. The test mapping was then disabled and the Studio sensor server was restarted. This validates that one mapping and the observed press path; it does not establish full keyboard, gamepad, profile or receiver security functionality.

The test firmware starts with HID **disabled**, settings are RAM-only, and closes keyboard output when its CDC host disconnects. A 150 ms valid-frame timeout clears pressed states. The HID path accepts samples with the test C6's expected Wi-Fi source address `b4:3a:45:80:cf:00`; this is **not cryptographic authentication** and an attacker can spoof it. Test broadcasts remain unencrypted. Do not use this as a secure production keyboard. The H test output is explicitly turned off; configuration does not survive reboot. HID USB is keyboard-only in this path; although a gamepad descriptor exists, gamepad reports are not yet produced.

### Temporary USB CDC bench commands

Commands are newline terminated; valid/invalid commands return `R,HID,OK` / `R,HID,ERROR`. CDC serial can be owned by only one process; do not open it concurrently with Studio's server. Commands are for bench use, not an implemented Studio settings interface. They are configuration acknowledgements over USB and do not add ACKs to sensor telemetry.

```text
HID,CLEAR
HID,SET,<adc_channel>,<released_adc>,<pressed_adc>,<usb_hid_usage>,<actuation_pct>,<hysteresis_pct>
HID,ON
HID,OFF
```

For example `HID,SET,1,1914,0,11,74,5` configures channel 1 as `H` for the calibration used in the single-key bench test. It is **not** a universal calibration or an invitation to overwrite saved user profiles. The physical channel mapping and limits must be measured per board. `HID,SET` is accepted only while HID is disabled; usages 4–101 and modifier usages 224–231 are accepted. At least one valid channel is required for `HID,ON`. 6KRO rollover is signaled when more than six different non-modifier keys are active. Closing CDC disarms HID, so the current firmware is not yet a standalone configured dongle. A safer, authenticated wireless protocol, persistent atomic configuration, richer behaviors, gamepad reports and a device-write path from Studio remain to be implemented.

Host-side HID state tests:

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I examples/c6_s3_sensor_bridge/s3_composite/main \
  examples/c6_s3_sensor_bridge/tests/test_hid_engine.c \
  examples/c6_s3_sensor_bridge/s3_composite/main/hid_engine.c -o /tmp/whe-hid-test
/tmp/whe-hid-test
```

## Dynamic scan / telemetry rates (3 November 2026 bench test)

The C6 test firmware now uses the movement policy independently of the optional MOSFET experiment. With all keys released, it targets **100 full scans/s** and sends sensor snapshots at **10 Hz**. When the native Studio process is running and its sensor server has the S3 port open, the app server sends a 250 ms USB `APP,1` heartbeat. The S3 sends an unacknowledged presence status to the C6 every 200 ms; while that status remains fresh, idle telemetry targets **100 Hz**. The S3's small status frames are additional to the C6 sensor telemetry. Closing only the Studio window keeps the app process and server running in the menu bar, so it remains in the app-open state. Quitting the application or losing its heartbeat returns to the 10 Hz idle target after timeout. Merely opening the serial port with another program does not send the app heartbeat.

When any sensor differs from its startup released baseline by the default 20 ADC counts, the detection scan is immediately sent and the next scans/telemetry target **1 kHz** without waiting for the configured key-actuation threshold. After all channels return within 17 counts of baseline for two seconds, it returns to idle. This release threshold was raised from 10 after released sensors showed baseline drift on the test hardware; it remains provisional and close to the wake threshold. The first scan assumes every key is released. False wake, drift, held-at-start behavior and cross-channel effects require hardware testing. A 100 Hz idle scan can introduce 0–10 ms phase delay plus processing/radio/USB latency; brief motions entirely between scans may be missed. The activity status uses source-address filtering and CRC only, with no authentication or encryption; sensor telemetry remains unacknowledged.

Only the `CONFIG_WHE_POWER_GATE_EXPERIMENT` build drives the active-low D5 MOSFET gate. **The normal build does not switch the analog rail**; rates alone do not demonstrate duty-cycle power savings. Do not enable the experimental gate until the prior direct `VCC_ANALOG`–3.3 V bridge, gate polarity, actual wiring, rail rise time and backfeed are verified.

Normal C6, S3 composite, macOS app and separate MOSFET build compile. Unit tests cover wake/release policy, idle transmit cadence, control packet validation and heartbeat expiration. The normal C6 and S3 builds were flashed and the mode transitions were bench-tested; the MOSFET build was not flashed. Existing MAC addresses in the lab code are board-specific; use separate configuration for other devices.

**Observed on attached boards:** with Studio running and keys released, C6 reported about 100 scans and 100 sensor submissions per second. After quitting Studio, C6 reported about 100 scans and 9–10 submissions per second. A partial press moved ADC channel 1 from about 1906 to 1507 and produced active-mode reports of roughly 963–984 scans per second and 955–982 submitted frames per second; missed ticks and radio contention mean the 1 kHz target is not guaranteed. After release the firmware returned to idle. Studio was reopened and app-present 100/100 behavior returned. C6 counters measure scheduling and submissions, not successful S3 receipt, USB HID polling or end-to-end latency.

A one-scan spike was observed while evaluating wake behavior. The C6 now checks candidate wake channels twice more within the same idle scan and uses the median of those three values, rather than waiting for the next 10 ms scan. This can reject a single anomalous read, but it does not prove that all noise, false wakes or missed brief motions are eliminated. Actual threshold-crossing-to-report latency and long-run idle power have not been measured. The startup released baseline still requires released keys; restarting while a key is held can prevent correct detection.

C6 timing experiment (3 Nov): reducing the mux settling wait from 20 to 10 µs produced approximately 1,000 scans/s in a short physical held-key test while the same test submitted and received about 900–910 wireless packets/s. The latest-sample queue replaced roughly 90–100 samples/s; those are not established radio losses. Idle returned to 100 scans/s with Studio connected. The 10 µs setting is experimental: moving adjacent channels, settling errors, noise and longer-term stability still require validation. It does not establish 1 kHz end-to-end delivery.

## Layer button bench link

XIAO C6 D7/GPIO17 is an active-low mechanical input with an internal pull-up; the other switch contact is GND. A debounced press cycles the volatile C6 layer 1–4. WHT2 status bits carry the current layer and button state; the S3 sends `L,<0..3>,<pressed>` on change and periodically, and the Mac test server exposes it as `layer_status`. Studio follows physical layer changes while retaining the ability to edit another layer locally. The input, RF and UI update were tested with the connected boards. This does not implement persistent profiles, per-layer device HID mappings, physical layer LEDs, or the planned hold+encoder gesture. C6/S3 need this updated protocol together; older WHT2 decoders reject the newly used status bits. A short initial press test appeared to advance two layers; a subsequent held press advanced one, so bounce and rapid repeated presses still need further testing.

## Live Studio lighting experiment

Studio sends all four lighting profiles to its localhost bridge, which repeats them via S3 ESP-NOW broadcast. C6 selects the physical layer and updates its LED engine without an ACK; colors, speed and brightness are volatile, and the last received profile remains only until reset. Host-reference time is refreshed to align continuously animated phases approximately. C6 uses its local raw sensor baseline for reactive events while Studio uses its own calibration; event timing/color counts can diverge. Actual LED intensity, radio jitter, startup, missed updates, physical power and LED/sensor interference require more hardware validation. The bench accepts commands only from the fixed test S3 MAC, with CRC for corruption detection; this is not authenticated encryption. USB lighting commands do not write NVS or enable keyboard HID.

Studio interactive synchronization: the layer picker and onscreen layer button send an idempotent layer request through S3; the server repeats it for up to three seconds until the reported device layer matches. Physical layer changes are reported back to Studio. An onscreen key click in live/lighting preview sends a repeated, deduplicated test hit to C6; this is a live preview, not a saved macro or HID action. The physical layer button's held state animates the 2D/3D model. Animation durations use double precision on C6 to avoid accumulated phase drift against Studio's reference clock. End-to-end controls were hardware-tested; per-pixel appearance, wireless timing, real key-event hue history, power consumption and behavior through packet loss remain imperfect and need further validation. Control messages are source-MAC-filtered CRC-protected test broadcasts, not authenticated or encrypted; sensor traffic continues without ACKs.

4 Nov active/idle adjustment on physical test setup: RGB animation caused raw ADC noise to repeatedly reset the release timer and falsely wake the board. The experimental MOSFET build uses a 32-sample release-decision filter and provisional 60-count wake / 30-count release thresholds; raw ADC reports and idle wake samples are not filtered by this filter. A brief hardware test observed 15 seconds idle at 100 Hz, a held light press at approximately 999 scans/s, then 12 seconds back at 100 Hz. The observed press moved about 500 ADC counts; the minimum detectable motion, behavior across all keys/RGB effects, sleep power, backfeed and long-duration stability remain unverified. These thresholds trade sensitivity for noise tolerance and are not a calibrated physical travel measurement.

## Persistent layered USB keyboard test (4 Nov)

The S3 stores four independent keyboard maps in versioned NVS storage. Studio's **Write** action validates the selected layer and transfers only that layer through acknowledged chunks; CRC and validation are checked before committing, preserving other layers if the transfer fails. Assigned keys require released and fully pressed calibration for their mapped ADC channel. Explicit HID keyboard usages, modifiers, traditional actuation/hysteresis and two-stage keyboard assignments are supported. Unsupported descriptive actions, SOCD and Rapid Trigger are rejected rather than silently compiled. The HID engine sends a neutral keyboard report before loading a changed layer and times out stale sensor input. The saved map runs while Studio's CDC connection is closed.

Hardware tests used temporary raw channel 1 mappings: Layer 1 typed H, Layer 2 typed J; the J mapping survived S3 USB unplug/replug and typed while Studio was quit. Rewriting only Layer 2 retained Layer 1. These temporary mappings were cleared after the test; the user's existing Studio layers were not overwritten. HID profile and engine tests cover modifiers, two stages, invalid records, and a layer change, but modifiers/two-stage behavior has not yet been physically verified. Output is keyboard 6KRO, not NKRO. Consumer HID now supports play/pause, previous/next, volume, mute, display brightness, stop, fast-forward and rewind. Volume down was physically confirmed on macOS; the other media usages depend on host support. Actual encoder output, Rapid Trigger, SOCD, analog/gamepad mapping, authenticated pairing and device configuration readback are not implemented by this path. Lighting synchronization is live and separate from the persistent USB keyboard map.

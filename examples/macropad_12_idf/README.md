> **Güncel durum — 5 Kasım 2026:** Bu dosyanın aşağıdaki tarihli test notları kendi dönemine aittir. Projenin güncel C6/S3, BLE, HID ve doğrulama durumu için [ana README’ye](../../Readme.md) bakın.

# Macropad USB test platformu

> **Güncel not — 2 Kasım 2026:** Native Studio'da bazı klavye ve medya görevleri için isteğe bağlı Mac çıktısı ve CrossOver için ayrı analog test köprüsü vardır; bunlar cihaza ayar yazmaz. Güncel kapsam [Studio README](mac_app/README.md), tüm proje ise [ana README](../../Readme.md) içindedir.

Bu bağımsız ESP-IDF örneği, gerçek cihazın C6 göndericisi veya S3 alıcısı değildir. Test için klasik ESP32 DevKit kullanır; radyo, ESP-NOW, USB HID ve üretim modları kullanılmaz. Mac üzerinde `server.py` seri portu okur ve yalnızca `127.0.0.1:8765` adresindeki arayüze aktarır.

## Bağlantılar

| Sinyal | Test ESP32 GPIO |
|---|---:|
| MCP3202 SCK / MISO / MOSI / CS | 18 / 19 / 23 / 21 |
| CD74HC4067 S0 / S1 / S2 / S3 | 25 / 26 / 32 / 33 |

MCP3202 CH0, multiplexer COM'a bağlıdır; besleme 3,3 V, tüm GND uçları ortak olmalıdır. ANA_KEY_0..11 numaraları şemada sütun sütun ilerler: satırlar `0 4 8`, `1 5 9`, `2 6 10`, `3 7 11`.

## Başlatma

ESP-IDF v6.1 ile test yazılımını oluşturmak için `idf.py set-target esp32 build` çalıştırın. Karta yazmadan önce fiziksel bağlantıları ve portu kontrol edin; kullanıcı istediğinde `idf.py -p PORT flash` kullanın.

Mac'te Python ve `pyserial` bulunan bir ortamda `python server.py` çalıştırıp `http://127.0.0.1:8765/` adresini açın. İstenirse `--port /dev/cu.usbserial-...` ile doğru portu seçin. Seri portu aynı anda başka bir uygulama kullanamaz. Sunucu kapanana kadar çalışır; Ctrl+C ile durdurulur.

## Ölçüm ve kayıt

Native uygulamanın Canlı test sayfasında sensör portu açıkça seçilebilir; seçim sonraki uygulama açılışında `--port` olarak sunucuya aktarılır. Varsayılan eski otomatik seçim korunur ve birden fazla kartta yanlış portu seçebilir. S3 portu ayrıca seçilebilir; istenirse sensör kaynağı olarak atanır ve uygulama yeniden açıldığında o port okunur. Salt S3 seçimi portu açmaz veya S3 ile iletişimin kurulduğunu göstermez. HID modundaki bir S3 hiç seri port göstermeyebilir. `/state` içindeki isteğe bağlı `port` alanı geçerli sensör verisinin geldiği portu belirtir; eski istemciler bu alanı yok sayabilir.

Paket `K,sıra,kanal0,...,kanal11` biçimindedir; eski sıra numarasız test yazılımı da desteklenir. Her oturum `logs/` altında JSONL dosyası olarak saklanır. Özet, geçerli / bozuk paketleri ve sıra numarası bulunan akışta tespit edilen boşlukları içerir; bu, sensörlerin elektriksel olarak doğru çalıştığını doğrulamaz. Sıra numarası olmayan eski sürümde paket kaybı bilinemez.

Kalibrasyonlar yalnızca tarayıcıda saklanır ve gerçek hareketin mm cinsinden ölçümü değildir. Layer düğmesi, layer LED'leri, encoder, dijital algılama, RGB ve pil için bu test protokolünde veri yoktur. Üretim mimarisi ve farklı pinleri için `../../Firmware/macropad-sender` ve `../../Firmware/dongle-receiver` kaynaklarına bakın; burada bulunan test GPIO atamalarını C6 pinleriyle karıştırmayın.

## Wireless HE Studio · macOS

Uygulamanın güncel özellikleri, sınırları, veri akışı ve geliştirme notları için [Wireless HE Studio README](mac_app/README.md) dosyasına bakın.

`mac_app/Wireless HE Studio.app` bu Mac'te derlenmiş, SwiftUI ile hazırlanmış, yerel USB test sunucusunu da başlatan bir uygulamadır. macOS 26 ve üzerinde Liquid Glass kullanır. Finder'da çift tıklayarak açılır; kaynak klasörü ile ESP-IDF Python ortamının yerlerinde bulunması gerekir. Uygulama macOS ARM64 üzerinde derlenmiştir; dağıtım için imzalanmış veya bağımsız bir paket değildir. Kaynak ya da Python yolu değişirse `mac_app/build_app.sh` çalıştırarak yeniden derleyin.

Uygulama `127.0.0.1:18768` üzerinde kendi yerel sunucusunu açar ve kapanırken kapatır. Aynı seri portu kullanan eski test sunucusunu önce kapatın. Studio içindeki ayarlar, tuş eşlemeleri ve animasyonlar sadece demo taslağıdır; cihaza yazılmaz. Test ekranının kalibrasyonu ile demo taslağı yerel uygulama tercihlerinde saklanır; eski web sürümündeki kalibrasyon veya demo verileriyle otomatik paylaşılmaz.

### 3B model

Canlı test sayfasındaki 3B model, kullanıcının Blender dosyasının birinci karesindeki birleşik modelden üretilir. Fareyle sürükleme döndürür, kaydırma yakınlaştırır; bir tuş kapağına tıklamak ilgili analog kanalı seçer. Kalibre edilmiş canlı sensör verisi varsa tuş kapağı yaklaşık hareket ile gösterilir; bu animasyon gerçek tuş yolu ölçümü değildir. 2B görünüm seçeneği korunur. Blender kaynak dosyası değiştirilmez. USD aktarımında malzemeler birebir taşınamadığından kasa ve tuş kapaklarına uygulamada basitleştirilmiş malzemeler uygulanır. Kaynak model `mac_app/assets/macropad.usdz` içinde tutulur ve derleme sırasında uygulamaya kopyalanır.

3B görünümde **Simülasyon** açılarak seçili tuşun hareketi kaydırıcı veya Bas/Bırak ile denenebilir; bu değer gerçek ADC ve günlükleri etkilemez. **Işık önizleme**, aktif demo layer'ının aydınlatma tercihlerini görsel olarak kullanır; 3B ışıklı çizgiler gerçek LED geometrisinin ölçümü değildir. Klavye sabittir, kamera zeminin altına inmez ve yakınlaştırma aralığı sınırlandırılmıştır.

3B ve 2B ışık önizlemeleri tuş çevresindeki renkli göstergeyi kullanır; ışın izleme kullanılmaz. Bu efektler fiziksel LED şiddetini veya malzemenin gerçek geçirgenliğini ölçmez.

> **Güncelleme 6 Kasım 2026:** Aşağıdaki eski durum notları tarihsel olabilir. C6 0.1.5 / Studio 0.1.2 ile şifreli BLE ayar servisi, canlı katman ve aydınlatma ayarı, tek katman HID profilinin C6 NVS belleğine doğrulanarak yazılması eklendi. Mac üzerinde iki layer aydınlatması ve H atamasının C6 yeniden başladıktan sonra yazması kullanıcı tarafından doğrulandı. Studio kapalıyken C6 yeniden başlatıldığında sabit ışık renginin korunduğu kullanıcı tarafından doğrulandı. BLE canlı sensör telemetrisi yok. S3 ve C6 HID profilleri ayrı; ESP-NOW dönüşünde S3 profili C6 profilinin üstüne yazılabilir. Üç yuvanın adları yalnızca Mac'te düzenlenen takma adlardır, eşleşmiş cihazın gerçek adı otomatik okunmaz. Diğer cihazlar ve kapsamlı yeniden bağlantı testleri bekliyor.

> Kod: `examples/c6_s3_sensor_bridge/c6/main/ble_studio_gatt.c`, `shared/ble_layer_staging.h`, `examples/macropad_12_idf/mac_app/BLEStudio.swift`. Yalnızca ilgili yuvanın eşleşmesini silme kullanıcının açık isteğiyle bir defa yapıldı; geçici silme kodu son sürümde bulunmuyor.

# Geliştirici / yapay zekâ devir notları

Son doğrulama: 5 Kasım 2026. Ürüne ve güncel test sonuçlarına genel bakış için
[Proje_Rehberi.md](Proje_Rehberi.md); bağımsız sürümler için
[releases/CHANGELOG.md](../releases/CHANGELOG.md). Bu belge çalışmaya tam
olarak devam etmek isteyen kişi içindir. Kod ve gerçek cihaz durumu her zaman
son doğruluk kaynağıdır.

## Korunacak mevcut sistem

- Test kartı XIAO ESP32-C6 ve ESP32-S3 dongle. Kullanıcı BLE ile Mac'e yazmayı,
  Layer + KEY 0 ile mod geçişini, geçiş ışığını ve BLE bağlantısız turuncu
  uyarıyı denedi. RGB yumuşatma sonrasında turuncu efekt beğenildi.
- C6 ve S3'e bu depodaki `examples/c6_s3_sensor_bridge` projelerinden yazılım
  yüklendi. Kartın içeriği sonradan değişmiş olabilir; USB kimliği, bağlantı
  ve çalışan sürümü kontrol etmeden varsaymayın.
- C6 `sdkconfig.ble`: IDF 6.1, ESP32-C6, BLE NimBLE, NVS bonding, 8192 byte
  ana görev yığını, large single-app bölümü, MOSFET GPIO23, boşta duty cycle,
  600 µs bekleme, wake/release 60/30. Son fiziksel yüklemede derleme dizini `examples/c6_s3_sensor_bridge/build-ble` idi; normal komut C6 dizininden çalıştırılırsa `c6/build-ble` kullanılır. Her iki `build-ble` üretilen dosyadır, kaynak
  değildir. S3 composite ESP-IDF projesi USB HID + seri arayüzdür. Son C6 fiziksel yüklemesi `examples/c6_s3_sensor_bridge/build-ble` dizininden yapılmıştır; C6 klasörü içinden derleme yapınca `c6/build-ble` oluşur.
- Bluetooth bonding için NVS kalıcılığı açıldı ve kullanıcı tekrar eşleşti.
  Güç kesme, tekrar açma ve ESP-NOW'a geçip dönme sonrası otomatik bağlanma
  **henüz kanıtlanmadı**. Önce bunu dene; kullanıcının Mac'indeki eşleşmeyi
  sebepsiz silme. BLE Just Works MITM doğrulaması sağlamaz.
- ESP-NOW paketleri şifresiz broadcast'tir. S3 profil yayınındaki CRC ve MAC
  kontrolü kimlik doğrulama değildir. Uçtan uca güvenli cihaz eşleşmesi eksik.
- `backups/ble-test/` yerel flaş yedekleri bonding anahtarı ve kişisel cihaz
  bilgileri içerebilir. Git'e ekleme; paylaşma. Tam yedeği körlemesine geri
  yazmak güncel kullanıcı kayıtlarını siler. Önce mevcut durumu yedekle.

## Kaynak haritası

| İş | Dosyalar |
|---|---|
| C6 okuma, güç, layer, LED, radyo | `examples/c6_s3_sensor_bridge/c6/main/main.c` |
| C6 BLE, keşif, pairing, HID | `examples/c6_s3_sensor_bridge/c6/main/ble_transport.c` |
| Tuş kombinasyonu, paketler, güç | `examples/c6_s3_sensor_bridge/shared/` |
| S3 profil, telemetri, HID | `examples/c6_s3_sensor_bridge/s3_composite/main/main.c`, `hid_engine.c`, `hid_profile.c`, `usb_composite.*` |
| RGB motoru | `examples/lighting_espnow_lab/components/lighting_core/` |
| Mac uygulaması | `examples/macropad_12_idf/mac_app/Native.swift`, `Model3D.swift`, `build_app.sh` |
| Mac seri köprüsü | `examples/macropad_12_idf/server.py` |
| Durum görselleri | `tools/progress.py`, `tools/progress_grid.py`, `tools/progress_english.py` |

Layer verisi S3 NVS'de saklanır ve parçalarla C6'ya aktarılır; cihaz profili,
Mac yerel taslakları ve aydınlatmanın canlı durumu ayrı kavramlardır. BLE C6
üzerindeki profil kopyasını kullanır. Mod tuşu C6 kodunda fiziksel ADC 1'e
sabitlenmiştir; uygulama kanal eşlemesiyle otomatik değişmez.

## Derleme ve test

C6 derlemesinden önce `source ~/.espressif/v6.1/esp-idf/export.sh`, ardından
`cd examples/c6_s3_sensor_bridge/c6` ve
`idf.py -B build-ble -D SDKCONFIG=sdkconfig.ble build`. S3 için
`cd examples/c6_s3_sensor_bridge/s3_composite` ve `idf.py build`.
Kartların portunu ve yedeğini kontrol etmeden `flash` çalıştırma. `idf.py`
bölüm tablosunu da yazabilir; NVS ve flash adreslerini önce karşılaştır.

S3/C6 ortak C testleri `examples/c6_s3_sensor_bridge/tests/` altındadır;
`test_transport_mode.c`, `test_ble_hid_report.c`,
`test_ble_profile_staging.c`, `test_ble_profile_wire.c` ve
`test_ble_profile_integration.c` bağımsız host C testleridir. HID engine ve
profile kaynaklarıyla derlenir. Build ve unit test donanım davranışını kanıtlamaz.

Studio `examples/macropad_12_idf/mac_app/build_app.sh` ile yerelde derlenir.
Bu script uygulamayı silip yeniden oluşturur ve Info.plist'e yerel Python ile
proje yollarını yazar. Bu nedenle üretilen uygulama başka Mac'e otomatik
kurulabilir sayılmaz. Kullanıcının açık uygulamasını kapatmadan veya yerel
verilerini silmeden çalış; ayarlar ve kullanıcı profillerini koru.

## C6 0.1.1 Easy Switch denemesi

Layer + en alt sol/orta/sağ tuşlar BLE yuvası 1/2/3 için kodlandı ve derlendi.
Boş yuva seçilince eşleştirme penceresi açılır. Daha önce tek bir BLE bağı varsa
ilk yuvaya taşınması hedeflenir; bonding verileri silinmez. Kartta sürüm yüklü,
bir kez şifreli bağlantı gözlendi; üç cihaz arasında fiziksel geçiş, bağlantı
kararlılığı ve NVS sonrası yeniden başlatma henüz test edilmedi. Slot kodu
`c6/main/ble_transport.c`, kombinasyon `shared/ble_slots.h` ve
`tests/test_ble_slots.c` dosyalarındadır.

## Öncelikli sonraki testler

1. Tüm tuşlar serbestken C6'yı yeniden başlat. Mevcut BLE eşleşmesi korunuyor
   mu ve kullanıcı cihazı unutmadan otomatik geri bağlanıyor mu?
2. ESP-NOW'a geç, USB HID'i doğrula; BLE'ye dön, yeniden eşleşme istemeden
   klavye çıktısını kontrol et. Mod geçişi ve LED renklerini izle.
3. BLE bağlantısı kesilirken basılı tuş takılı kalıyor mu? Layer, medya,
   RT ve SOCD davranışlarını BLE'de ayrı ayrı doğrula.
4. 3 saniyelik keşif modunun diğer tuşa basınca iptalini, pencere süresini ve
   ikinci bir cihazda davranışını kontrol et. Keşif görünürlüğü kısıtlaması
   tek başına güvenli kimlik doğrulama değildir.
5. ADC gürültüsü, güç rail yerleşmesi, MOSFET ters beslemesi ve gerçek akımı
   ölç; yazılım duty oranını pil ömrü olarak sunma.

## Sürümleme

Üç bağımsız bileşenin sürümleri `releases/versions.json`, geçmişi
`releases/CHANGELOG.md` dosyasındadır. Git commit'leri eski kodu korur;
GitHub ön sürümleri `c6/vX.Y.Z`, `s3/vX.Y.Z` ve `studio/vX.Y.Z` etiketlerini
kullanır. Kod değişikliğinde ilgili bileşenin sürümünü artır, değişikliği ve
kanıtını not et, test et ve yeni commit ile yayınla. Yayımlanmış etiketi veya
release'i değiştirme. Başlangıç 0.1.0 ürünün bitmiş olduğunu göstermez.

## Studio 0.1.1 bağlantı durumu

Studio, `IOHIDDevice` sistem kayıtlarında ürün adı Wireless HE ve Bluetooth taşıması olan cihazı gözlemler; giriş olaylarını okumaz. Bu yalnızca macOS'un HID aygıtı gördüğünü belirtir, gerçek sensör akışını kanıtlamaz. Canlı sensör ve ayar yazma protokolü hâlâ S3 USB yolu üzerindedir. Bu not 0.1.1 içindir; 0.1.2 ile BLE yapılandırma servisi eklendi, ancak BLE sensör telemetrisi hâlâ yoktur.

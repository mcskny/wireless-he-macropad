# Wireless HE — cihaz mimarisi ve entegrasyon durumu

Bu belge hedef C6/S3 için çalışma tasarımıdır; cihazın çalıştığını veya fiziksel test yapıldığını iddia etmez. `examples/macropad_12_idf` klasik ESP32 sensör test platformu ayrı kalır. Test kartının `K,...` akışı, pinleri, seri sunucusu ve kalibrasyon kayıtları bu değişikliklerle değiştirilmez.

## Mevcut yapı

- C6: MCP3202/CD74HC4067, shift register, encoder, güç, ışık ve 5/23 baytlık ESP-NOW tuş paketleri.
- S3: ESP-NOW alıcısı, Hall işleme, dört layer, 6KRO klavye, Consumer HID, iki eksenli gamepad, temel makro ve ayrı AP yapılandırması.
- Mac Studio: USB sensör testi; cihaz yapılandırması ve gerçek layer iletişimi henüz bağlı değildir.
- `examples/lighting_espnow_lab`: bağımsız deney; gerçek ana firmware'in radyo yöneticisine henüz entegre değildir.

## Bu çalışmada hazır olan parçalar

1. C6 ana firmware'inde mux COM bağlantısının MCP3202 kanalı `CONFIG_MACROPAD_ADC_CHANNEL` ile seçilir; paylaşılan şemadaki gibi varsayılan CH0'dır. Sensör ve pil mux okumaları aynı kanalı kullanır. Gerçek PCB, pil bölücü ve kanal bağlantısı ölçülmeden donanım doğrulanmış sayılmaz.
2. S3 6KRO işleme 12 fiziksel tuşun tamamını tarar: 6 normal tuştan sonra gelen modifier/consumer kodları artık sırf liste dolduğu için göz ardı edilmez. Bu değişiklik NKRO yapmaz. Değişmeyen klavye raporu tekrar gönderilerek bekleyen medya raporunun USB uç noktasında aç kalması önlenir. Watchdog klavye ve gamepad bırakma raporları USB meşgulse sonraki denemeye kadar önceki durumu korur; böylece başarısız gönderim başarılı sanılmaz.
3. `shared/encoder_controller/` platformdan bağımsız durum makinesidir. Kısa layer basışı bir sonraki katman; uzun basış ilk katman; layer basılıyken encoder çevirme/basma layer basışını tüketir. Encoder basışındaki alternatif durum bırakılana kadar saklanır. Bağlantı kopunca girişler serbest kalana kadar yeniden senkronize olur ve sahte katman geçişi oluşturmaz. Girişler önceden debounce edilmiş olmalıdır. **Henüz C6/S3 ana döngüsüne veya HID çıkışına bağlanmadı.**
4. `shared/config_protocol/` USB için aday, sınır kontrollü sürümlü çerçeve kodudur; mesaj kimliği ve CRC32 taşır. Transporta, yapılandırma içeriğine veya kalıcı kayıt işlemine bağlı değildir; CRC kimlik doğrulama sağlamaz.
5. `tools/validate_studio_config.py`, Studio tam yedeğini veya yapılandırma JSON'unu yalnızca okur; yapılandırılmış HID tuşlarını ve açıklamayla uyumsuz/eski kayıtları raporlar. Bir cihaz yapılandırması üretmez, seri port açmaz, veri göndermez.
6. `shared/config_protocol/config_stream.c`, aday WHEC çerçevelerini bayt akışında sınırlandırılmış bellekte birleştirir; kısmi başlıkları, geçersiz uzunluğu ve CRC'si bozuk çerçeveleri işler. **USB taşımasına, kimlik doğrulamaya, ayarların uygulanmasına ve NVS'e henüz bağlı değildir.** Taşıyıcı, kesintide veya tamamlanmayan makul başlık için zaman aşımında okuyucuyu sıfırlamalıdır. Callback içindeki çerçeve verisi yalnızca callback süresince geçerlidir.

## Hedef bileşen sorumlulukları

- **C6:** sensörleri ve layer/encoder girişlerini okur; mevcut 5/23 baytlık veri yolunu sürdürür; NeoPixel motorunu yerelde çalıştırır. Işık yapılandırması doğrulanıp kalıcı yazılmadan başarılı olarak bildirilmez.
- **S3:** tek layer durumunun sahibi; Hall işleme, fiziksel girişler, katman/encoder kuralları, HID raporlarının tek zamanlayıcısı ve USB üzerinden yapılandırma uç noktası. USB raporları gönderim başarısı teyit edilmeden 'gönderildi' diye önbelleğe alınmaz; kesintide tuşlar/consumer/gamepad bırakılıncaya kadar tekrar denenir.
- **Mac:** sensör test bağlantısı ile gerçek cihaz taşıyıcısı açıkça ayrılır. Düzenleme Mac'te saklanır; `Yazdır` yalnızca kullanıcı bastığında, doğrulama ve cihazın kapasite kontrolünden sonra ilgili cihaza gönderir. İşlem kimliği ile ACK ve kaydedilmiş sürüm/geri okuma eşleşmeden başarılı gösterilmez.

## İletişim ve yazma kuralları — henüz uygulanmadı

- S3 USB yapılandırma kanalı ve HID aynı anda çalışmalı; ayrı sürümlü çerçeveler, uzunluk sınırı, isteğe bağlı alanların açık sürümü, işlem numarası ve hataya özgü cevap gerekir. İletişim açıkça cihaz kimliğine bağlanmalı; seri portun ilk eşleşeni otomatik seçilmemelidir.
- Düzenlenmiş tüm yapılandırma önce bellekte doğrulanır; mevcut etkin ayarlar kısmi paketlerle değiştirilmez. Başarısız doğrulama/eski sürüm/bilinmeyen tuş görevi sonucunda eski ayarlar korunur. Kalıcı kayıt tamamlanınca ACK, sonra doğrulama için cihazdan geri okuma yapılır.
- Studio'daki etiket, HID kodu değildir. `primaryKey`/`secondaryKey` yalnız görünür metinle eşleşiyorsa adaydır; eski serbest metinler tahmin edilip gönderilmez. Her görevin türü ve yetenek kontrolü, modifier+key, consumer, ikinci kademe, encoder ve desteklenmeyen host görevleri için ayrı olmalıdır. Mevcut S3 `uint16_t key_mappings` biçimi modifier+tuş birleşimini veya bütün Consumer aralığını güvenli ifade etmez; eski NVS blobuna doğrudan yeni alan eklemek yerine sürümlü, açık kodlanmış format ve migration şarttır.
- S3 → C6 kontrol iletişimi mevcut 5/23 bayt gerçek zamanlı paketlerle karışmamalı; ESP-NOW tek yöneticide tür/uzunluk/sürüm ile ayrılmalı. İstek/ACK, tekrar gönderimde idempotence, yeniden başlatma, timeout ve NVS atomikliği açıkça ele alınmalı. MAC adresi veya CRC kimlik doğrulama değildir. Eşleştirme, şifreli bağlantı ve tekrar saldırısı koruması tamamlanmadan güvenli sayılmaz.
- Layer'ın sahibi S3'tür: anlık değişiklik C6'ya teyitli bildirilir, dört LED ve Mac gerçek cihaz bilgisini gösterir; ACK gelmeyen istek gerçek durum gibi sunulmaz. Bağlantı kopuşu ve yeniden bağlantı açık durumlarla ele alınır.
- Mevcut C6 `SW2 + encoder` güç modunu değiştirir; yeni layer basılı encoder görevleriyle çakışır. Ayrıca S3 `SW2 + encoder basma` yapılandırma modunu anında açar. İkisi de yeni mantığa bağlanmadan önce yeniden tasarlanmalıdır. `shared/encoder_controller` tek başına bunları değiştirmez.
- `İleri/Geri`, parlaklık ve uygulama özelindeki görevlerin USB HID ve işletim sistemi desteği ayrı doğrulanır; standartta var olması her Mac/harici monitörde çalıştığı anlamına gelmez. Analog gamepad eksen ve 6KRO/NKRO yetenekleri de açık bildirilmelidir.

## Doğrulama

```sh
cc -std=c11 -Wall -Wextra -Werror -I Firmware/shared/encoder_controller \
  Firmware/shared/encoder_controller/encoder_controller.c \
  Firmware/shared/tests/test_encoder_controller.c -o /tmp/whe-encoder-test
/tmp/whe-encoder-test
cc -std=c11 -Wall -Wextra -Werror -I Firmware/shared/config_protocol \
  Firmware/shared/config_protocol/config_protocol.c \
  Firmware/shared/tests/test_config_protocol.c -o /tmp/whe-config-test
/tmp/whe-config-test
cc -std=c11 -Wall -Wextra -Werror -I Firmware/shared/config_protocol \
  Firmware/shared/config_protocol/config_protocol.c Firmware/shared/config_protocol/config_stream.c \
  Firmware/shared/tests/test_config_stream.c -o /tmp/whe-stream-test
/tmp/whe-stream-test
cc -std=c11 -Wno-unused-variable -Wno-unused-parameter \
  -I Firmware/shared/tests/hid_stubs \
  -I Firmware/dongle-receiver/components/usb_hid/include \
  -I Firmware/dongle-receiver/components/usb_hid \
  Firmware/shared/tests/test_usb_hid.c -o /tmp/whe-hid-test
/tmp/whe-hid-test
python3 -m unittest discover -s Firmware/shared/tests -p 'test_*.py'
python3 Firmware/tools/validate_studio_config.py /path/to/studio-backup.json
```

Son komut yalnız dosyayı okur. Donanım gerektirmeyen testler fiziksel pinleri, ADC kararlılığını, USB/RF zamanlamasını veya HID host davranışını doğrulamaz. Build çıktılarının hiçbirini karta yüklemeyin; gerçek bağlantı ve test adımları donanım mevcut olduğunda ayrı planlanır.

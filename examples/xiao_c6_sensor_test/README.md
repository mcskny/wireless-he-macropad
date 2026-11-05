> **Güncel durum — 5 Kasım 2026:** Bu dosyanın aşağıdaki tarihli test notları kendi dönemine aittir. Projenin güncel C6/S3, BLE, HID ve doğrulama durumu için [bütünleşik rehbere](../../Docs/Proje_Rehberi.md) bakın.

# XIAO ESP32-C6 - 12 kanallı USB sensör testi

Bu proje mevcut `examples/macropad_12_idf` testinin ayrı bir XIAO ESP32-C6 uyarlamasıdır. Mevcut DevKit projesi, Mac uygulaması ve üretim C6/S3 firmware'i değiştirilmez. ESP-NOW, encoder, fiziksel layer ve LED çıktısı bu örneğe dahil değildir.

## Bağlantılar

| Hat | XIAO etiketi | GPIO | Entegre |
|---|---|---:|---|
| ADC SCK | D8 | 19 | MCP3202 CLK, pin 7 |
| ADC MISO | D9 | 20 | MCP3202 DOUT, pin 6 |
| ADC MOSI | D10 | 18 | MCP3202 DIN, pin 5 |
| ADC CS | D4 | 22 | MCP3202 CS, pin 1 |
| MUX S0 | D0 | 0 | CD74HC4067 S0, pin 10 |
| MUX S1 | D1 | 1 | CD74HC4067 S1, pin 11 |
| MUX S2 | D2 | 2 | CD74HC4067 S2, pin 14 |
| MUX S3 | D3 | 21 | CD74HC4067 S3, pin 13 |
| 3,3 V | 3V3 | - | MCP3202 VDD/VREF, mux VCC ve Hall sensör VCC |
| GND | GND | - | MCP3202 VSS, mux GND ve sensör GND |

MUX COM MCP3202 CH0'a gider; mux aktif-düşük E ucu GND'ye bağlıdır. ADC ve sensör devresi 3,3 V çalışmalıdır; 5 V pinine bağlanmaz. Devre 3,3 V hattını harici besliyorsa XIAO'nun 3V3 çıkışıyla beslemeleri rastgele paralellemek yerine tek uygun besleme düzeni seçilmelidir. Elektriği kesip bağlantıları yapın.

## Derleme / yükleme

ESP-IDF 6.1 ile `idf.py build` başarılıdır. USB konsolu ESP32-C6 USB Serial/JTAG olarak ayarlanmıştır. 2 Kasım'da bu örnek bağlı XIAO C6'ya yüklendi; sensör sunucusunda 12 kanal akışı ve bir yaklaşık 31,7 saniyelik oturumda 3173 paket ve tespit edilen 0 sıra boşluğu görüldü. Bütün fiziksel tuşların doğruluğu veya uzun süreli kararlılık bu testle doğrulanmış değildir.

```bash
source "$HOME/.espressif/v6.1/esp-idf/export.sh"
cd examples/xiao_c6_sensor_test
idf.py build
ls /dev/cu.usbmodem*
idf.py -p /dev/cu.usbmodemPORT flash
```

`/dev/cu.usbmodemPORT` örnektir, gerçek portla değiştirilir. USB portu başka seri monitör ya da sunucu tarafından kullanılıyorsa önce kapatılır. Yükleme yalnız kullanıcı istediğinde yapılır; mevcut XIAO programını değiştirir. USB'den `K,sıra,kanal0,...,kanal11` satırları gönderilir; mevcut Mac uygulamasının seri ayrıştırıcısı bu biçimi destekler. ADC kanal sırası kart üzerindeki gerçek kablolara ve uygulamadaki kayıtlı eşlemeye bağlıdır, fiziksel tuşları yeniden doğrulayın.

Pin adları: [Seeed XIAO ESP32C6 pin haritası](https://wiki.seeedstudio.com/xiao_esp32c6_getting_started/).

> **Güncel durum — 5 Kasım 2026:** Bu dosyanın aşağıdaki tarihli test notları kendi dönemine aittir. Projenin güncel C6/S3, BLE, HID ve doğrulama durumu için [bütünleşik rehbere](../../Docs/Proje_Rehberi.md) bakın.

# PEC11R ESP32 test aracı

Klasik ESP32 DevKit (ESP32-WROOM) için ESP-IDF projesi. ESP32 kendi Wi-Fi ağını açar: **ESP32-Encoder**, parola **encoder123**, adres **http://192.168.4.1**. İnternet bağlantısı gerekmez.

## Bağlantılar

Bağlantıları USB gücünü keserek yapın. Ekran görüntüsündeki mevcut direnç ve kondansatörlerin zaten lehimli olduğu varsayılır:

| Şemadaki ağ | ESP32 DevKit |
|---|---|
| ENC_A_f (R43 ile C9 birleşimi) | GPIO25 |
| ENC_B_f (R42 ile C10 birleşimi) | GPIO26 |
| ENC_BTN (J3 pin 1) | GPIO27 |
| batt− (J3 pin 4 ve 5, C9/C10'un diğer uçları) | GND |
| 3.3v (R23, R24, R25'in diğer uçları) | 3V3 |

```text
ESP32 3V3 ──┬── 10k ── ENC_A ── 10k ──┬── ENC_A_f ── GPIO25
            │                           └── 10nF ── GND
            ├── 10k ── ENC_B ── 10k ──┬── ENC_B_f ── GPIO26
            │                           └── 10nF ── GND
            └── 47k ── ENC_BTN ─────────────────── GPIO27
                       
Enkoder: A = ENC_A, B = ENC_B, ortak uç = GND
Düğme: bir uç ENC_BTN, diğer uç GND
ESP32 GND ── batt− ── kondansatörlerin GND uçları ve enkoder ortak uçları
```

J3 numaraları ekran görüntüsündeki bağlantı numaralarıdır; enkoderin fiziksel bacak numaralarıyla karıştırmayın. 3V3'e 5V bağlamayın; harici batarya bağlıysa besleme düzenini kontrol edin, aynı anda gelişigüzel iki kaynağı bağlamayın. Fiziksel pinleri ölçerek doğrulayın.

## Derleme ve yükleme

ESP-IDF ortamını etkinleştirdikten sonra bu klasörde `idf.py build` ve `idf.py -p /dev/cu.USB_PORT flash monitor` çalıştırın. Gerçek seri port adını kullanın. Bu işlem kartın uygulama yazılımını değiştirir.

Sayım dört geçişte bir artar veya azalır; açılıştan itibaren göreli konumdur. A ve B yer değiştirirse yön tersine döner. Test sırasında yavaş çevirin. Sayı veya düğme kararsızsa ortak GND, filtreli uçlar, lehimler ve pinleri kontrol edin. Düğme yazılımla yaklaşık 20 ms süzülür; geçersiz geçiş sayısı gürültüyü veya kaçırılan geçişleri gösterebilir.

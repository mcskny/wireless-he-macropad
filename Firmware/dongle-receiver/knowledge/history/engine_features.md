# ESP32-S3 Dongle - Engine Features History

Bu dosya projeye eklenen tum yeni ozelliklerin ve sistemsel gelismelerin
kronolojik kaydidir. AI_GUIDELINES.md kural 3'e gore ZORUNLU tutulur.

## 2026-07-23 - F0: Proje Iskeleti

### Eklenen ozellikler

- **AI_GUIDELINES.md**: C6'dan kopyalandi + S3'e ozel bolumler eklendi:
  - #21 TinyUSB & USB HID (composite NKRO + Consumer + Gamepad, 1000Hz)
  - #22 USB-OTG (S3'te var, C6'da yok)
  - #23 Wi-Fi AP & WebSocket (CONFIG_AP_MODE, esp_http_server httpd_ws)
  - #24 ESP-NOW RX (C6 paket alici, MAC/size filter, seq/watchdog)
  - #25 Hall Effect Motoru (normalizasyon + RT + Snap Tap + Joystick)

- **esp32_s3algoritma.md**: C6 klasorunden S3 klasorune tasiindi.

- **ESP-IDF proje iskeleti kuruldu**:
  - `CMakeLists.txt` (project root, esp32s3_dongle)
  - `sdkconfig.defaults`: ESP32-S3 target, UART0 log (GPIO43/44), NVS, Wi-Fi,
    PSRAM (Xiao S3R8 8MB), tickless idle kapali (USB powered).
  - `main/CMakeLists.txt`: 11 bileseni REQUIRES listesinde bagli.
  - `main/Kconfig.projbuild`: C6 MAC, ESP-NOW kanali, watchdog timeout (1000ms),
    USB poll interval (1ms), NKRO max keys, Wi-Fi AP SSID/password/kanal,
    Hall histerezis/RT sinirlar, loop rate (1000Hz).
  - `main/include/pinout.h`: MINIMAL - dongle'da harici pin YOK (kullanici onayi).
    Sadece USB donanim (IO19/IO20 D+/D-) + status LED stub (-1).
  - `components/system_state/include/system_state.h`: S3 tipleri:
    s3_mode_t (NORMAL/CONFIG_AP), calibration_config_t (min/max_adc, actuation,
    RT sensitivity, key_mappings[4][12], snap_tap, joystick), key_state_t
    (position, pressed, highest/lowest_pos), s3_system_state_t (global).
  - `main/main.c`: F0 minimal app_main (baslangic logu + tip derleme dogrulama).

- **11 bilesen iskeleti** (components/ altinda): system_state (gercek tipler),
  nvs_store, espnow_rx, packet_decoder, hall_effect_engine, usb_hid,
  macro_engine, layer_manager, config_ap, watchdog, usb_power.
  Her biri CMakeLists.txt + include/<name>.h + <name>.c (yer tutucu).

### Alinan mimari kararlar (F0)

| Karar | Sonuc |
|---|---|
| Klasor: esp32s3algoritma/ | C6'dan ayri, paralel. Tum S3 dosyalari burada. |
| Modul: Xiao ESP32-S3 | USB-OTG destekli, TinyUSB NKRO HID icin. |
| Harici pin YOK | pinout.h minimal. Sadece USB + RF. |
| KATMAN_SAYISI = 4 | C6'da 4 layer LED'i var. key_mappings[4][12]. |
| Kalibrasyon: NVS blob | Tek key'de struct blob. Default + web kalibrasyon. |
| Config AP: WebSocket | Gerçek zamanli ADC akisi icin (esp_http_server httpd_ws). |
| Log: UART0 (GPIO43/44) | TinyUSB USB-OTG ile USB-Serial/JTAG cakisir. |
| PSRAM: 8MB (S3R8) | Xiao ESP32-S3. WebSocket + kalibrasyon icin. |
| Sleep: tickless idle kapali | S3 USB powered, power yonetimi gerekmez. |

### C6 ile paket format uyumu

S3, C6'nin gonderdigi paketleri alir (AYNI spec, ters yon):
- 5B: [u8 counter][u8 status][u16 LE buttons][i8 enc_delta]
- 23B: 5B + 18B (12x12-bit LE bit-packing, mux adres sirasiyla)
- status: bit0-1 mode, bit2 SW2, bit3 ENC_BTN, bit4 flag_15, bit5 battery_dead,
  bit6 charging, bit7 charge_complete

packet_decoder, C6 packet encoder'inin TERSI: bit-unpacking + status parse.

### Bekleyen sorunlar

- TinyUSB component secimi (esp_tinyusb vs tinyusb) F3'te belirlenecek.
- USB-OTG ile USB-Serial/JTAG cakismasi: programlama USB-Serial/JTAG (bootloader),
  runtime TinyUSB. Test gerekli.
- Xiao ESP32-S3 PSRAM boyutu (S3R2 2MB mi S3R8 8MB mi) dogrulanmali.

## 2026-07-23 - F1-F5: Tam S3 Dongle Implementasyonu

### F1: nvs_store + espnow_rx + packet_decoder + watchdog

- **nvs_store**: Kalibrasyon blob (calibration_config_t tek NVS key). Default degerler
  (min=0, max=4095, actuation=0.5, RT=0.1, key_mappings katman 0 = A-L). load/save/reset.
- **espnow_rx**: C6 paket alici. MAC + size filter (5/23B). FreeRTOS queue (depth 8).
  RX callback wifi task'te, queue kendi kilidine sahip. esp_now_recv_info_t (v6).
- **packet_decoder**: 5B/23B C6 paket cozme. bit-unpacking (12×12-bit LE, C6 encoder
  tersi). packet_parse_status: power_mode/SW2/ENC_BTN/flag_15/battery_dead/charging.
- **watchdog**: C6 baglanti 1000ms. feed() RX'ten, is_expired() main'den. portMUX.

### F2: hall_effect_engine (Bölüm 4)

- **hall_normalize**: (adc-min)/(max-min) → [0.0, 1.0], clamp.
- **hall_process_traditional**: actuation + histerezis (bas/birak esikleri).
- **hall_process_rapid**: RT dinamik ust/dip takip (highest/lowest_pos, Sp/Sr).
- **hall_resolve_snap_tap**: SOCD zıt çiftler, son basilan aktif.
- **hall_compute_joystick**: bipolar (V_pos - V_neg) × 32767, X/Y 16-bit.
- **hall_engine_release_all**: watchdog timeout icin tum tuş birakma.

### F3: usb_hid (STUB)

- **STUB**: TinyUSB managed component (espressif/esp_tinyusb) eklenmedi.
  usb_hid_init/send_keyboard/send_gamepad/send_consumer/release_all log-only.
  Rapor degisim kontrolu (sadece degisimde gonder, Bölüm 6.1).
  @note TinyUSB eklenince gercek descriptor + tud_hid_report implementasyonu.

### F4: layer_manager + macro_engine (Bölüm 5)

- **layer_manager**: SW2 basili → layer 1, birak → layer 0. 4 katman key_mappings.
  get_keycode(layer, key_index, key_mappings).
- **macro_engine**: Asenkron non-blocking. macro_step_t (keycode + delay_ms).
  start/tick/is_running/stop. tick her ms'de, sure dolunca siradaki adim.
  @note Makro NVS yukleme + TinyUSB entegrasyonu sonraya.

### F5: config_ap (STUB) + usb_power + main.c state machine (Bölüm 2)

- **config_ap (STUB)**: Wi-Fi AP baslatir (WIFI_MODE_AP). WebSocket handler v6 API
  degisikligi (httpd_ws_frame_t yok) nedeniyle STUB. send_adc/exit_requested false.
  @note WebSocket v6 imzasi kontrol edilip implemente edilecek.
- **usb_power**: USB suspend/resume flag + log (Bölüm 6.3). TinyUSB callback sonraya.
- **main.c Bölüm 2 state machine**: NORMAL_MODE (ESP-NOW RX + Hall + USB HID 1000Hz)
  ↔ CONFIG_AP_MODE (Wi-Fi AP). Config request: SW2+ENC_BTN. Seq kayip takibi.
  Watchdog timeout → release_all. Macro tick. USB suspend check.

### Alinan mimari kararlar (F1-F5)

| Karar | Sonuc |
|---|---|
| power_mode_t S3 system_state'e eklendi | C6 status decode icin (S3'in kendi guc modu yok). |
| Kalibrasyon NVS blob | Tek key, struct serialize. Default + web. |
| espnow_rx FreeRTOS queue | Callback (wifi task) → main, queue kilidi yeterli. |
| packet_decoder bit-by-bit | C6 encoder tersi, basit + dogru. |
| usb_hid STUB | TinyUSB managed component sonraya. |
| config_ap STUB | WebSocket v6 API degisti, sonraya. |
| macro_engine log-only | TinyUSB + NVS makro sonraya. |
| Config mode: SW2+ENC_BTN | Placeholder, gercek Config_Mode_Request biti sonraya. |

### Bekleyen sorunlar (F3/F5 stub'lar)

1. **TinyUSB (usb_hid)**: esp_tinyusb managed component eklenip gercek NKRO +
   Consumer + Gamepad descriptor yazilmali. usb_hid.c log-only → gercek.
2. **WebSocket (config_ap)**: ESP-IDF v6 httpd_ws API imzasi kontrol edilip
   implemente edilmeli. config_ap.c STUB → gercek WebSocket handler.
3. **Makro NVS**: macro tanimlari NVS blob olarak nvs_store'a eklenmeli.
4. **Config_Mode_Request**: C6 status bit'inden okunmali (simdilik SW2+ENC_BTN placeholder).
5. **PSRAM boyutu**: Xiao ESP32-S3 S3R2 (2MB) mi S3R8 (8MB) mi dogrulanmali.

## 2026-07-23 - S3'ü Çalışır Hale Getirme (Faz A/B/C)

### Faz A: TinyUSB (usb_hid gerçek)

- **esp_tinyusb managed component** eklendi (idf_component.yml, ^2.0.1~1).
- **usb_hid.c yeniden yazildi**: TinyUSB composite HID (tek interface, 3 report ID):
  - Report ID 1: 6KRO klavye (modifier + 6 keycode, bInterval=1 = 1000Hz)
  - Report ID 2: Consumer Control (16-bit usage, generic tud_hid_report)
  - Report ID 3: Gamepad (X/Y int8, generic tud_hid_report)
- **Descriptor**: TUD_HID_REPORT_DESC_KEYBOARD/CONSUMER/GAMEPAD + TUD_CONFIG/HID_DESCRIPTOR.
- **Callback'ler**: tud_suspend_cb/resume_cb (usb_power), tud_hid_descriptor_report_cb,
  tud_hid_get/set_report_cb. tud_mount_cb/umount_cb KALDIRILDI (esp_tinyusb cakismasi).
- **Rapor degisim kontrolu** (Bölüm 6.1): sadece degisimde gonder.
- **sdkconfig**: CONFIG_TINYUSB_HID_COUNT=1 (HID class aktif).

### Faz B: BOOT button CONFIG_AP geçişi

- **pinout.h**: PIN_BOOT_BUTTON = 0 (GPIO0, Xiao BOOT button, active low pull-up).
- **main.c check_boot_button()**: debounce (50ms) + kısa basış (<1sn) algılama.
- **Ana döngü**: NORMAL_MODE'da boot kısa basış → CONFIG_AP. CONFIG_AP'de boot kısa basış → NORMAL.
- C6'sız config mode erisimi saglandi.

### Faz C: WebSocket (config_ap gerçek)

- **sdkconfig**: CONFIG_HTTPD_WS_SUPPORT=y (WebSocket API aktif).
- **config_ap.c gerçek WebSocket handler**: /ws URI, httpd_ws_recv_frame/send_frame.
  Komutlar: "exit" (NORMAL_MODE), "get_adc" (100Hz ADC akisi), "stop_adc", "get_calib" (TODO).
- **config_ap_send_adc**: text frame "adc:1234,2345,...,4567" (async send).
- **CMakeLists**: esp_http_server REQUIRES geri eklendi.

### Alinan mimari kararlar (Faz A/B/C)

| Karar | Sonuc |
|---|---|
| 6KRO (standart keyboard) | NKRO karmaşık, 6KRO once. TUD_HID_REPORT_DESC_KEYBOARD. |
| Generic tud_hid_report (gamepad/consumer) | tud_hid_gamepad_report/consumer_report link hatasi, generic kullanildi. |
| tud_mount_cb/umount_cb kaldirildi | esp_tinyusb ile cakisma (multiple definition). tud_mounted() ile sorgu. |
| CONFIG_TINYUSB_HID_COUNT=1 | HID class derlenmesi icin. |
| BOOT button GPIO0 | Xiao'da mevcut, harici donanım yok. Active low pull-up. |
| CONFIG_HTTPD_WS_SUPPORT=y | WebSocket API v6'da config ile aktif. |

### S3 artik FONKSIYONEL

1. USB HID: bilgisayar 6KRO klavye + Consumer + Gamepad gorur (TinyUSB).
2. BOOT button (GPIO0) kısa basış → Wi-Fi AP (S3-Config) + WebSocket :80/ws.
3. WebSocket: exit/get_adc/stop_adc komutlari, ADC akisi 100Hz.
4. C6'dan ESP-NOW RX + Hall Effect motoru + USB HID 1000Hz raporlama.
5. Watchdog 1000ms → tum tuşlar birak.

### Kalan stub/TODO

- **get_calib/set_calib JSON**: kalibrasyon NVS oku/yaz WebSocket'ten (JSON parse karmaşık).
- **Makro NVS**: macro tanimlari NVS blob.

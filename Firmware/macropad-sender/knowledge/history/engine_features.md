# ESP32-C6 Macropad - Engine Features History

Bu dosya projeye eklenen tum yeni ozelliklerin ve sistemsel gelismelerin
kronolojik kaydidir. AI_GUIDELINES.md kural 3'e gore ZORUNLU tutulur.

## 2026-07-23 - F0: Proje Iskeleti

### Eklenen ozellikler

- **AI_GUIDELINES.md yeniden yazildi**: Rust/wgpu/Aeon Engine icerigi cikarildi,
  ESP-IDF v6.0.x / C dili icin guncellendi. Yeni bolumler: ESP-IDF API Accuracy
  (#15), Memory Safety C (#16), Concurrency Safety (#17), Pin & Hardware
  Abstraction (#18). Zero-Error Policy `idf.py build` ile 0 error/0 warning
  hedefi olarak baglandi.

- **ESP32-C6_algoritma.md guncellendi** (3 yer):
  1. Bölüm 1.2: SW2 ve ENC_BTN'in direk GPIO degil, U30 SN74HC165 shift
     register'inin E/F pinlerinden okundugu belirtildi. Pull-up ifadeleri
     kaldirildi. Diot-OR hattinin yalnizca 12 dijital tusu OR'ladigi netlestirildi.
     VBUS//CHG pin routing'i netlesmedigi icin firmware stub olarak tanimlandi.
  2. Bölüm 3.1.2: Encoder butonu (ENC_BTN) ve SW2'nin SR okuma döngüsünde
     U30 E/F pinlerinden okundugu belirtildi.
  3. Bölüm 4.1 (Dijital Mod): Light sleep uyanma mantigi guncellendi. Diot-OR
     GPIO wake (aninda) + periyodik timer wake (encoder/SR buton polling,
     ~50-100ms) iki kaynakli yapiya gecildi.

- **ESP-IDF proje iskeleti kuruldu**:
  - `CMakeLists.txt` (project root)
  - `sdkconfig.defaults`: ESP32-C6 target, USB Serial/JTAG log (UART0 RXD0/TXD0
    LED SR'a ayrildi), NVS, ESP-NOW/Wi-Fi, tickless idle, RMT IRAM.
  - `main/CMakeLists.txt`: 14 bileseni REQUIRES listesinde bagli.
  - `main/Kconfig.projbuild`: S3 MAC, ESP-NOW kanali, varsayilan guc modu/
    parlaklik/layer, fail-safe esigi, hibrit inaktivite timer, pil olcum
    periyodu, packet rate, wakeup polling period.
  - `main/include/pinout.h`: TUM GPIO atamalarinin tek kaynagi. Stub pinler
    (VBUS/CHG) -1 olarak tanimlandi.
  - `main/include/system_state.h`: persistent_config_t, battery_flags_t,
    link_state_t, system_state_t tipleri.
  - `main/main.c`: F0 minimal app_main (baslangic logu + tip derleme dogrulama).

- **14 bilesen iskeleti** (components/ altinda): nvs_store, spi_bus, mcp3202,
  mux_cd74hc4067, shift_in_sn74hc165, shift_out_74hc595, encoder, neopixel,
  power_ctrl, battery, charger_if, packet, espnow_link, wakeup_manager.
  Her biri CMakeLists.txt + include/<name>.h + <name>.c (yer tutucu).

### Alinan mimari kararlar

| Karar | Sonuc |
|---|---|
| Paket formati | 5B: [u8 sayac][u8 system_status][u16 LE dijital tuslar][i8 encoder_delta]. 23B: 5B + 18B (12x12-bit LE bit-packing). Onaylandi. |
| Analog kanal siralama | Mux adres sirasyla (kanal0 = mux adres 0). |
| SPI stratejisi | MCP3202 = HW SPI (SPI2_HOST). SN74HC165 + 74HC595 = bit-bang, ortak SCK IO6. |
| Log | USB Serial/JTAG (ESP32-C6'da USB-OTG yok). |
| OTA (Bölüm 6) | Hic baslanmayacak - sonraya birakildi. |
| S3 MAC/kanal | menuconfig sabit (NVS override ileride). |
| VBUS//CHG | Stub + charger_if soyutlamasi (pin sonradan). |
| SW2/ENC_BTN | U30 SR E/F pinlerinden (direk GPIO degil). |
| Diot-OR (IO4) | Yalnizca 12 dijital tusu OR'lar. |
| Wakeup | GPIO wake (IO4 Diot-OR) + periyodik timer wake (encoder/SR polling). |

### Bekleyen sorunlar

- ESP-IDF v6.0.2 Python venv kurulu degil (`C:\Users\mcskn\.espressif\python_env`
  bos). `idf.py build` icin kullanici tarafindan `install.ps1` calistirilmasi
  veya VS Code ESP-IDF extension "Configure ESP-IDF" adiminin tamamlanmasi
  gerekiyor. F0 iskeletinin build dogrulamasi bekliyor.

## 2026-07-23 - F1: Sensör Okuma Bilesenleri

### Eklenen ozellikler

- **nvs_store**: Generic (tip-bagimsiz) NVS wrapper. `nvs_store_init/get_u8/
  set_u8/get_u32/set_u32/get_bool/set_bool`. Namespace "mpd". file-local static
  handle. persistent_config_t'ye bagimli degil (circular dependency onlemek icin).

- **spi_bus**: Ortak SCK (IO6) bit-bang yonetimi. HW SPI KULLANILMIYOR (kullanici
  karari: tamamen bit-bang). Sebep: HW SPI bus IO6'yi SPI peripheral'ina atar,
  SR'lar togglelayamazdi. spi_bus YALNIZCA SCK pinini yonetir (MISO/MOSI her
  cihazda farkli pinde). `spi_bus_init(int sck_gpio)`, `spi_bus_set_sck(bool)`,
  `spi_bus_pulse_sck()`. AI_GUIDELINES.md kural 18: pin numarasi parametre olarak
  main.c'den alinir, pinout.h include edilmez.

- **mcp3202**: MCP3202 12-bit ADC bit-bang SPI surucu. Mode 0, 17 clock/okuma.
  `mcp3202_init(cs, miso, mosi)`, `mcp3202_read_channel(ch)` -> 0-4095.
  ~17us/okuma, 12 kanal ~200us. spi_bus'a REQUIRES.

- **mux_cd74hc4067**: 16-kanal analog mux. `mux_init(s0,s1,s2,s3)`,
  `mux_select(ch)`. Kanal 0-11 analog tuslar, 12 pil, 13 /CHG (stub).

- **shift_in_sn74hc165**: SN74HC165 x2 zincir okuma. 12 dijital tus + SW2 (U30 F)
  + ENC_BTN (U30 E). Bit count parametrik (16 algoritma / 24 pcb celiski).
  `shift_in_init(latch, data, bit_count)`, `shift_in_read()` -> uint32_t MSB first.

- **shift_out_74hc595**: 74HC595 layer LED (D8-D11) surme. `shift_out_init(rclk,
  ser, bit_count)`, `shift_out_write(value)`. RCLK rising pulse ile output guncelle.

- **main.c F1**: `init_subsystems()` tum bilesenleri baslatir, NVS'den kalici
  ayarlari yukler. `f1_sensor_test()` ADC + SR + LED test logu.

### Alinan mimari kararlar (F1)

| Karar | Sonuc |
|---|---|
| HW SPI yerine tamamen bit-bang | Kullanici karari. SCK paylasim sorunu cozuldu. |
| spi_bus sadece SCK yonetir | MISO/MOSI her cihazda farkli pinde oldugu icin. |
| Pin numaralari init parametresi | AI_GUIDELINES.md kural 18 (circular dep onle). |
| nvs_store generic wrapper | system_state.h'ya bagimlilik yok (circular dep onle). |
| shift_in bit count parametrik | 16/24 pcb celiski Kconfig'e tasinacak. |
| `esp_driver_gpio` component'i | ESP-IDF v6: `driver` parcalandi. |

### Bekleyen sorunlar

- Shift register bit sayisi (16 vs 24) henuz netlesmedi; main.c'de 16 sabit.
  Kconfig'e tasinmasi bekleniyor.
- SR bit mapping (hangi bit hangi tus/buton) main.c'de yapilacak (F2/F3).

## 2026-07-23 - F2: Encoder + Neopixel + Power Control

### Eklenen ozellikler

- **encoder**: PCNT v2 API (driver/pulse_cnt.h) ile quadrature 4x decode.
  `encoder_init(a, b)`, `encoder_get_delta()` -> int16_t (count oku + sıfırla).
  Glitch filter 1000ns. -32768..32767 limit. AI_GUIDELINES.md kural 15: eski
  driver/pcnt.h DEPRECATED, driver/pulse_cnt.h kullanıldı.

- **neopixel**: SK6812MINI 12 LED RMT v6 surucu. Kendi bytes encoder
  (rmt_new_bytes_encoder), led_strip managed component KULLANILMADI (bağımlılık
  yok). 10 MHz resolution. `neopixel_init/set_pixel/fill/set_brightness/
  refresh/clear`. GRB sirasi. Parlaklik %0-100 ölçekleme. Reset: 80us delay.

- **power_ctrl**: MOSFET (ANALOG_PWR_EN + TPS_EN) + parlaklik hesaplama +
  mod LED geri bildirimi (Bölüm 4.5). `power_ctrl_calc_brightness()` mod limit
  (Dijital %40, Hibrit %60, Agresif %80) + flag_25 %30 cap. `power_ctrl_mode_
  feedback()` 3 sn bloklayıcı LED geri bildirimi.

- **system_state component**: system_state.h main/include'den components/
  system_state'a taşındı. Sebep: power_ctrl/packet/espnow_link/battery
  power_mode_t/battery_flags_t'ye erişmeli, ama main'e REQUIRES yapamaz
  (circular dependency). Tip sağlayıcı header-only component.

### Alinan mimari kararlar (F2)

| Karar | Sonuc |
|---|---|
| PCNT v2 API (pulse_cnt.h) | Eski pcnt.h deprecated. |
| Kendi RMT encoder | led_strip managed component bağımlılığı yok. |
| system_state component | Circular dependency çözüldü. Tüm componentler tiplere erişebilir. |
| power_ctrl → neopixel REQUIRES | LED geri bildirimi için coupling (kabul edilebilir). |

### Bekleyen sorunlar

- (Devam eden) SR bit mapping (F3).
- esp_rom_delay_us neopixel.c'de kullanıldı, esp_rom REQUIRES eklendi.

## 2026-07-23 - F3: Packet + ESP-NOW Link

### Eklenen ozellikler

- **packet**: 5B hizli + 23B tam paket olusturma. `packet_make_status()`
  bit haritasi (mode bit0-1, SW2 bit2, ENC_BTN bit3, flag_15 bit4, battery_dead
  bit5, charging bit6, charge_complete bit7). `packet_build_fast()` 5B (counter +
  status + u16 LE buttons + i8 delta). `packet_pack_analog()` 12x12-bit LE
  bit-packing (bit-by-bit, 18 byte). `packet_build_full()` 23B. Onayli format.

- **espnow_link**: ESP-NOW S3 TX + callback + fail-safe. `espnow_link_init()`
  Wi-Fi STA + ESP-NOW + peer + send_cb. `espnow_link_send()`. Tx callback wifi
  task context'inde, shared state portMUX critical section ile korumali.
  Fail-safe: 100 ardisik basarisiz → search_mode (1 Hz). Accessor: is_connected,
  is_search_mode, get_failed_count, reset.

### Alinan mimari kararlar (F3)

| Karar | Sonuc |
|---|---|
| esp_now component'i v6'da yok | esp_wifi icerisinde. REQUIRES esp_wifi yeterli. |
| ESP-NOW v6 callback imzasi | esp_now_send_info_t (eski uint8_t *mac_addr degil). |
| packet bit-packing bit-by-bit | Basit + dogru. 1 kHz'de 144 bit ihmal edilebilir. |
| espnow_link kendi internal state | system_state.h'ya bagimli degil. main.c accessor ile senkron. |
| Critical section portMUX | Tx callback (wifi task) <-> main task shared state korumasi. |

## 2026-07-23 - F4 + F5: Battery, Charger, Wakeup, Ana Döngü (TAMAM)

### Eklenen ozellikler

- **battery**: Pil yonetimi (Bölüm 4.4). `battery_update(percent, state)` flag_50/25/15/5
  kontrol + animasyonlar. `battery_blink_layer()` flag_15 layer LED 5 blink.
  `battery_adc_to_percent()` stub lineer map (kalibrasyon Kconfig ile). Animasyonlar
  neopixel + power_ctrl (LED MOSFET) kullanir. battery_dead set → main.c deep sleep.

- **charger_if**: VBUS//CHG stub soyutlama + sarj animasyonlari (Bölüm 5).
  pin=-1 → stub mode (sabit false). Gerçek pin ile GPIO okuma.
  `play_charge_start_animation()` mor basamak (NEO1-3→6→9→12, 500ms/step).
  `play_charge_complete_animation()` dusuk guc yesil sabit.
  `clear_battery_flags()` Bölüm 5 sarja takilinca flag sifirla.

- **wakeup_manager**: Light/deep sleep + wakeup (Bölüm 4.1, 4.4).
  `light_sleep(poll_ms)` GPIO wake (Diot-OR IO4) + timer wake (polling).
  `deep_sleep()` ext1 wakeup (ESP32-C6 ext0 desteklemiyor). `woken_by_gpio()`
  wakeup cause sorgu. IO4 = RTC GPIO 4 varsayimi.

- **main.c Bölüm 2 ana döngü**: Tam implementasyon. 8 step fonksiyonu:
  step_charge_check (Bölüm 5), step_critical_battery (Bölüm 4.4 deep sleep),
  step_mode_change (Bölüm 2.1.4 SW2+encoder), step_brightness_change (Bölüm 2.1.5
  Tuş12+encoder), step_read_and_send (Bölüm 3 sensör+paket), step_battery_periodic
  (Bölüm 4.4 10sn), step_power_sleep (Bölüm 4.1/4.2 light sleep). Deep sleep'ten
  uyanista kritik kilitle layer LED 5 sn blink.

### Alinan mimari kararlar (F4+F5)

| Karar | Sonuc |
|---|---|
| ext0 yok ESP32-C6'da | ext1 wakeup (ANY_LOW/ANY_HIGH) kullanildi. |
| esp_sleep_get_wakeup_cause deprecated | esp_sleep_get_wakeup_causes (bitmask) kullanildi. |
| rtc_gpio pullup manuel değil | ext1 otomatik pullup yapar, manuel kaldirildi. |
| battery animasyonlar neopixel+power_ctrl | battery bu'lara REQUIRES (coupling kabul). |
| SR bit mapping VARSAYIM | bit0-11 tuşlar, bit2=SW2, bit3=ENC_BTN. Donanim testi gerekli. |
| Bölüm 6 (RX/OTA) ATLANDI | Kullanici karari, sonraya birakildi. |
| Kombinasyon denetimi ATLANDI | Bölüm 6 bagimli, sonraya. |

### Bekleyen sorunlar (donanim testi gerekli)

1. **SR bit mapping**: shift_in_sn74hc165_read() 16 bit, hangi bit hangi tuş/buton
   VARSAYIM. Donanim testinde dogrulanmali (SR_BUTTONS_MASK, SR_SW2_BIT, SR_ENC_BTN_BIT).
2. **Pil ADC kalibrasyonu**: battery_adc_to_percent stub lineer (adc_min=0, adc_max=4095).
   Gerçek pil voltaj bölücü ile kalibre edilmeli (Kconfig).
3. **VBUS//CHG pin routing**: stub (-1). Donanim karari netlesince pinout.h güncellenecek.
4. **Diot-OR polarity**: active low varsayildi. Donanima göre düzeltilebilir.
5. **Hibrit inaktivite timer**: step_power_sleep'te basit sayac, gercek hareket
   algilama (tuş/encoder) ile sifirlama main.c'de eklenecek.
6. **Tuş12 birakma → NVS + 2sn söndürme**: step_brightness_change'de kismi, rafine edilmeli.

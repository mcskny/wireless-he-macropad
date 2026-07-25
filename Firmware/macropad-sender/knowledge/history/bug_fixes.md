# ESP32-C6 Macropad - Bug Fixes History

Bu dosya tum hata duzeltmelerinin, cokme cozumlerinin ve kararlik yamalarinin
kronolojik kaydidir. AI_GUIDELINES.md kural 3'e gore ZORUNLU tutulur.

## 2026-07-23 - F1 Build Duzeltmeleri

### FIX-001: `driver` component'i -> `esp_driver_gpio` (ESP-IDF v6)

**Semptom:** F1 ilk buildinde hata:
```
mcp3202.c includes driver/gpio.h, provided by esp_driver_gpio component(s).
However, esp_driver_gpio is not in the requirements list of "mcp3202".
```

**Kok Neden:** ESP-IDF v5.1+'dan itibaren eski monolitik `driver` component'i
parcalandi. `driver/gpio.h` artik `esp_driver_gpio` component'inde. v6.0.2'de
`REQUIRES driver` ile `driver/gpio.h` include edilemiyor.

**Cozum:** Tum GPIO kullanan bilesenlerin CMakeLists.txt'inde
`REQUIRES driver` -> `REQUIRES esp_driver_gpio` yapildi:
- spi_bus, mcp3202, mux_cd74hc4067, shift_in_sn74hc165, shift_out_74hc595

**Etki:** Build basarili (0 error / 0 warning). AI_GUIDELINES.md kural 15
(ESP-IDF API Accuracy) dogrulandi: v6 parcalanmis component yapisi kullaniliyor.

### FIX-002: `pinout.h` bilesenler tarafindan include edilemiyor (circular dep)

**Semptom:** spi_bus.c `#include "pinout.h"` yaptiginda, pinout.h main/include
altinda oldugu icin bilesen main'e REQUIRES yapamaz (circular dependency).

**Kok Neden:** AI_GUIDELINES.md kural 18 ("Components receive pin numbers via
init functions") ihlal edilmisti.

**Cozum:** `spi_bus_init()` imzasi `spi_bus_init(int sck_gpio)` seklinde
degistirildi. pinout.h include'u kaldirildi. Tum bilesenler pin numaralarini
init parametresi olarak main.c'den alir. Ayni pattern mcp3202, mux,
shift_in, shift_out icin de uygulandi.

### FIX-003: sdkconfig.defaults gecersiz config'ler

**Semptom:** Ilk buildte "Value is treated as 'n'" uyarilari.

**Kok Neden:** ESP-IDF v6'da bazi config isimleri yok veya degisti:
- `CONFIG_ESP_CONSOLE_UART_NONE` -> yok (USB_SERIAL_JTAG=y UART'i otomatik kapatir)
- `CONFIG_COMPILER_WARNING_REMOVED_FLAGS` -> yok
- `CONFIG_COMPILER_WARN_X86_SSE` -> x86 specific, ESP32-C6'da yok
- `CONFIG_RMT_ISR_HANDLER_IN_IRAM` -> `CONFIG_RMT_TX_ISR_HANDLER_IN_IRAM`
- `CONFIG_PM_POWER_DOWN_CPU` -> `CONFIG_PM_POWER_DOWN_CPU_IN_LIGHT_SLEEP`

**Cozum:** Geçersiz config'ler kaldirildi veya dogru isimle degistirildi.
Fullclean + rebuild sonrasi 0 compiler warning (ESP-IDF internal BLE_MESH
Kconfig uyarilari AI_GUIDELINES.md kural 1 ile tolere edilir).


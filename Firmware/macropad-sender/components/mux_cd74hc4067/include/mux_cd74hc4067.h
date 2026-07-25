// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file mux_cd74hc4067.h
 * @brief CD74HC4067 16-kanal analog coklayici (mux) surucu.
 *
 * S0-S3 adres pinleri ile 0-15 arasi kanal secilir. Secilen kanalin sinyali
 * mux'un ortak cikisina (Z) baglanir; bu cikis MCP3202 ADC'nin girisine gider.
 *
 * Kullanilan pinler (pcb_pinler.md):
 *   S0=IO18, S1=IO19, S2=IO20, S3=IO15
 *
 * Kanal eslemesi (Bölüm 3.1.3 - mux adres sirasiyla paketlenir):
 *   - Adres 0..11  → ANA_KEY_0..11 (analog Hall sensor tuslar)
 *   - Adres 12     → Pil voltaj boleni (stub; pin sonradan)
 *   - Adres 13     → /CHG sinyali (stub; pin sonradan)
 *   - Adres 14, 15 → reserve
 *
 * @note Mux gecis suresi ~50ns (CD74HC4067 spec). Kanal degisiminden sonra
 *       ADC okumadan once kisa bir settle suresi (birak~1us) onerilir.
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Toplam kanal sayisi (CD74HC4067 16 kanallidir). */
#define MUX_CD74HC4067_CHANNEL_COUNT 16

/**
 * @brief Mux adres pinlerini (S0-S3) GPIO output olarak yapilandirir.
 *
 * @param s0_gpio S0 pin numarasi.
 * @param s1_gpio S1 pin numarasi.
 * @param s2_gpio S2 pin numarasi.
 * @param s3_gpio S3 pin numarasi.
 * @return ESP_OK veya GPIO yapilandirma hatasi.
 */
esp_err_t mux_cd74hc4067_init(int s0_gpio, int s1_gpio, int s2_gpio, int s3_gpio);

/**
 * @brief Belirtilen kanali mux cikisina (Z) baglar.
 *
 * @param channel 0-15 arasi kanal numarasi. >=16 ise hata loglanir ve
 *                fonksiyon etkisiz olur.
 * @return ESP_OK veya ESP_ERR_INVALID_ARG.
 *
 * @note Kanal degisiminden sonra ADC okumadan once ~1us settle suresi birak.
 */
esp_err_t mux_cd74hc4067_select(uint8_t channel);

#ifdef __cplusplus
}
#endif

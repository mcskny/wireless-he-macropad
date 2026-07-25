// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file power_ctrl.h
 * @brief Guc kontrolu: MOSFET yonetimi + parlaklik hesaplama + mod geri bildirimi.
 *
 * Sorumluluklar:
 *   1. Analog MOSFET (Q1, ANALOG_PWR_EN IO21) ve LED boost (TPS_EN IO14) ac/kapat.
 *   2. Guc moduna gore effective_brightness hesapla (Bölüm 4.5).
 *   3. Mod degisiminde LED geri bildirimi (3 sn renk gösterimi).
 *
 * Parlaklik kurallari (Bölüm 4.5):
 *   - Dijital mod : max %40
 *   - Hibrit mod  : max %60
 *   - Agresif mod : max %80
 *   - Pil <%25   : max %30 (flag_25)
 *   effective = min(base_brightness, mod_limit), sonra flag_25 ile %30 cap.
 *
 * @note neopixel bilesenine REQUIRES yapar (LED geri bildirimi icin).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "system_state.h"  /* power_mode_t */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MOSFET pinlerini (analog + LED boost) yapilandirir, ikisini de KAPALI baslatir.
 *
 * @param analog_pwr_gpio ANALOG_PWR_EN pin (Q1 P-FET gate). Active high.
 * @param tps_en_gpio     TPS_EN pin (LED boost regulator). Active high.
 * @return ESP_OK veya GPIO hatasi.
 */
esp_err_t power_ctrl_init(int analog_pwr_gpio, int tps_en_gpio);

/**
 * @brief Analog sensör MOSFET'ini (Q1) acar/kapatir.
 * @param on true=ac (analog sensörlere guç), false=kapat (pil tasarrufu).
 */
void power_ctrl_analog_power(bool on);

/**
 * @brief LED boost regulatorunu (TPS_EN) acar/kapatir.
 * @param on true=ac (5V_LED hatti, neopiksel + layer LED), false=kapat.
 */
void power_ctrl_led_power(bool on);

/**
 * @brief Guc modu ve pil durumuna gore effective parlakligi hesaplar.
 *
 * @param mode            Aktif guc modu.
 * @param base_brightness Kullanici taban parlakligi (0-100, NVS'den).
 * @param flag_25         Pil <%25 bayragi (parlakligi %30 ile caplar).
 * @return Effective parlaklik (0-100).
 */
uint8_t power_ctrl_calc_brightness(power_mode_t mode,
                                   uint8_t base_brightness,
                                   bool flag_25);

/**
 * @brief Hesaplanan parlakligi neopixel'e uygular (neopixel_set_brightness cagirir).
 */
void power_ctrl_apply_brightness(power_mode_t mode,
                                 uint8_t base_brightness,
                                 bool flag_25);

/**
 * @brief Bölüm 4.5: Guc modu LED geri bildirimi.
 *
 * Akis:
 *   1. LED MOSFET ac (TPS_EN).
 *   2. Effective brightness uygula.
 *   3. 12 neopikseli mod rengine boya (Dijital=Yeşil, Hibrit=Mavi, Agresif=Kirmizi).
 *   4. Refresh.
 *   5. 3 saniye bekle (BLOKLAYICI - sadece mod degisiminde cagrilmali).
 *   6. LED'leri söndür, MOSFET kapat.
 *
 * @note Bu fonksiyon 3 saniye bloklar. Ana döngüde DEGIL, sadece mod degisiminde.
 */
void power_ctrl_mode_feedback(power_mode_t mode, uint8_t base_brightness, bool flag_25);

#ifdef __cplusplus
}
#endif

// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file battery.h
 * @brief Pil yonetimi: olcum + flag_50/25/15/5 + kritik kapanma (Bölüm 4.4).
 *
 * Sorumluluklar:
 *   1. Pil yuzdesini alir (main.c ADC'den hesaplar), flag'leri gunceller.
 *   2. Pil <%50: 6 neopiksel Sarı 3 blink.
 *   3. Pil <%25: 3 neopiksel Sarı 3 blink + parlaklik %30 cap.
 *   4. Pil <%15: flag_15 set, her 5 dk'da layer LED 5 blink (main.c timer).
 *   5. Pil <%5: 12 neopiksel Kirmizi 3 blink + battery_dead=true (kritik kilitle).
 *
 * Animasyonlar neopixel + power_ctrl (LED MOSFET) kullanir.
 *
 * @note Kritik kapanma (deep sleep) wakeup_manager + main.c ile entegre (F5).
 *       battery sadece battery_dead=true set eder; deep sleep'e girme main.c'de.
 * @note Pil yuzdesi hesabi (ADC -> voltaj -> yüzde) donanima bagli, kalibrasyon
 *       Gerekli. battery_adc_to_percent stub lineer map; Kconfig ile kalibre edilir.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "system_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Blink sayisi ve suresi (Bölüm 4.4). */
#define BATTERY_BLINK_COUNT       3
#define BATTERY_BLINK_ON_MS       200
#define BATTERY_BLINK_OFF_MS      200

/** Layer LED blink (flag_15, her 5 dk). */
#define BATTERY_LAYER_BLINK_COUNT 5

/**
 * @brief Battery state'i sifirlar (tum flag'ler false). app_main'de BIR KEZ.
 */
void battery_init(void);

/**
 * @brief Pil yuzdesini alir, flag'leri gunceller, animasyonlari tetikler.
 *
 * Bölüm 4.4 kontrol sirasi: %50 → %25 → %15 → %5. Her esik flag'i ilk
 * geciste animasyon oynatir, tekrar oynatmaz (flag=true).
 *
 * @param percent Pil yuzdesi (0-100).
 * @param state   Sistem state'i (battery_flags + battery_dead guncellenir).
 * @return true = battery_dead set edildi (kritik kilitle), main.c deep sleep'e girmeli.
 */
bool battery_update(uint8_t percent, system_state_t *state);

/**
 * @brief Layer LED blink (flag_15 icin, her 5 dk'da bir cagrilmali).
 *
 * Aktif katman LED'ini (D8-D11) 5 kez yakip sondurur.
 *
 * @param active_layer Aktif katman indeksi (0-3, D8-D11).
 * @note shift_out_74hc595 kullanir. LED MOSFET (TPS_EN) main.c tarafindan
 *       acik olmali (bu fonksiyon MOSFET acmaz).
 */
void battery_blink_layer(uint8_t active_layer);

/**
 * @brief ADC ham degerini pil yuzdesine cevirir (stub lineer map).
 *
 * Kalibrasyon: Vbat_min (esik %0) ve Vbat_max (esik %100) Kconfig'ten.
 * Lineer: percent = (adc - adc_min) * 100 / (adc_max - adc_min).
 *
 * @param adc_raw MCP3202'den okunan 12-bit deger (0-4095).
 * @return Pil yuzdesi (0-100, clamped).
 */
uint8_t battery_adc_to_percent(uint16_t adc_raw);

#ifdef __cplusplus
}
#endif

// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file neopixel.h
 * @brief SK6812MINI (12 LED) neopixel surucu - RMT v6 API ile.
 *
 * ESP32-C6'nin RMT donanimi kullanilarak SK6812MINI LED'ler surulur.
 * Harici led_strip managed component KULLANILMAZ; kendi bytes encoder'imiz
 * yazildi (AI_GUIDELINES.md: bağımlılık olmaması tercih edilir).
 *
 * SK6812MINI protokolu:
 *   - Bit 1: T1H=600ns high, T1L=600ns low
 *   - Bit 0: T0H=300ns high, T0L=900ns low
 *   - Reset: >80us low (bus idle)
 *   - Renk sirasi: GRB (Yeşil, Kırmızı, Mavi)
 *
 * RMT clock resolution: 10 MHz (100 ns/tick).
 *
 * Parlaklik yonetimi (Bölüm 4.5):
 *   - neopixel_set_brightness(percent) ile global parlaklik limiti set edilir.
 *   - set_pixel'de r/g/b degerleri brightness ile ölçeklenir.
 *   - power_ctrl bileseni mod bazli limiti (Dijital %40, Hibrit %60, Agresif %80)
 *     base_brightness ile caplayıp neopixel_set_brightness'a gecer.
 *
 * Kullanilan pin (pcb_pinler.md): NEOPIXEL_DATA = IO5
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RMT TX channel ve bytes encoder yapilandirir.
 *
 * @param gpio      Neopixel data pin.
 * @param led_count LED sayisi (bu projede 12).
 * @return ESP_OK veya RMT yapilandirma hatasi.
 */
esp_err_t neopixel_init(int gpio, int led_count);

/**
 * @brief Bir pixel'in rengini set eder (buffer'a yazar, transmit ETMEZ).
 *
 * Parlaklik limiti (neopixel_set_brightness) r/g/b degerlerine uygulanir.
 * SK6812 GRB sirasi ile buffer'a yazilir.
 *
 * @param index Pixel indeksi (0..led_count-1).
 * @param r     Kırmızı (0-255, brightness oncesi).
 * @param g     Yeşil  (0-255, brightness oncesi).
 * @param b     Mavi   (0-255, brightness oncesi).
 */
void neopixel_set_pixel(int index, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Tum pixel'leri bir renge set eder.
 * @param r,g,b Renk degerleri (brightness uygulanir).
 */
void neopixel_fill(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Global parlaklik limitini set eder.
 * @param percent 0-100. Mevcut pixel'leri etkilemez; sonraki set_pixel'lerde uygulanir.
 */
void neopixel_set_brightness(uint8_t percent);

/**
 * @brief Buffer'daki pixel verisini LED'lere transmit eder.
 *
 * RMT transmit + reset delay (~1ms > 80us SK6812 reset).
 * @return ESP_OK veya RMT transmit hatasi.
 */
esp_err_t neopixel_refresh(void);

/**
 * @brief Tum pixel'leri söndür (0,0,0) ve refresh eder.
 * @return ESP_OK veya refresh hatasi.
 */
esp_err_t neopixel_clear(void);

#ifdef __cplusplus
}
#endif

// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file encoder.h
 * @brief Rotary encoder surucu - PCNT v2 API ile quadrature decode.
 *
 * ESP32-C6'nin PCNT (Pulse Counter) donanimi kullanilarak encoder A/B fazlari
 * donanimsal olarak decode edilir. CPU'yu mesgul etmeden count biriktirir.
 *
 * Quadrature 4x decode (en yuksek cozunurluk):
 *   - A rising,  B low  → count up
 *   - A rising,  B high → count down
 *   - A falling, B low  → count down
 *   - A falling, B high → count up
 *
 * Kullanilan pinler (pcb_pinler.md):
 *   ENC_A = IO22 (filtreli, R43/C9), ENC_B = IO23 (filtreli, R42/C10)
 *
 * @note ESP-IDF v6 PCNT v2 API kullanilir (driver/pulse_cnt.h).
 *       Eski driver/pcnt.h DEPRECATED, kullanilmaz (AI_GUIDELINES.md kural 15).
 *
 * @note encoder_get_delta() count'u okur ve SIFIRLAR. Her ana döngü adiminda
 *       bir kez cagrilmalidir. Race condition (get ve clear arasi encoder pulse)
 *       ihmal edilebilir (1 kHz döngüde insan hizi encoder).
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PCNT unit ve channel yapilandirir, quadrature decode baslatir.
 *
 * @param phase_a_gpio Encoder A faz pini (edge signal).
 * @param phase_b_gpio Encoder B faz pini (level signal).
 * @return ESP_OK veya PCNT yapilandirma hatasi.
 */
esp_err_t encoder_init(int phase_a_gpio, int phase_b_gpio);

/**
 * @brief Son okumadan beri biriken encoder delta'sini okur ve sayaci sifirlar.
 *
 * @return Delta degeri (signed). Pozitif = saat yonu, negatif = ters yon.
 *         Init edilmemisse 0 doner.
 *
 * @note Bu fonksiyon atomik DEGILDIR (get_count + clear_count). 1 kHz döngüde
 *       sorun degil; daha hassas senaryolarda critical section gerekebilir.
 */
int16_t encoder_get_delta(void);

/**
 * @brief Encoder sayacini sifirlar (get_delta yapmadan).
 */
void encoder_clear(void);

#ifdef __cplusplus
}
#endif

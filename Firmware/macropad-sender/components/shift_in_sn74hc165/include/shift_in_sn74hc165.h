// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file shift_in_sn74hc165.h
 * @brief SN74HC165 (x2) shift-in surucu - dijital tuslar + SW2 + ENC_BTN.
 *
 * U29 + U30 SN74HC165 shift register'lari zincirli baglanmistir. Ortak SCK
 * pin (spi_bus) ile clock'lanir; LATCH pini paralel yuklemeyi tetikler.
 *
 * Okunan verinin icerigi (bit sirasi MSB-first, U29 once):
 *   - Ilk 12 bit: TCS40DLR dijital tus durumlari (Tuş 1-12)
 *   - U30 E pini : ENC_BTN (encoder butonu)
 *   - U30 F pini : SW2 (Layer butonu)
 *   - Kalan bitler: reserve/bos
 *
 * @note Algoritma Bölüm 3.1.1 "16 bit" der; pcb_pinler.md "24 bit" der.
 *       Bu celiski nedeniyle bit_count parametrik yapildi; main.c tarafindan
 *       CONFIG_SHIFT_IN_BITS (Kconfig) ile set edilir.
 *
 * SPI Mode 0 uyumlu: LATCH=CS, her rising edge'de 1 bit sample (QH oku) +
 * shift. N clock = N bit. Ilk bit = U29 H pini (MSB).
 *
 * Kullanilan pinler (pcb_pinler.md):
 *   LATCH=IO0 (SH/~LD), DATA=IO1 (QH), SCK=IO6 (spi_bus ortak)
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Shift register pinlerini ve bit sayisini yapilandirir.
 *
 * spi_bus_init() cagrildiktan sonra cagrilmalidir (SCK hazir olmali).
 *
 * @param latch_gpio LATCH (SH/~LD) pin numarasi (output, active low).
 * @param data_gpio  DATA (QH) pin numarasi (input).
 * @param bit_count  Okunacak bit sayisi (16 veya 24 onerilir).
 * @return ESP_OK veya GPIO yapilandirma hatasi.
 */
esp_err_t shift_in_sn74hc165_init(int latch_gpio, int data_gpio, uint8_t bit_count);

/**
 * @brief Shift register'lardan N bit okur ve ham deger olarak dondurur.
 *
 * Protokol:
 *   1. LATCH low (paralel yukle)
 *   2. LATCH high (shift mode)
 *   3. bit_count clock: her rising edge'de 1 bit oku (MSB first)
 *   4. LATCH high (idle)
 *
 * @return Ham shift-in degeri. bit_count 16 ise 0-65535 arasi.
 *         Init edilmemisse 0 doner.
 *
 * @note Bit konumlari (hangi bit hangi tus/buton) main.c tarafindan map edilir.
 */
uint32_t shift_in_sn74hc165_read(void);

#ifdef __cplusplus
}
#endif

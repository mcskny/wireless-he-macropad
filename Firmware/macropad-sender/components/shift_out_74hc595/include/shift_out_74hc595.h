// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file shift_out_74hc595.h
 * @brief 74HC595 shift-out surucu - layer/durum LED'leri (D8-D11).
 *
 * U14 74HC595 shift register, durum LED'leri D8-D11 icin kullanilir.
 * Ortak SCK pin (spi_bus) ile clock'lanir; RCLK output latch, SER data input.
 *
 * Protokol (SPI Mode 0 uyumlu):
 *   1. RCLK low
 *   2. N bit: her SCK rising'de 1 bit SER'e yaz + shift (MSB first)
 *   3. RCLK rising pulse: paralel cikis (QA-QH) guncelle
 *
 * Kullanilan pinler (pcb_pinler.md):
 *   RCLK=IO30 (RXD0 GPIO kullanimi), SER=IO31 (TXD0 GPIO kullanimi),
 *   SCK=IO6 (spi_bus ortak)
 *
 * @note RXD0/TXD0 normalde UART0 pinleridir; bu projede USB Serial/JTAG
 *       log kullanildigi icin bu pinler GPIO olarak LED SR'a ayrilmistir.
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 74HC595 pinlerini ve bit sayisini yapilandirir.
 *
 * spi_bus_init() cagrildiktan sonra cagrilmalidir.
 *
 * @param rclk_gpio RCLK (storage/latch) pin numarasi (output, rising edge guncelle).
 * @param ser_gpio  SER (data input) pin numarasi (output).
 * @param bit_count Yazilacak bit sayisi (8 onerilir, 74HC595 8-bit).
 * @return ESP_OK veya GPIO yapilandirma hatasi.
 */
esp_err_t shift_out_74hc595_init(int rclk_gpio, int ser_gpio, uint8_t bit_count);

/**
 * @brief Shift register'a N bit yazar ve cikisi gunceller (RCLK pulse).
 *
 * @param value Yazilacak deger. Dusuk anlamlı bitler SON shift edilir (LSB
 *              en son QH'ye gelir). MSB first shift: bit (bit_count-1) → QA.
 * @note Init edilmemise etkisiz olur (loglanir).
 */
void shift_out_74hc595_write(uint32_t value);

#ifdef __cplusplus
}
#endif

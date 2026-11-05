// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file spi_bus.h
 * @brief Ortak SCK pininin bit-bang yonetimi.
 *
 * Bu projede MCP3202 ADC, SN74HC165 (dijital tus SR) ve 74HC595 (LED SR)
 * ayni SCK hattini paylasir. Donanimin HW SPI peripheral'i KULLANILMAZ
 * (kullanici karari: tamamen bit-bang). Sebep: HW SPI bus baslatilinca SCK
 * pin SPI peripheral'ina atanir ve SR'lar SCK'yi togglelayamazdi.
 *
 * MISO/MOSI pinleri her cihaz icin FARKLIDIR (pcb routing):
 *   - MCP3202: MISO=IO2, MOSI=IO3, CS=IO7
 *   - SN74HC165: DATA=IO1 (MISO), LATCH=IO0
 *   - 74HC595: SER=IO31 (MOSI), RCLK=IO30
 *
 * Bu yuzden spi_bus YALNIZCA ortak SCK pinini yonetir. Her cihaz kendi
 * MISO/MOSI/LATCH pinini kendi bileseni icinde yapilandirir ve buradaki
 * spi_bus_set_sck() / spi_bus_pulse_sck() fonksiyonlarini kullanir.
 *
 * SPI Mode 0 varsayilir: SCK idle LOW, rising edge'de sample/shift.
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Ortak SCK pinini GPIO output olarak yapilandirir, LOW seviyesine ceker.
 *
 * app_main'de BIR KEZ cagrilmalidir. MCP3202 / shift_in / shift_out
 * bilesenleri bu cagrilmadan once SCK kullanmamalidir.
 *
 * AI_GUIDELINES.md kural 18: bilesenler pinout.h'yi include ETMEZ;
 * pin numarasini main.c tarafindan parametre olarak alir.
 *
 * @param sck_gpio SCK pin numarasi (ESP32-C6 icin GPIO numarasi).
 * @return ESP_OK veya GPIO yapilandirma hatasi.
 */
esp_err_t spi_bus_init(int sck_gpio);

/**
 * @brief SCK pininin seviyesini ayarlar.
 * @param level true=HIGH, false=LOW
 * @note SPI Mode 0: idle LOW. Rising edge (LOW→HIGH) sample/shift anidir.
 */
void spi_bus_set_sck(bool level);

/**
 * @brief Tek bir SCK clock pulse uretir: LOW → HIGH → LOW.
 *
 * Rising edge (LOW→HIGH) tum cihazlarda sample/shift anidir. Falling edge
 * (HIGH→LOW) sadece idle'a donustur. Cagri sonunda SCK LOW olur (Mode 0 idle).
 *
 * @note Bu fonksiyon ~1us surer (GPIO toggle + kisa delay). 1 MHz'e kadar
 *       ~100 ns/kenar ile calisir. Daha yuksek hiz gerekirse delay kaldirilabilir.
 */
void spi_bus_pulse_sck(void);

#ifdef __cplusplus
}
#endif

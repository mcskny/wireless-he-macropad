// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file mcp3202.h
 * @brief MCP3202 2-kanal 12-bit ADC bit-bang SPI surucusu.
 *
 * MCP3202 SPI Mode 0 protokolu kullanir (SCK idle LOW, rising edge sample).
 * Donanim HW SPI KULLANILMAZ (tamamen bit-bang). SCK pin spi_bus bileseninden
 * ortak kullanilir; MISO/MOSI/CS pinleri bu bilesene init sirasinda verilir.
 *
 * Protokol (17 clock):
 *   1. CS low
 *   2. Din (MOSI): start=1, SGL/DIFF=1 (single), ODD/SIGN=ch, MSBF=1  (4 clock)
 *   3. Dout (MISO): null bit (yoksay)                                  (1 clock)
 *   4. Dout (MISO): B11..B0 (12-bit, MSB first)                       (12 clock)
 *   5. CS high
 *
 * Cozum suresi: ~17 us/okuma (1 MHz bit-bang). 12 kanal ~200 us.
 *
 * Kullanilan pinler (pcb_pinler.md):
 *   - SCK  = IO6 (spi_bus ortak)
 *   - MISO = IO2 (ADC DOUT)
 *   - MOSI = IO3 (ADC DIN)
 *   - CS   = IO7 (ADC ~CS)
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MCP3202 ADC pinlerini yapilandirir.
 *
 * spi_bus_init() cagrildiktan sonra cagrilmalidir (SCK pin hazir olmali).
 *
 * @param cs_gpio   CS pin numarasi (active low).
 * @param miso_gpio MISO (DOUT) pin numarasi (input).
 * @param mosi_gpio MOSI (DIN) pin numarasi (output).
 * @return ESP_OK veya GPIO yapilandirma hatasi.
 */
esp_err_t mcp3202_init(int cs_gpio, int miso_gpio, int mosi_gpio);

/**
 * @brief Belirtilen kanaldan 12-bit analog deger okur.
 *
 * @param channel 0 veya 1 (MCP3202 2 kanallidir).
 *                 Bu projede kanal 1 kullanilir (pcb_pinler.md).
 * @return 0-4095 arasi 12-bit deger. Hata durumunda 0 doner (loglanir).
 *
 * @note Bu fonksiyon ~17 us surer. SPI Mode 0 bit-bang ile calisir.
 *       Thread-safety: ayni anda birden fazla task cagirmamali (CS/SCK paylasimli).
 */
uint16_t mcp3202_read_channel(uint8_t channel);

#ifdef __cplusplus
}
#endif

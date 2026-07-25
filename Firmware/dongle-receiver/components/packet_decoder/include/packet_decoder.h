// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file packet_decoder.h
 * @brief C6'dan gelen ESP-NOW paketlerini cozme (5B/23B + bit-unpacking).
 *
 * C6'nin packet encoder'inin TERSI. Paket formati AYNI (kullanici onayli):
 *
 * 5-Byte Hizli Paket:
 *   Byte 0: packet_counter (u8)
 *   Byte 1: system_status (bit field)
 *   Byte 2-3: digital_buttons (u16 LE, 12-bit)
 *   Byte 4: encoder_delta (i8 signed)
 *
 * 23-Byte Tam Paket:
 *   Byte 0-4: 5-byte hizli paket
 *   Byte 5-22: 18 byte analog (12x12-bit LE bit-packing)
 *
 * system_status bit haritasi:
 *   bit 0-1: active_power_mode (C6'dan)
 *   bit 2  : SW2 (layer btn)
 *   bit 3  : ENC_BTN
 *   bit 4  : flag_15
 *   bit 5  : battery_dead
 *   bit 6  : charging
 *   bit 7  : charge_complete
 *
 * Analog bit-unpacking (little-endian):
 *   12 kanal x 12-bit. kanal0 = bit0-11, ..., kanal11 = bit132-143.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "system_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Paket boyutlari (C6 ile ayni). */
#define PKT_DEC_SIZE_FAST          5
#define PKT_DEC_SIZE_FULL          23
#define PKT_DEC_ANALOG_CHANNELS    12
#define PKT_DEC_ANALOG_PACKED_SIZE 18

/**
 * @brief 5-byte hizli paketi cozer.
 * @param data     Gelen paket (en az 5 byte).
 * @param counter  Cikis: paket sayaci.
 * @param status   Cikis: system_status byte.
 * @param buttons  Cikis: 12-bit dijital tus durumu.
 * @param delta    Cikis: encoder delta (signed).
 * @return ESP_OK veya ESP_ERR_INVALID_ARG.
 */
esp_err_t packet_decode_fast(const uint8_t *data,
                             uint8_t *counter,
                             uint8_t *status,
                             uint16_t *buttons,
                             int8_t *delta);

/**
 * @brief 18 byte analog bit-packed veriyi 12 kanala cozer (LE bit-unpacking).
 * @param data   Gelen 18 byte analog veri.
 * @param analog Cikis: 12 kanal ADC degeri (0-4095).
 * @return ESP_OK veya ESP_ERR_INVALID_ARG.
 */
esp_err_t packet_decode_analog(const uint8_t *data,
                               uint16_t analog[PKT_DEC_ANALOG_CHANNELS]);

/**
 * @brief 23-byte tam paketi cozer (5B hizli + 18B analog).
 * @return ESP_OK veya ESP_ERR_INVALID_ARG.
 */
esp_err_t packet_decode_full(const uint8_t *data,
                             uint8_t *counter,
                             uint8_t *status,
                             uint16_t *buttons,
                             int8_t *delta,
                             uint16_t analog[PKT_DEC_ANALOG_CHANNELS]);

/**
 * @brief system_status byte'ini bit'lere ayirir.
 */
void packet_parse_status(uint8_t status,
                         power_mode_t *mode,
                         bool *sw2_pressed,
                         bool *enc_btn_pressed,
                         bool *flag_15,
                         bool *battery_dead,
                         bool *charging,
                         bool *charge_complete);

#ifdef __cplusplus
}
#endif

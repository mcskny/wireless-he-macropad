// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file packet_decoder.c
 * @brief C6 paket cozme + bit-unpacking implementasyonu.
 */

#include "packet_decoder.h"

#include <string.h>

esp_err_t packet_decode_fast(const uint8_t *data,
                             uint8_t *counter,
                             uint8_t *status,
                             uint16_t *buttons,
                             int8_t *delta)
{
    if (data == NULL || counter == NULL || status == NULL
        || buttons == NULL || delta == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *counter = data[0];
    *status  = data[1];
    /* u16 little-endian: low + (high << 8), 12-bit mask */
    *buttons = (uint16_t)(data[2] | ((uint16_t)(data[3] & 0x0Fu) << 8));
    *delta   = (int8_t)data[4];

    return ESP_OK;
}

esp_err_t packet_decode_analog(const uint8_t *data,
                               uint16_t analog[PKT_DEC_ANALOG_CHANNELS])
{
    if (data == NULL || analog == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Bit-by-bit little-endian unpacking (C6 encoder'in tersi):
     * kanal ch'nin 12 biti, bit_offset = ch * 12'den itibaren okunur. */
    int bit_pos = 0;
    for (int ch = 0; ch < PKT_DEC_ANALOG_CHANNELS; ch++) {
        uint16_t val = 0;
        for (int b = 0; b < 12; b++) {
            bool bit = (data[bit_pos / 8] >> (bit_pos % 8)) & 1u;
            if (bit) {
                val |= (uint16_t)(1u << b);
            }
            bit_pos++;
        }
        analog[ch] = val;
    }

    return ESP_OK;
}

esp_err_t packet_decode_full(const uint8_t *data,
                             uint8_t *counter,
                             uint8_t *status,
                             uint16_t *buttons,
                             int8_t *delta,
                             uint16_t analog[PKT_DEC_ANALOG_CHANNELS])
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Ilk 5 byte: hizli paket */
    esp_err_t ret = packet_decode_fast(data, counter, status, buttons, delta);
    if (ret != ESP_OK) {
        return ret;
    }

    /* Byte 5-22: analog bit-unpacking */
    if (analog != NULL) {
        ret = packet_decode_analog(data + PKT_DEC_SIZE_FAST, analog);
    }

    return ret;
}

void packet_parse_status(uint8_t status,
                         power_mode_t *mode,
                         bool *sw2_pressed,
                         bool *enc_btn_pressed,
                         bool *flag_15,
                         bool *battery_dead,
                         bool *charging,
                         bool *charge_complete)
{
    if (mode != NULL) {
        *mode = (power_mode_t)(status & 0x03u);
    }
    if (sw2_pressed != NULL) {
        *sw2_pressed = (status & (1u << 2)) != 0;
    }
    if (enc_btn_pressed != NULL) {
        *enc_btn_pressed = (status & (1u << 3)) != 0;
    }
    if (flag_15 != NULL) {
        *flag_15 = (status & (1u << 4)) != 0;
    }
    if (battery_dead != NULL) {
        *battery_dead = (status & (1u << 5)) != 0;
    }
    if (charging != NULL) {
        *charging = (status & (1u << 6)) != 0;
    }
    if (charge_complete != NULL) {
        *charge_complete = (status & (1u << 7)) != 0;
    }
}

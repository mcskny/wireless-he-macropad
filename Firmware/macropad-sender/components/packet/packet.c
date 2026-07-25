// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file packet.c
 * @brief ESP-NOW paket olusturma + bit-packing implementasyonu.
 */

#include "packet.h"

#include <string.h>

uint8_t packet_make_status(power_mode_t mode,
                           bool sw2_pressed,
                           bool enc_btn_pressed,
                           bool flag_15,
                           bool battery_dead,
                           bool charging,
                           bool charge_complete)
{
    uint8_t status = 0;
    /* bit 0-1: power_mode (0-2, mask 0x03) */
    status |= (uint8_t)((mode & 0x03u) << 0);
    /* bit 2: SW2 */
    if (sw2_pressed)         status |= (1u << 2);
    /* bit 3: ENC_BTN */
    if (enc_btn_pressed)     status |= (1u << 3);
    /* bit 4: flag_15 */
    if (flag_15)             status |= (1u << 4);
    /* bit 5: battery_dead */
    if (battery_dead)        status |= (1u << 5);
    /* bit 6: charging */
    if (charging)            status |= (1u << 6);
    /* bit 7: charge_complete */
    if (charge_complete)     status |= (1u << 7);
    return status;
}

uint8_t packet_build_fast(uint8_t *out,
                          uint8_t counter,
                          uint8_t status,
                          uint16_t digital_buttons,
                          int8_t encoder_delta)
{
    if (out == NULL) {
        return 0;
    }

    /* Byte 0: packet_counter */
    out[0] = counter;

    /* Byte 1: system_status */
    out[1] = status;

    /* Byte 2-3: digital_buttons (u16 little-endian, 12-bit kullanım) */
    out[2] = (uint8_t)(digital_buttons & 0xFFu);              /* low byte */
    out[3] = (uint8_t)((digital_buttons >> 8) & 0x0Fu);       /* high byte, 12-bit mask */

    /* Byte 4: encoder_delta (i8 signed) */
    out[4] = (uint8_t)encoder_delta;

    return PACKET_SIZE_FAST;
}

uint8_t packet_pack_analog(uint8_t *out, const uint16_t analog[PACKET_ANALOG_CHANNELS])
{
    if (out == NULL || analog == NULL) {
        return 0;
    }

    /* 18 byte sifirla */
    memset(out, 0, PACKET_ANALOG_PACKED_SIZE);

    /* Bit-by-bit little-endian packing:
     * kanal ch'nin 12 biti, bit_offset = ch * 12'den itibaren.
     * Her bit: out[bit_pos/8]'in (bit_pos%8). bitine yazilir. */
    int bit_pos = 0;
    for (int ch = 0; ch < PACKET_ANALOG_CHANNELS; ch++) {
        uint16_t val = analog[ch] & 0x0FFFu;  /* 12-bit mask */
        for (int b = 0; b < 12; b++) {
            if ((val >> b) & 1u) {
                out[bit_pos / 8] |= (uint8_t)(1u << (bit_pos % 8));
            }
            bit_pos++;
        }
    }

    return PACKET_ANALOG_PACKED_SIZE;
}

uint8_t packet_build_full(uint8_t *out,
                          uint8_t counter,
                          uint8_t status,
                          uint16_t digital_buttons,
                          int8_t encoder_delta,
                          const uint16_t analog[PACKET_ANALOG_CHANNELS])
{
    if (out == NULL) {
        return 0;
    }

    /* Ilk 5 byte: hizli paket */
    packet_build_fast(out, counter, status, digital_buttons, encoder_delta);

    /* Byte 5-22: analog bit-packing */
    if (analog != NULL) {
        packet_pack_analog(out + PACKET_SIZE_FAST, analog);
    } else {
        /* analog NULL ise 18 byte sifir */
        memset(out + PACKET_SIZE_FAST, 0, PACKET_ANALOG_PACKED_SIZE);
    }

    return PACKET_SIZE_FULL;
}

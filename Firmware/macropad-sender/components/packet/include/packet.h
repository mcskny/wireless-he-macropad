// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file packet.h
 * @brief ESP-NOW paket olusturma: 5B hizli + 23B tam paket + bit-packing.
 *
 * Paket formati (kullanici onayli, Bölüm 3.2):
 *
 * 5-Byte Hizli Paket:
 *   Byte 0: packet_counter (u8, 0-255 wrap)
 *   Byte 1: system_status (bit field, asagida)
 *   Byte 2-3: digital_buttons (u16 LE, bit0=Tus1..bit11=Tus12, bit12-15 reserved=0)
 *   Byte 4: encoder_delta (i8 signed, -128..+127)
 *
 * 23-Byte Tam Paket:
 *   Byte 0-4: 5-byte hizli paket
 *   Byte 5-22: 18 byte sikistirilmis analog (12 kanal x 12-bit, LE bit-packing)
 *
 * system_status bit haritasi:
 *   bit 0-1: active_power_mode (0=Dijital, 1=Hibrit, 2=Agresif)
 *   bit 2  : SW2 (layer btn) basili (1=basildi)
 *   bit 3  : ENC_BTN basili
 *   bit 4  : flag_15 (dusuk pil uyari)
 *   bit 5  : battery_dead (kritik kilitle)
 *   bit 6  : charging (VBUS algilandi)
 *   bit 7  : charge_complete (/CHG == HIGH)
 *
 * Analog bit-packing (little-endian):
 *   12 kanal, her 12-bit (0-4095), kanal 0'dan 11'e sirali.
 *   kanal0 = bit0-11, kanal1 = bit12-23, ..., kanal11 = bit132-143.
 *   byte0 = bit0-7, byte1 = bit8-15, ... byte17 = bit136-143.
 *   Kanal adres sirasiyla paketlenir (mux adres 0 = paket kanal 0).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "system_state.h"  /* power_mode_t */

#ifdef __cplusplus
extern "C" {
#endif

/** Paket boyutlari. */
#define PACKET_SIZE_FAST          5
#define PACKET_SIZE_FULL          23
#define PACKET_ANALOG_CHANNELS    12
#define PACKET_ANALOG_PACKED_SIZE 18  /* 12 * 12 bit = 144 bit = 18 byte */

/**
 * @brief system_status byte'ini bit haritasina göre olusturur.
 *
 * @param mode            Aktif guc modu (bit 0-1).
 * @param sw2_pressed     SW2 (layer btn) basili mi (bit 2).
 * @param enc_btn_pressed ENC_BTN basili mi (bit 3).
 * @param flag_15         Pil <%15 bayragi (bit 4).
 * @param battery_dead    Kritik pil kilidi (bit 5).
 * @param charging        VBUS/sarj aktif (bit 6).
 * @param charge_complete Sarj tamamlandi /CHG HIGH (bit 7).
 * @return system_status byte.
 */
uint8_t packet_make_status(power_mode_t mode,
                           bool sw2_pressed,
                           bool enc_btn_pressed,
                           bool flag_15,
                           bool battery_dead,
                           bool charging,
                           bool charge_complete);

/**
 * @brief 5-byte hizli paket olusturur (Bölüm 3.2).
 *
 * @param out            Cikis buffer (en az PACKET_SIZE_FAST byte).
 * @param counter        Paket sayaci (0-255 wrap).
 * @param status         system_status byte (packet_make_status'tan).
 * @param digital_buttons 12-bit dijital tus durumu (bit0=Tus1..bit11=Tus12).
 * @param encoder_delta  Encoder delta (signed).
 * @return Yazilan byte sayisi (5).
 */
uint8_t packet_build_fast(uint8_t *out,
                          uint8_t counter,
                          uint8_t status,
                          uint16_t digital_buttons,
                          int8_t encoder_delta);

/**
 * @brief 12 kanal analog veriyi 18 byte'a bit-pack eder (little-endian).
 *
 * @param out    Cikis buffer (en az PACKET_ANALOG_PACKED_SIZE byte).
 * @param analog 12 kanal ADC degeri (her biri 0-4095, 12-bit mask uygulanir).
 * @return Yazilan byte sayisi (18).
 */
uint8_t packet_pack_analog(uint8_t *out, const uint16_t analog[PACKET_ANALOG_CHANNELS]);

/**
 * @brief 23-byte tam paket olusturur (5B hizli + 18B analog).
 *
 * @param out            Cikis buffer (en az PACKET_SIZE_FULL byte).
 * @param counter        Paket sayaci.
 * @param status         system_status byte.
 * @param digital_buttons Dijital tus durumu.
 * @param encoder_delta  Encoder delta.
 * @param analog         12 kanal analog veri (NULL ise analog kismi sifir yazilir).
 * @return Yazilan byte sayisi (23).
 */
uint8_t packet_build_full(uint8_t *out,
                          uint8_t counter,
                          uint8_t status,
                          uint16_t digital_buttons,
                          int8_t encoder_delta,
                          const uint16_t analog[PACKET_ANALOG_CHANNELS]);

#ifdef __cplusplus
}
#endif

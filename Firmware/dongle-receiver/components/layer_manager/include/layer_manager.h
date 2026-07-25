// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file layer_manager.h
 * @brief Katman secimi + tuş esleme (Bölüm 5.1).
 *
 * C6 paketinden gelen SW2 (layer_button) durumuna gore katman secilir:
 *   - SW2 BASILI   → layer 1
 *   - SW2 BIRAKILDI → layer 0
 *
 * @note Algoritma 2 katman (0/1) ima ediyor ama KATMAN_SAYISI=4 (C6'da 4 LED).
 *       Katman 2-3 key_mappings'te tanimli ama SW2 ile erisilemez (simdilik).
 *       Web config ile farkli katman secimi sonraya.
 *
 * Tuş esleme: key_mappings[layer][key_index] → USB HID keycode.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Katman yoneticisini baslatir (current_layer = 0).
 */
void layer_manager_init(void);

/**
 * @brief SW2 durumuna gore aktif katmani gunceller (Bölüm 5.1).
 * @param sw2_pressed SW2 (layer_button) basili mi?
 * @return Aktif katman indeksi (0 veya 1).
 */
 uint8_t layer_manager_update(bool sw2_pressed, uint32_t now_ms);
/**
 * @brief Aktif katmani doner.
 */
uint8_t layer_manager_get_current(void);

/**
 * @brief Belirtilen katman + tuş icin USB HID keycode doner.
 * @param layer        Katman indeksi (0-3).
 * @param key_index    Tuş indeksi (0-11).
 * @param key_mappings 4×12 keycode tablosu (NVS'den).
 * @return HID keycode (0 = eslenmemis/tuş kapali).
 */
uint16_t layer_manager_get_keycode(uint8_t layer, uint8_t key_index,
                                   const uint16_t key_mappings[4][12]);

#ifdef __cplusplus
}
#endif

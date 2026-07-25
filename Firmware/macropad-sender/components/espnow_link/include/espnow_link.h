// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file espnow_link.h
 * @brief ESP-NOW S3 haberlesme + fail-safe baglanti sagligi (Bölüm 2.2).
 *
 * Sorumluluklar:
 *   1. Wi-Fi + ESP-NOW baslatma, S3 peer kaydi.
 *   2. Paket gonderimi (esp_now_send).
 *   3. Tx callback: basarili/basarisiz sayaci.
 *   4. Fail-safe: 100 ardisik basarisiz gonderimde arama moduna dus
 *      (is_connected=false, search_mode=true). main.c bu durumda 1 Hz'e iner.
 *
 * Thread-safety: Tx callback wifi task context'inde calisir. Shared state
 * (failed count, is_connected, search_mode) portMUX critical section ile
 * korunur (AI_GUIDELINES.md kural 17).
 *
 * @note S3 MAC adresi + Wi-Fi kanali menuconfig'ten (CONFIG_S3_PEER_MAC,
 *       CONFIG_ESPNOW_CHANNEL) main.c tarafindan saglanir.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Wi-Fi + ESP-NOW baslatir, S3 peer ekler, Tx callback tanimlar.
 *
 * @param peer_mac           S3 MAC adresi (6 byte).
 * @param channel            Wi-Fi kanali (1-13).
 * @param failsafe_threshold Arama moduna gecis icin ardisik basarisiz paket
 *                           sayisi (Bölüm 2.2, default 100).
 * @return ESP_OK veya Wi-Fi/ESP-NOW hatasi.
 */
esp_err_t espnow_link_init(const uint8_t peer_mac[6],
                           uint8_t channel,
                           uint32_t failsafe_threshold);

/**
 * @brief Paketi S3'e gonderir (esp_now_send).
 *
 * @param data Paket verisi.
 * @param len  Paket boyutu (byte).
 * @return ESP_OK (gonderim kuyruga alindi) veya hata.
 * @note Geri bildirim asenkron: Tx callback durum gunceller. Basari durumu
 *       espnow_link_is_connected() ile sorgulanir.
 */
esp_err_t espnow_link_send(const uint8_t *data, size_t len);

/**
 * @brief S3'e bagli mi? (son gonderimde ACK alindi mi?).
 * @return true = bagli, false = bagli degil (arama modu).
 */
bool espnow_link_is_connected(void);

/**
 * @brief Arama modunda mi? (fail-safe tetiklendi, 1 Hz'e dusuldu).
 * @return true = arama modu (dusuk hiz), false = normal.
 */
bool espnow_link_is_search_mode(void);

/**
 * @brief Ardisik basarisiz paket sayisi.
 * @return 0 = son gonderim basarili, >=threshold = arama modu.
 */
uint32_t espnow_link_get_failed_count(void);

/**
 * @brief Baglanti durumunu sifirlar (test/manuel kurtarma icin).
 *        consecutive_failed=0, is_connected=true, search_mode=false.
 */
void espnow_link_reset(void);

#ifdef __cplusplus
}
#endif

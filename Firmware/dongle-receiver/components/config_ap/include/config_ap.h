// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file config_ap.h
 * @brief Wi-Fi AP + WebSocket server (Bölüm 2 CONFIG_AP_MODE).
 *
 * S3 dongle config modunda Wi-Fi AP acar, WebSocket server ile kalibrasyon
 * yapilir. Gerçek zamanli ADC akisi WebSocket frame olarak gonderilir.
 *
 * Komutlar (JSON, WebSocket text frame):
 *   - {"cmd":"get_calib"} → kalibrasyon JSON doner
 *   - {"cmd":"set_calib","data":{...}} → NVS'e yaz
 *   - {"cmd":"get_adc"} → 12 kanal ADC akisi baslar
 *   - {"cmd":"stop_adc"} → ADC akisi durur
 *   - {"cmd":"exit"} → CONFIG_AP_MODE'dan cik, NORMAL_MODE
 *
 * @note STUB: WebSocket handler simdilik basit. Tam JSON parse + kalibrasyon
 *       yazma F5 sonrasi veya ayri fazda. esp_http_server httpd_ws kullanilir.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "system_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Wi-Fi AP + WebSocket server baslatir (CONFIG_AP_MODE).
 * @param ssid     AP SSID.
 * @param password AP sifre (bos = acik).
 * @param channel  Wi-Fi kanali.
 * @return ESP_OK veya hata.
 */
esp_err_t config_ap_start(const char *ssid, const char *password, uint8_t channel);

/**
 * @brief AP + WebSocket server durdur, NORMAL_MODE'a donus.
 * @return ESP_OK veya hata.
 */
esp_err_t config_ap_stop(void);

/**
 * @brief AP mode aktif mi?
 */
bool config_ap_is_active(void);

/**
 * @brief WebSocket istemcilerine 12 kanal ADC gonder (gerçek zamanli akis).
 * @param analog 12 kanal ADC degeri.
 * @return ESP_OK veya hata (istemci yoksa ESP_ERR_NOT_FOUND).
 */
esp_err_t config_ap_send_adc(const uint16_t analog[12]);

/**
 * @brief WebSocket'den cikis komutu geldi mi? (config mode → normal)
 * @return true = cikis istegi alindi.
 */
bool config_ap_exit_requested(void);

#ifdef __cplusplus
}
#endif

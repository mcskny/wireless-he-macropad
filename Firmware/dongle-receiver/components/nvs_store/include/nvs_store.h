// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file nvs_store.h
 * @brief NVS kalibrasyon blob + generic anahtar-deger saklama.
 *
 * S3 dongle'in kalibrasyon verisi (min/max_adc, actuation, RT, key_mappings,
 * snap_tap, joystick) NVS'de tek blob olarak saklanir. Default degerler
 * nvs_store.c icinde tanimli; web kalibrasyon NVS'e yazar.
 *
 * NVS namespace: "s3d" (s3 dongle).
 *
 * @note system_state.h'ye bagimli (calibration_config_t). C6'daki generic
 *       wrapper'dan farkli: kalibrasyon blob ozel fonksiyon.
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
 * @brief NVS alt sistemini baslatir (nvs_flash_init + namespace ac).
 * @return ESP_OK veya hata.
 */
esp_err_t nvs_store_init(void);

/**
 * @brief Kalibrasyon yapilandirmasini NVS'den yukler.
 *
 * NVS'de kayit yoksa default degerleri (nvs_store.c icinde) kopyalar.
 *
 * @param out Yuklenen kalibrasyon (cagirirsa tarafindan ayrilmis buffer).
 * @return ESP_OK; anahtar yoksa yine ESP_OK + default degerler.
 */
esp_err_t nvs_store_load_calibration(calibration_config_t *out);

/**
 * @brief Kalibrasyon yapilandirmasini NVS'ye kaydeder (blob).
 * @return ESP_OK veya NVS yazma hatasi.
 */
esp_err_t nvs_store_save_calibration(const calibration_config_t *cfg);

/**
 * @brief Kalibrasyonu default degerlere sifirlar (NVS'ten siler + default yukler).
 * @param out Default degerlerin yazildigi buffer.
 * @return ESP_OK veya hata.
 */
esp_err_t nvs_store_reset_calibration(calibration_config_t *out);

#ifdef __cplusplus
}
#endif

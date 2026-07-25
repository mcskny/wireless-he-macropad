// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file nvs_store.h
 * @brief NVS uzerinde generic (tip-bagimsiz) anahtar-deger saklama katmani.
 *
 * Bu bilesen persistent_config_t veya system_state.h tiplerine bagimli DEGILDIR.
 * Amac: NVS handle yonetimini ve hata kontrolunu tek yerde toplamak; ust katman
 * (main.c) bu wrapper uzerinden active_power_mode / base_brightness / active_layer
 * gibi kalici degerleri okuyup yazmasi.
 *
 * Kullanim:
 *   nvs_store_init();                                  // bir kez app_main'de
 *   uint8_t mode = 0;
 *   nvs_store_get_u8("pw_mode", 1, &mode);             // yoksa default=1
 *   nvs_store_set_u8("pw_mode", 2);                    // kaydet
 *
 * NVS namespace: "mpd" (macropad). Tum anahtarlar bu namespace altindadir.
 *
 * Thread-safety: NVS API kendi icinde kilitliitr; bu wrapper ek kilitleme yapmaz.
 * Ancak ayni handle uzerinde ayni anda birden fazla task yazma yaparsa son-yazan
 * kazanir. Pil/critical-path verileri icin tek task yazmasi onerilir.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief NVS alt sistemini baslatir.
 *
 * nvs_flash_init() cagirir; eger flash korrupt/bos ise bir kez erase + init
 * yapar. Basariyla acildiginda "mpd" namespace'ini NVS_READWRITE modunda acar
 * ve handle dosya-lokal static olarak saklar. app_main'de BIR KEZ cagrilmalidir.
 *
 * @return ESP_OK basarili; aksi halde nvs_flash_init/nvs_open hata kodu.
 */
esp_err_t nvs_store_init(void);

/**
 * @brief uint8_t deger okur; anahtar yoksa default degeri doner.
 * @param key    NVS anahtari (NULL-terminated string, max 15 char).
 * @param def    Anahtar yoksa donulecek varsayilan deger.
 * @param out    Okunan degerin yazildigi cikis bufferi (NULL olamaz).
 * @return ESP_OK; anahtar yoksa yine ESP_OK doner ve *out = def olur.
 *         Hata: ESP_ERR_INVALID_ARG (out/key NULL) veya NVS okuma hatasi.
 */
esp_err_t nvs_store_get_u8(const char *key, uint8_t def, uint8_t *out);

/**
 * @brief uint8_t deger kaydeder ve commit eder.
 * @param key   NVS anahtari (max 15 char).
 * @param value Yazilacak deger.
 * @return ESP_OK veya NVS yazma hatasi.
 */
esp_err_t nvs_store_set_u8(const char *key, uint8_t value);

/**
 * @brief uint32_t deger okur; anahtar yoksa default degeri doner.
 * @param key NVS anahtari.
 * @param def Anahtar yoksa donulecek varsayilan deger.
 * @param out Cikis bufferi (NULL olamaz).
 * @return ESP_OK veya hata.
 */
esp_err_t nvs_store_get_u32(const char *key, uint32_t def, uint32_t *out);

/**
 * @brief uint32_t deger kaydeder ve commit eder.
 * @return ESP_OK veya NVS yazma hatasi.
 */
esp_err_t nvs_store_set_u32(const char *key, uint32_t value);

/**
 * @brief bool deger okur; anahtar yoksa default degeri doner.
 *
 * NVS'de bool olarak saklanmaz, uint8_t (0/1) olarak saklanir.
 * @return ESP_OK veya hata.
 */
esp_err_t nvs_store_get_bool(const char *key, bool def, bool *out);

/**
 * @brief bool deger kaydeder (uint8_t olarak) ve commit eder.
 * @return ESP_OK veya NVS yazma hatasi.
 */
esp_err_t nvs_store_set_bool(const char *key, bool value);

#ifdef __cplusplus
}
#endif

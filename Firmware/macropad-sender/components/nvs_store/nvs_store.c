// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file nvs_store.c
 * @brief NVS generic wrapper implementasyonu.
 */

#include "nvs_store.h"

#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "nvs_store";

/** NVS namespace: "mpd" = macropad. Tum anahtarlar bu altinda. */
#define NVS_STORE_NAMESPACE "mpd"

/**
 * @brief Dosya-lokal NVS handle.
 *
 * nvs_store_init() tarafindan acilir, sistem calisma suresince acik kalir.
 * Baska .c dosyasi bu degiskene erisemez; erisim yalnizca bu dosyadaki
 * fonksiyonlar uzerinden olur (AI_GUIDELINES.md kural 16: static file-local).
 */
static nvs_handle_t s_nvs_handle = 0;

/** Handle acik mi? (init cagrildi mi?) */
static bool s_initialized = false;

esp_err_t nvs_store_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS corrupt/bos, erase + yeniden init yapiliyor");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nvs_open(NVS_STORE_NAMESPACE, NVS_READWRITE, &s_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS namespace '%s' acilamadi: %s",
                 NVS_STORE_NAMESPACE, esp_err_to_name(ret));
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "NVS baslatildi (namespace='%s')", NVS_STORE_NAMESPACE);
    return ESP_OK;
}

esp_err_t nvs_store_get_u8(const char *key, uint8_t def, uint8_t *out)
{
    if (!s_initialized || key == NULL || out == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = nvs_get_u8(s_nvs_handle, key, out);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        *out = def;
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "get_u8('%s') hata: %s", key, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t nvs_store_set_u8(const char *key, uint8_t value)
{
    if (!s_initialized || key == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = nvs_set_u8(s_nvs_handle, key, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "set_u8('%s') hata: %s", key, esp_err_to_name(ret));
        return ret;
    }
    return nvs_commit(s_nvs_handle);
}

esp_err_t nvs_store_get_u32(const char *key, uint32_t def, uint32_t *out)
{
    if (!s_initialized || key == NULL || out == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = nvs_get_u32(s_nvs_handle, key, out);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        *out = def;
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "get_u32('%s') hata: %s", key, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t nvs_store_set_u32(const char *key, uint32_t value)
{
    if (!s_initialized || key == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = nvs_set_u32(s_nvs_handle, key, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "set_u32('%s') hata: %s", key, esp_err_to_name(ret));
        return ret;
    }
    return nvs_commit(s_nvs_handle);
}

esp_err_t nvs_store_get_bool(const char *key, bool def, bool *out)
{
    if (!s_initialized || key == NULL || out == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t v = 0;
    esp_err_t ret = nvs_get_u8(s_nvs_handle, key, &v);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        *out = def;
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "get_bool('%s') hata: %s", key, esp_err_to_name(ret));
        return ret;
    }
    *out = (v != 0);
    return ESP_OK;
}

esp_err_t nvs_store_set_bool(const char *key, bool value)
{
    return nvs_store_set_u8(key, value ? 1u : 0u);
}

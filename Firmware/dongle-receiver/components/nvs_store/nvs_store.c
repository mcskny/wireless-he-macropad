// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file nvs_store.c
 * @brief NVS kalibrasyon blob implementasyonu + default degerler.
 */

#include "nvs_store.h"

#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "nvs_store";

/** NVS namespace. */
#define NVS_STORE_NAMESPACE "s3d"

/** Kalibrasyon blob anahtari. */
#define NVS_KEY_CALIBRATION "calib"

/** File-local NVS handle. */
static nvs_handle_t s_handle = 0;
static bool s_initialized = false;

/**
 * @brief Default kalibrasyon degerleri (Bölüm 1.1).
 *
 * Ilk acilista veya reset'te kullanilir. Web kalibrasyon ile override edilir.
 * min/max ADC: tam aralik (0-4095), actuation %50, RT hassasiyet %10.
 * Key mappings: katman 0'da tus 0-11 = HID A-L (0x04-0x0F), diger katmanlar bos.
 */
static const calibration_config_t s_default_calibration = {
    .min_adc  = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    .max_adc  = {4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095, 4095},
    .actuation_point = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                        0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f},
    .rt_press_sensitivity = {0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f,
                             0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f},
    .rt_release_sensitivity = {0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f,
                               0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f},
    .trigger_mode = {
        TRIGGER_MODE_TRADITIONAL, TRIGGER_MODE_TRADITIONAL,
        TRIGGER_MODE_TRADITIONAL, TRIGGER_MODE_TRADITIONAL,
        TRIGGER_MODE_TRADITIONAL, TRIGGER_MODE_TRADITIONAL,
        TRIGGER_MODE_TRADITIONAL, TRIGGER_MODE_TRADITIONAL,
        TRIGGER_MODE_TRADITIONAL, TRIGGER_MODE_TRADITIONAL,
        TRIGGER_MODE_TRADITIONAL, TRIGGER_MODE_TRADITIONAL,
    },
    /* Katman 0: tus 0-11 = HID A,B,C,D,E,F,G,H,I,J,K,L (0x04-0x0F).
       Katman 1-3: bos (0). Web'den duzenlenir. */
    .key_mappings = {
        {0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    },
    .snap_tap_pairs = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},  /* 0xFF = bos */
    .joystick_axes = {0xFF, 0xFF, 0xFF, 0xFF},  /* 0xFF = bos */
};

esp_err_t nvs_store_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS corrupt, erase + init");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nvs_open(NVS_STORE_NAMESPACE, NVS_READWRITE, &s_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS namespace '%s' acilamadi: %s",
                 NVS_STORE_NAMESPACE, esp_err_to_name(ret));
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "NVS baslatildi (namespace='%s')", NVS_STORE_NAMESPACE);
    return ESP_OK;
}

esp_err_t nvs_store_load_calibration(calibration_config_t *out)
{
    if (!s_initialized || out == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Default degerleri once kopyala (fallback) */
    memcpy(out, &s_default_calibration, sizeof(calibration_config_t));

    /* Blob oku; yoksa default kalir */
    size_t required_size = sizeof(calibration_config_t);
    esp_err_t ret = nvs_get_blob(s_handle, NVS_KEY_CALIBRATION, out, &required_size);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Kalibrasyon NVS'de yok, default yuklendi");
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_get_blob hatasi: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Kalibrasyon NVS'den yuklendi (%u byte)",
             (unsigned)required_size);
    return ESP_OK;
}

esp_err_t nvs_store_save_calibration(const calibration_config_t *cfg)
{
    if (!s_initialized || cfg == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = nvs_set_blob(s_handle, NVS_KEY_CALIBRATION, cfg,
                                 sizeof(calibration_config_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_blob hatasi: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_commit(s_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit hatasi: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Kalibrasyon NVS'ye kaydedildi (%u byte)",
             (unsigned)sizeof(calibration_config_t));
    return ESP_OK;
}

esp_err_t nvs_store_reset_calibration(calibration_config_t *out)
{
    if (!s_initialized || out == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* NVS'ten sil */
    esp_err_t ret = nvs_erase_key(s_handle, NVS_KEY_CALIBRATION);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "nvs_erase_key hatasi: %s", esp_err_to_name(ret));
        return ret;
    }
    nvs_commit(s_handle);

    /* Default kopyala */
    memcpy(out, &s_default_calibration, sizeof(calibration_config_t));
    ESP_LOGI(TAG, "Kalibrasyon default'a sifirlandi");
    return ESP_OK;
}

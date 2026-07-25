// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file mux_cd74hc4067.c
 * @brief CD74HC4067 16-kanal analog mux implementasyonu.
 */

#include "mux_cd74hc4067.h"

#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "mux";

/** Adres pinleri (init'de set edilir). */
static int s_s0_gpio = -1;
static int s_s1_gpio = -1;
static int s_s2_gpio = -1;
static int s_s3_gpio = -1;

static bool s_initialized = false;

esp_err_t mux_cd74hc4067_init(int s0_gpio, int s1_gpio, int s2_gpio, int s3_gpio)
{
    if (s_initialized) {
        return ESP_OK;
    }
    if (s0_gpio < 0 || s1_gpio < 0 || s2_gpio < 0 || s3_gpio < 0) {
        ESP_LOGE(TAG, "Gecersiz pin: s0=%d s1=%d s2=%d s3=%d",
                 s0_gpio, s1_gpio, s2_gpio, s3_gpio);
        return ESP_ERR_INVALID_ARG;
    }

    s_s0_gpio = s0_gpio;
    s_s1_gpio = s1_gpio;
    s_s2_gpio = s2_gpio;
    s_s3_gpio = s3_gpio;

    /* 4 adres pinini output olarak yapilandir */
    uint64_t mask = (1ULL << s0_gpio) | (1ULL << s1_gpio)
                  | (1ULL << s2_gpio) | (1ULL << s3_gpio);
    gpio_config_t io_cfg = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config hatasi: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Idle: kanal 0 */
    mux_cd74hc4067_select(0);

    s_initialized = true;
    ESP_LOGI(TAG, "CD74HC4067 baslatildi: S0=GPIO%d S1=GPIO%d S2=GPIO%d S3=GPIO%d",
             s0_gpio, s1_gpio, s2_gpio, s3_gpio);
    return ESP_OK;
}

esp_err_t mux_cd74hc4067_select(uint8_t channel)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (channel >= MUX_CD74HC4067_CHANNEL_COUNT) {
        ESP_LOGE(TAG, "Gecersiz kanal: %u (0-%d arasi olmali)",
                 channel, MUX_CD74HC4067_CHANNEL_COUNT - 1);
        return ESP_ERR_INVALID_ARG;
    }

    /* S0 = bit0, S1 = bit1, S2 = bit2, S3 = bit3 */
    gpio_set_level(s_s0_gpio, (channel >> 0) & 1u);
    gpio_set_level(s_s1_gpio, (channel >> 1) & 1u);
    gpio_set_level(s_s2_gpio, (channel >> 2) & 1u);
    gpio_set_level(s_s3_gpio, (channel >> 3) & 1u);

    return ESP_OK;
}

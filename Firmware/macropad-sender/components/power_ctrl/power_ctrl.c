// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file power_ctrl.c
 * @brief Guc kontrolu implementasyonu: MOSFET + parlaklik + mod geri bildirimi.
 */

#include "power_ctrl.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "neopixel.h"

static const char *TAG = "power";

/** Pin numaralari (init'de set). */
static int s_analog_pwr_gpio = -1;
static int s_tps_en_gpio     = -1;

static bool s_initialized = false;

esp_err_t power_ctrl_init(int analog_pwr_gpio, int tps_en_gpio)
{
    if (s_initialized) {
        return ESP_OK;
    }
    if (analog_pwr_gpio < 0 || tps_en_gpio < 0) {
        ESP_LOGE(TAG, "Gecersiz pin: analog=%d tps=%d",
                 analog_pwr_gpio, tps_en_gpio);
        return ESP_ERR_INVALID_ARG;
    }

    s_analog_pwr_gpio = analog_pwr_gpio;
    s_tps_en_gpio     = tps_en_gpio;

    /* Iki MOSFET pin: output, idle LOW (guc kapali) */
    uint64_t mask = (1ULL << s_analog_pwr_gpio) | (1ULL << s_tps_en_gpio);
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

    /* Baslangicta her ikisi de KAPALI (Bölüm 1.2) */
    gpio_set_level(s_analog_pwr_gpio, 0);
    gpio_set_level(s_tps_en_gpio, 0);

    s_initialized = true;
    ESP_LOGI(TAG, "Power ctrl baslatildi: ANALOG_PWR=GPIO%d TPS_EN=GPIO%d",
             s_analog_pwr_gpio, s_tps_en_gpio);
    return ESP_OK;
}

void power_ctrl_analog_power(bool on)
{
    if (!s_initialized) return;
    gpio_set_level(s_analog_pwr_gpio, on ? 1 : 0);
}

void power_ctrl_led_power(bool on)
{
    if (!s_initialized) return;
    gpio_set_level(s_tps_en_gpio, on ? 1 : 0);
}

uint8_t power_ctrl_calc_brightness(power_mode_t mode,
                                   uint8_t base_brightness,
                                   bool flag_25)
{
    /* Mod bazli limit (Bölüm 4.5) */
    uint8_t mod_limit;
    switch (mode) {
        case POWER_MODE_DIGITAL:    mod_limit = 40; break;
        case POWER_MODE_HYBRID:     mod_limit = 60; break;
        case POWER_MODE_AGGRESSIVE: mod_limit = 80; break;
        default:                    mod_limit = 60; break;
    }

    /* effective = min(base, mod_limit) */
    uint8_t eff = (base_brightness < mod_limit) ? base_brightness : mod_limit;

    /* Pil <%25 ise %30 cap (Bölüm 4.4) */
    if (flag_25 && eff > 30) {
        eff = 30;
    }
    return eff;
}

void power_ctrl_apply_brightness(power_mode_t mode,
                                 uint8_t base_brightness,
                                 bool flag_25)
{
    uint8_t eff = power_ctrl_calc_brightness(mode, base_brightness, flag_25);
    neopixel_set_brightness(eff);
}

void power_ctrl_mode_feedback(power_mode_t mode,
                              uint8_t base_brightness,
                              bool flag_25)
{
    if (!s_initialized) {
        return;
    }

    /* 1. LED MOSFET ac */
    power_ctrl_led_power(true);

    /* 2. Effective brightness uygula */
    power_ctrl_apply_brightness(mode, base_brightness, flag_25);

    /* 3. Mod rengine göre 12 neopikseli boya */
    uint8_t r, g, b;
    switch (mode) {
        case POWER_MODE_DIGITAL:    r = 0;   g = 255; b = 0;   break; /* Yeşil */
        case POWER_MODE_HYBRID:     r = 0;   g = 0;   b = 255; break; /* Mavi  */
        case POWER_MODE_AGGRESSIVE: r = 255; g = 0;   b = 0;   break; /* Kırmızı */
        default:                    r = 0;   g = 0;   b = 255; break; /* Mavi  */
    }
    neopixel_fill(r, g, b);
    neopixel_refresh();

    /* 4. 3 saniye bekle (BLOKLAYICI - Bölüm 4.5) */
    vTaskDelay(pdMS_TO_TICKS(3000));

    /* 5. LED'leri söndür, MOSFET kapat */
    neopixel_clear();
    power_ctrl_led_power(false);

    ESP_LOGI(TAG, "Mod geri bildirimi: mode=%d", mode);
}

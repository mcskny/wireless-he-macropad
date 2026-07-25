// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file wakeup_manager.c
 * @brief Light/deep sleep + wakeup implementasyonu.
 */

#include "wakeup_manager.h"

#include "esp_log.h"
#include "esp_sleep.h"
#include "driver/gpio.h"

static const char *TAG = "wakeup";

/** Diot-OR pin numarasi. */
static int s_diot_or_gpio = -1;
/** Tuş polarity: true = active low (basili = LOW). */
static bool s_active_low = true;

static bool s_initialized = false;

esp_err_t wakeup_manager_init(int diot_or_gpio, bool active_low)
{
    if (s_initialized) {
        return ESP_OK;
    }
    if (diot_or_gpio < 0) {
        ESP_LOGE(TAG, "Gecersiz Diot-OR pin: %d", diot_or_gpio);
        return ESP_ERR_INVALID_ARG;
    }

    s_diot_or_gpio = diot_or_gpio;
    s_active_low   = active_low;

    /* GPIO input olarak yapilandir (light sleep GPIO wake icin).
       Pull-up ekle (active low ise tuş basilmadiğinda HIGH idle). */
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << s_diot_or_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en   = active_low ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = active_low ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config hatasi: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Light sleep GPIO wake: low level (active low) veya high level (active high) */
    gpio_int_type_t wake_level = active_low ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL;
    ret = gpio_wakeup_enable(s_diot_or_gpio, wake_level);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_wakeup_enable hatasi: %s", esp_err_to_name(ret));
        return ret;
    }
    esp_sleep_enable_gpio_wakeup();

    s_initialized = true;
    ESP_LOGI(TAG, "Wakeup manager baslatildi: Diot-OR=GPIO%d active_low=%d",
             s_diot_or_gpio, s_active_low);
    return ESP_OK;
}

esp_err_t wakeup_manager_light_sleep(uint32_t poll_period_ms)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "wakeup_manager_init cagrilmadi");
        return ESP_ERR_INVALID_STATE;
    }

    /* Timer wake (periyodik polling icin) */
    if (poll_period_ms > 0) {
        esp_sleep_enable_timer_wakeup((uint64_t)poll_period_ms * 1000u);
    }

    /* GPIO wake zaten init'de enable edildi */
    esp_err_t ret = esp_light_sleep_start();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "esp_light_sleep_start: %s", esp_err_to_name(ret));
    }
    return ret;
}

void wakeup_manager_deep_sleep(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "wakeup_manager_init cagrilmadi, deep sleep iptal");
        return;
    }

    /* Deep sleep ext1: RTC GPIO bitmask ile wakeup.
       ESP32-C6 ext0 desteklemez, ext1 kullanilir.
       active_low: tuş basili = LOW → ESP_EXT1_WAKEUP_ALL_LOW (tek pin LOW = hepsi LOW).
       active_high: tuş basili = HIGH → ESP_EXT1_WAKEUP_ANY_HIGH. */
    uint64_t mask = (1ULL << s_diot_or_gpio);  /* IO4 = RTC GPIO 4 varsayimi */
    esp_sleep_ext1_wakeup_mode_t mode = s_active_low
        ? ESP_EXT1_WAKEUP_ANY_LOW
        : ESP_EXT1_WAKEUP_ANY_HIGH;
    esp_err_t ret = esp_sleep_enable_ext1_wakeup(mask, mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ext1 wakeup hatasi: %s", esp_err_to_name(ret));
    }

    ESP_LOGW(TAG, "Deep sleep'e giriliyor (kritik kapanma, Bölüm 4.4)");
    esp_deep_sleep_start();
    /* DONMEZ - reset sonrasi app_main */
}

bool wakeup_manager_woken_by_gpio(void)
{
    /* ESP-IDF v6: esp_sleep_get_wakeup_cause deprecated, esp_sleep_get_wakeup_causes
       bitmask doner. Herhangi bir wakeup cause varsa true (GPIO veya timer).
       Ayrim main.c tarafindan polling ile yapilir (sensör oku her uyanista). */
    uint32_t causes = esp_sleep_get_wakeup_causes();
    return (causes != 0u);
}

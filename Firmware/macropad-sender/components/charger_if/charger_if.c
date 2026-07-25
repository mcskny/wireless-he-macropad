// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file charger_if.c
 * @brief Sarj sensör stub + animasyon implementasyonu (Bölüm 5).
 */

#include "charger_if.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "neopixel.h"
#include "power_ctrl.h"

static const char *TAG = "charger";

/** Pin numaralari (-1 = stub). */
static int s_vbus_gpio = -1;
static int s_chg_gpio  = -1;

static bool s_initialized = false;
/** Stub modunda mi? (pin -1). */
static bool s_stub_mode = true;

esp_err_t charger_if_init(int vbus_gpio, int chg_gpio)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_vbus_gpio = vbus_gpio;
    s_chg_gpio  = chg_gpio;

    /* Stub modu: herhangi biri -1 ise stub */
    s_stub_mode = (vbus_gpio < 0 || chg_gpio < 0);

    if (!s_stub_mode) {
        /* Gerçek pin: ikisi de input. VBUS pull-down, /CHG pull-up. */
        gpio_config_t io_cfg = {
            .pin_bit_mask = (1ULL << s_vbus_gpio) | (1ULL << s_chg_gpio),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        esp_err_t ret = gpio_config(&io_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "gpio_config hatasi: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Charger if baslatildi: %s (VBUS=%d CHG=%d)",
             s_stub_mode ? "STUB" : "GERCEK",
             s_vbus_gpio, s_chg_gpio);
    return ESP_OK;
}

bool charger_if_is_charging(void)
{
    if (!s_initialized || s_stub_mode) {
        return false;  /* Stub: sarj yok */
    }
    /* VBUS active high: HIGH = USB takili */
    return (gpio_get_level(s_vbus_gpio) != 0);
}

bool charger_if_is_charge_complete(void)
{
    if (!s_initialized || s_stub_mode) {
        return false;  /* Stub: sarj bitmedi */
    }
    /* /CHG active low: HIGH = sarj bitti (STAT pin) */
    return (gpio_get_level(s_chg_gpio) != 0);
}

void charger_if_clear_battery_flags(battery_flags_t *flags)
{
    if (flags == NULL) {
        return;
    }
    flags->flag_50 = false;
    flags->flag_25 = false;
    flags->flag_15 = false;
    flags->flag_5  = false;
    ESP_LOGI(TAG, "Pil flag'leri sifirlandi (sarja takildi)");
}

/**
 * @brief NEO1..N arasini mor yak, kalanlari sondur, refresh.
 */
static void neopixel_purple_step(int n)
{
    /* Mor = R+B (no G) */
    for (int i = 0; i < n && i < 12; i++) {
        neopixel_set_pixel(i, 180, 0, 180);
    }
    for (int i = n; i < 12; i++) {
        neopixel_set_pixel(i, 0, 0, 0);
    }
    neopixel_refresh();
}

void charger_if_play_charge_start_animation(void)
{
    power_ctrl_led_power(true);
    neopixel_set_brightness(80);  /* orta parlaklik */

    /* Bölüm 5.1: basamak basamak mor */
    neopixel_purple_step(3);   /* NEO1-3 */
    vTaskDelay(pdMS_TO_TICKS(500));
    neopixel_purple_step(6);   /* NEO1-6 */
    vTaskDelay(pdMS_TO_TICKS(500));
    neopixel_purple_step(9);   /* NEO1-9 */
    vTaskDelay(pdMS_TO_TICKS(500));
    neopixel_purple_step(12);  /* NEO1-12 */
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Söndür + MOSFET kapat */
    neopixel_clear();
    power_ctrl_led_power(false);
    ESP_LOGI(TAG, "Sarj baslangic animasyonu (mor basamak) tamam");
}

void charger_if_play_charge_complete_animation(void)
{
    power_ctrl_led_power(true);
    neopixel_set_brightness(30);  /* cok dusuk guc (cılız yesil) */

    /* 12 neopiksel yesil sabit */
    for (int i = 0; i < 12; i++) {
        neopixel_set_pixel(i, 0, 255, 0);
    }
    neopixel_refresh();
    ESP_LOGI(TAG, "Sarj tamamlandi animasyonu (yesil sabit)");
}

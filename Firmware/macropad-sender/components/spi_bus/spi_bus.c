// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file spi_bus.c
 * @brief Ortak SCK bit-bang yonetimi implementasyonu.
 */

#include "spi_bus.h"

#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "spi_bus";

/** Yapilandirilan SCK pin numarasi (spi_bus_init'de set edilir). */
static int s_sck_gpio = -1;

/** SCK pin baslatildi mi? */
static bool s_initialized = false;

esp_err_t spi_bus_init(int sck_gpio)
{
    if (s_initialized) {
        return ESP_OK;
    }
    if (sck_gpio < 0) {
        ESP_LOGE(TAG, "Gecersiz SCK pin: %d", sck_gpio);
        return ESP_ERR_INVALID_ARG;
    }

    s_sck_gpio = sck_gpio;

    /* SCK output, idle LOW (SPI Mode 0) */
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << sck_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SCK (GPIO%d) gpio_config hatasi: %s",
                 sck_gpio, esp_err_to_name(ret));
        return ret;
    }

    gpio_set_level(s_sck_gpio, 0);
    s_initialized = true;
    ESP_LOGI(TAG, "Ortak SCK pin GPIO%d bit-bang modunda baslatildi (idle LOW)",
             sck_gpio);
    return ESP_OK;
}

void spi_bus_set_sck(bool level)
{
    if (!s_initialized) {
        return;
    }
    gpio_set_level(s_sck_gpio, level ? 1 : 0);
}

void spi_bus_pulse_sck(void)
{
    if (!s_initialized) {
        return;
    }
    /* Mode 0: rising edge = sample/shift. Kisa low-high-low pulse. */
    gpio_set_level(s_sck_gpio, 1);
    /* gpio_set_level ~100ns surer, ek delay gerekmez (~1 MHz clock) */
    gpio_set_level(s_sck_gpio, 0);
}

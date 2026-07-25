// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file shift_out_74hc595.c
 * @brief 74HC595 shift-out surucu implementasyonu.
 */

#include "shift_out_74hc595.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "spi_bus.h"

static const char *TAG = "shift_out";

/** Pin numaralari (init'de set edilir). */
static int s_rclk_gpio = -1;
static int s_ser_gpio  = -1;

/** Yazilacak bit sayisi. */
static uint8_t s_bit_count = 0;

static bool s_initialized = false;

/** Maksimum desteklenen bit sayisi. */
#define SHIFT_OUT_MAX_BITS 32

esp_err_t shift_out_74hc595_init(int rclk_gpio, int ser_gpio, uint8_t bit_count)
{
    if (s_initialized) {
        return ESP_OK;
    }
    if (rclk_gpio < 0 || ser_gpio < 0) {
        ESP_LOGE(TAG, "Gecersiz pin: rclk=%d ser=%d", rclk_gpio, ser_gpio);
        return ESP_ERR_INVALID_ARG;
    }
    if (bit_count == 0 || bit_count > SHIFT_OUT_MAX_BITS) {
        ESP_LOGE(TAG, "Gecersiz bit_count: %u (1-%d arasi)",
                 bit_count, SHIFT_OUT_MAX_BITS);
        return ESP_ERR_INVALID_ARG;
    }

    s_rclk_gpio = rclk_gpio;
    s_ser_gpio  = ser_gpio;
    s_bit_count = bit_count;

    /* RCLK: output, idle LOW. Rising edge = output guncelle. */
    gpio_config_t rclk_cfg = {
        .pin_bit_mask = (1ULL << s_rclk_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&rclk_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RCLK gpio_config hatasi: %s", esp_err_to_name(ret));
        return ret;
    }
    gpio_set_level(s_rclk_gpio, 0);

    /* SER: output, idle LOW */
    gpio_config_t ser_cfg = {
        .pin_bit_mask = (1ULL << s_ser_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&ser_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SER gpio_config hatasi: %s", esp_err_to_name(ret));
        return ret;
    }
    gpio_set_level(s_ser_gpio, 0);

    s_initialized = true;
    ESP_LOGI(TAG, "74HC595 baslatildi: RCLK=GPIO%d SER=GPIO%d bits=%u",
             s_rclk_gpio, s_ser_gpio, s_bit_count);
    return ESP_OK;
}

void shift_out_74hc595_write(uint32_t value)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "init cagrilmadi");
        return;
    }

    /* RCLK low (output latch hazirligi) */
    gpio_set_level(s_rclk_gpio, 0);

    /* MSB first shift: en yuksek bit ilk cikis (QA'ya gider) */
    for (int8_t i = (int8_t)s_bit_count - 1; i >= 0; i--) {
        bool bit = ((value >> i) & 1u) != 0;
        spi_bus_set_sck(false);
        gpio_set_level(s_ser_gpio, bit ? 1 : 0);
        spi_bus_set_sck(true);   /* rising edge: SER sample + shift */
        spi_bus_set_sck(false);  /* falling edge: idle */
    }

    /* RCLK rising pulse: paralel cikis guncelle */
    gpio_set_level(s_rclk_gpio, 1);
    gpio_set_level(s_rclk_gpio, 0);
}

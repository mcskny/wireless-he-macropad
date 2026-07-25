// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file shift_in_sn74hc165.c
 * @brief SN74HC165 (x2) shift-in surucu implementasyonu.
 */

#include "shift_in_sn74hc165.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "spi_bus.h"

static const char *TAG = "shift_in";

/** Pin numaralari (init'de set edilir). */
static int s_latch_gpio = -1;
static int s_data_gpio  = -1;

/** Okunacak bit sayisi. */
static uint8_t s_bit_count = 0;

static bool s_initialized = false;

/** Maksimum desteklenen bit sayisi (uint32_t siniri). */
#define SHIFT_IN_MAX_BITS 32

esp_err_t shift_in_sn74hc165_init(int latch_gpio, int data_gpio, uint8_t bit_count)
{
    if (s_initialized) {
        return ESP_OK;
    }
    if (latch_gpio < 0 || data_gpio < 0) {
        ESP_LOGE(TAG, "Gecersiz pin: latch=%d data=%d", latch_gpio, data_gpio);
        return ESP_ERR_INVALID_ARG;
    }
    if (bit_count == 0 || bit_count > SHIFT_IN_MAX_BITS) {
        ESP_LOGE(TAG, "Gecersiz bit_count: %u (1-%d arasi olmali)",
                 bit_count, SHIFT_IN_MAX_BITS);
        return ESP_ERR_INVALID_ARG;
    }

    s_latch_gpio = latch_gpio;
    s_data_gpio  = data_gpio;
    s_bit_count  = bit_count;

    /* LATCH: output, idle HIGH (shift mode). Low pulse = paralel yukle. */
    gpio_config_t latch_cfg = {
        .pin_bit_mask = (1ULL << s_latch_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&latch_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LATCH gpio_config hatasi: %s", esp_err_to_name(ret));
        return ret;
    }
    gpio_set_level(s_latch_gpio, 1);  /* idle HIGH */

    /* DATA: input */
    gpio_config_t data_cfg = {
        .pin_bit_mask = (1ULL << s_data_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,   /* SR cikisi bos ise HIGH (pull-up) */
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&data_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "DATA gpio_config hatasi: %s", esp_err_to_name(ret));
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "SN74HC165 baslatildi: LATCH=GPIO%d DATA=GPIO%d bits=%u",
             s_latch_gpio, s_data_gpio, s_bit_count);
    return ESP_OK;
}

uint32_t shift_in_sn74hc165_read(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "init cagrilmadi");
        return 0;
    }

    /* 1. Paralel yukle: LATCH low pulse */
    gpio_set_level(s_latch_gpio, 0);
    /* ~1us yukleme suresi (SN74HC165 tPLH ~30ns, fazla bekleme gerekmez) */
    gpio_set_level(s_latch_gpio, 1);

    /* 2. N bit oku (MSB first): her clock rising'de 1 bit sample + shift */
    uint32_t value = 0;
    for (uint8_t i = 0; i < s_bit_count; i++) {
        spi_bus_set_sck(false);
        /* SCK low: SR QH kararli, sample hazirligi */
        spi_bus_set_sck(true);  /* rising edge: sample QH + shift */
        bool bit = (gpio_get_level(s_data_gpio) != 0);
        spi_bus_set_sck(false); /* falling edge: idle */
        value = (value << 1) | (bit ? 1u : 0u);
    }

    return value;
}

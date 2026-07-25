// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file mcp3202.c
 * @brief MCP3202 12-bit ADC bit-bang SPI surucu implementasyonu.
 */

#include "mcp3202.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "spi_bus.h"

static const char *TAG = "mcp3202";

/** Pin numaralari (mcp3202_init'de set edilir). */
static int s_cs_gpio   = -1;
static int s_miso_gpio = -1;
static int s_mosi_gpio = -1;

/** Baslatildi mi? */
static bool s_initialized = false;

esp_err_t mcp3202_init(int cs_gpio, int miso_gpio, int mosi_gpio)
{
    if (s_initialized) {
        return ESP_OK;
    }
    if (cs_gpio < 0 || miso_gpio < 0 || mosi_gpio < 0) {
        ESP_LOGE(TAG, "Gecersiz pin: cs=%d miso=%d mosi=%d",
                 cs_gpio, miso_gpio, mosi_gpio);
        return ESP_ERR_INVALID_ARG;
    }

    s_cs_gpio   = cs_gpio;
    s_miso_gpio = miso_gpio;
    s_mosi_gpio = mosi_gpio;

    /* CS: output, active low, idle HIGH */
    gpio_config_t cs_cfg = {
        .pin_bit_mask = (1ULL << s_cs_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&cs_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CS gpio_config hatasi: %s", esp_err_to_name(ret));
        return ret;
    }
    gpio_set_level(s_cs_gpio, 1);  /* idle HIGH */

    /* MISO: input */
    gpio_config_t miso_cfg = {
        .pin_bit_mask = (1ULL << s_miso_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&miso_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MISO gpio_config hatasi: %s", esp_err_to_name(ret));
        return ret;
    }

    /* MOSI: output, idle LOW */
    gpio_config_t mosi_cfg = {
        .pin_bit_mask = (1ULL << s_mosi_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&mosi_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MOSI gpio_config hatasi: %s", esp_err_to_name(ret));
        return ret;
    }
    gpio_set_level(s_mosi_gpio, 0);

    s_initialized = true;
    ESP_LOGI(TAG, "MCP3202 baslatildi: CS=GPIO%d MISO=GPIO%d MOSI=GPIO%d",
             s_cs_gpio, s_miso_gpio, s_mosi_gpio);
    return ESP_OK;
}

/**
 * @brief Tek bir bit transfer eder (Mode 0): MOSI yaz, SCK rising, MISO oku, SCK falling.
 * @param mosi_bit MOSI'ya yazilacak bit.
 * @return MISO'dan okunan bit.
 */
static bool mcp3202_xfer_bit(bool mosi_bit)
{
    spi_bus_set_sck(false);                 /* SCK low (idle) */
    gpio_set_level(s_mosi_gpio, mosi_bit ? 1 : 0);
    spi_bus_set_sck(true);                  /* rising edge: device sample/shift */
    bool miso = (gpio_get_level(s_miso_gpio) != 0);
    spi_bus_set_sck(false);                 /* falling edge: idle */
    return miso;
}

uint16_t mcp3202_read_channel(uint8_t channel)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "mcp3202_init cagrilmadi");
        return 0;
    }
    if (channel > 1) {
        ESP_LOGE(TAG, "Gecersiz kanal: %u (0 veya 1 olmali)", channel);
        return 0;
    }

    /* CS low: transaction baslat */
    gpio_set_level(s_cs_gpio, 0);

    /* Config (4 bit, MSB first): start=1, SGL/DIFF=1 (single), ODD/SIGN=ch, MSBF=1 */
    (void)mcp3202_xfer_bit(true);                       /* start bit */
    (void)mcp3202_xfer_bit(true);                       /* SGL/DIFF = single-ended */
    (void)mcp3202_xfer_bit((channel & 1u) != 0);        /* ODD/SIGN = channel */
    (void)mcp3202_xfer_bit(true);                       /* MSBF = MSB first */

    /* Null bit (yoksay) */
    (void)mcp3202_xfer_bit(false);

    /* 12-bit veri (MSB first) */
    uint16_t value = 0;
    for (int i = 0; i < 12; i++) {
        bool bit = mcp3202_xfer_bit(false);
        value = (uint16_t)((value << 1) | (bit ? 1u : 0u));
    }

    /* CS high: transaction bitir */
    gpio_set_level(s_cs_gpio, 1);

    return value;
}

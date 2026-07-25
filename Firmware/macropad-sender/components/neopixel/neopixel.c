// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file neopixel.c
 * @brief SK6812MINI neopixel RMT v6 surucu implementasyonu.
 */

#include "neopixel.h"

#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"

static const char *TAG = "neopixel";

/** RMT clock resolution: 10 MHz = 100 ns/tick. */
#define NEOPIXEL_RMT_RESOLUTION_HZ  (10 * 1000 * 1000)

/** SK6812 timing (100ns tick cinsinden). */
#define SK6812_T0H_TICKS  3   /* 300ns high */
#define SK6812_T0L_TICKS  9   /* 900ns low  */
#define SK6812_T1H_TICKS  6   /* 600ns high */
#define SK6812_T1L_TICKS  6   /* 600ns low  */

/** Maksimum desteklenen LED sayisi (buffer stack'te). */
#define NEOPIXEL_MAX_LEDS 16

/** RMT TX channel handle. */
static rmt_channel_handle_t s_tx_chan = NULL;

/** RMT bytes encoder handle (bit0/bit1 -> RMT symbol). */
static rmt_encoder_handle_t s_encoder = NULL;

/** GRB pixel buffer (3 byte per LED). */
static uint8_t s_buffer[NEOPIXEL_MAX_LEDS * 3];

/** LED sayisi. */
static int s_led_count = 0;

/** Parlaklik limiti (0-100, default 60). */
static uint8_t s_brightness_percent = 60;

static bool s_initialized = false;

esp_err_t neopixel_init(int gpio, int led_count)
{
    if (s_initialized) {
        return ESP_OK;
    }
    if (gpio < 0 || led_count <= 0 || led_count > NEOPIXEL_MAX_LEDS) {
        ESP_LOGE(TAG, "Gecersiz param: gpio=%d led_count=%d (max %d)",
                 gpio, led_count, NEOPIXEL_MAX_LEDS);
        return ESP_ERR_INVALID_ARG;
    }

    s_led_count = led_count;

    /* RMT TX channel config */
    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num           = gpio,
        .clk_src            = RMT_CLK_SRC_DEFAULT,
        .resolution_hz      = NEOPIXEL_RMT_RESOLUTION_HZ,
        .mem_block_symbols  = 64,
        .trans_queue_depth  = 4,
    };
    esp_err_t ret = rmt_new_tx_channel(&tx_cfg, &s_tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "rmt_new_tx_channel hatasi: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Bytes encoder: bit0 ve bit1 RMT symbol'leri */
    rmt_bytes_encoder_config_t enc_cfg = {
        .bit0 = {
            .duration0 = SK6812_T0H_TICKS,
            .level0    = 1,
            .duration1 = SK6812_T0L_TICKS,
            .level1    = 0,
        },
        .bit1 = {
            .duration0 = SK6812_T1H_TICKS,
            .level0    = 1,
            .duration1 = SK6812_T1L_TICKS,
            .level1    = 0,
        },
        .flags.msb_first = 1,  /* SK6812 MSB first */
    };
    ret = rmt_new_bytes_encoder(&enc_cfg, &s_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "rmt_new_bytes_encoder hatasi: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Channel'i aktive et */
    ret = rmt_enable(s_tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "rmt_enable hatasi: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Buffer sifirla (tum LED'ler off) */
    memset(s_buffer, 0, sizeof(s_buffer));

    s_initialized = true;
    ESP_LOGI(TAG, "Neopixel baslatildi: GPIO%d, %d LED, RMT %lu Hz",
             gpio, led_count, (unsigned long)NEOPIXEL_RMT_RESOLUTION_HZ);
    return ESP_OK;
}

void neopixel_set_pixel(int index, uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_initialized || index < 0 || index >= s_led_count) {
        return;
    }

    /* Brightness ölçekleme: (deger * percent) / 100 */
    uint16_t scale = s_brightness_percent;  /* 0-100 */
    uint8_t gr = (uint8_t)((r * scale) / 100u);
    uint8_t gg = (uint8_t)((g * scale) / 100u);
    uint8_t gb = (uint8_t)((b * scale) / 100u);

    /* SK6812 GRB sirasi: Yeşil, Kırmızı, Mavi */
    s_buffer[index * 3 + 0] = gg;
    s_buffer[index * 3 + 1] = gr;
    s_buffer[index * 3 + 2] = gb;
}

void neopixel_fill(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_initialized) {
        return;
    }
    for (int i = 0; i < s_led_count; i++) {
        neopixel_set_pixel(i, r, g, b);
    }
}

void neopixel_set_brightness(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    s_brightness_percent = percent;
}

esp_err_t neopixel_refresh(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0,  /* tek seferlik transmit (v6: loop_count dogrudan alan) */
    };
    esp_err_t ret = rmt_transmit(s_tx_chan, s_encoder, s_buffer,
                                 (size_t)(s_led_count * 3), &tx_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "rmt_transmit hatasi: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Transmit tamamlanana kadar bekle (100ms timeout) */
    ret = rmt_tx_wait_all_done(s_tx_chan, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "rmt_tx_wait_all_done hatasi: %s", esp_err_to_name(ret));
        return ret;
    }

    /* SK6812 reset: >80us low. RMT idle'da low, ama guvenli olmak icin kisa delay. */
    esp_rom_delay_us(80);

    return ESP_OK;
}

esp_err_t neopixel_clear(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    memset(s_buffer, 0, (size_t)(s_led_count * 3));
    return neopixel_refresh();
}

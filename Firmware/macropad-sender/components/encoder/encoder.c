// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file encoder.c
 * @brief Rotary encoder PCNT v2 quadrature decode implementasyonu.
 */

#include "encoder.h"

#include "esp_log.h"
#include "driver/pulse_cnt.h"

static const char *TAG = "encoder";

/** PCNT unit handle (file-local static, AI_GUIDELINES.md kural 16). */
static pcnt_unit_handle_t s_unit = NULL;

/** PCNT channel handle. */
static pcnt_channel_handle_t s_chan = NULL;

static bool s_initialized = false;

esp_err_t encoder_init(int phase_a_gpio, int phase_b_gpio)
{
    if (s_initialized) {
        return ESP_OK;
    }
    if (phase_a_gpio < 0 || phase_b_gpio < 0) {
        ESP_LOGE(TAG, "Gecersiz pin: A=%d B=%d", phase_a_gpio, phase_b_gpio);
        return ESP_ERR_INVALID_ARG;
    }

    /* PCNT unit: genis limit (delta birikebilsin) */
    pcnt_unit_config_t unit_cfg = {
        .low_limit  = -32768,
        .high_limit = 32767,
    };
    esp_err_t ret = pcnt_new_unit(&unit_cfg, &s_unit);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "pcnt_new_unit hatasi: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Glitch filter: PCB'de RC filtre (R43/C9, R42/C10) var, ek yazilim filtresi
       kisa (1us) - cift sayimi onler. */
    pcnt_glitch_filter_config_t glitch_cfg = {
        .max_glitch_ns = 1000,
    };
    pcnt_unit_set_glitch_filter(s_unit, &glitch_cfg);

    /* PCNT channel: A=edge, B=level */
    pcnt_chan_config_t chan_cfg = {
        .edge_gpio_num  = phase_a_gpio,
        .level_gpio_num = phase_b_gpio,
    };
    ret = pcnt_new_channel(s_unit, &chan_cfg, &s_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "pcnt_new_channel hatasi: %s", esp_err_to_name(ret));
        pcnt_del_unit(s_unit);
        s_unit = NULL;
        return ret;
    }

    /* Quadrature 4x decode:
     *   edge_action: pos=INCREASE (A rising), neg=DECREASE (A falling)
     *   level_action: high=INVERSE (B high -> edge action'ları ters çevir),
     *                low=KEEP (B low -> edge action'ları koru)
     * Sonuc: tam 4x quadrature, CW=artı, CCW=eksi */
    pcnt_channel_set_edge_action(s_chan,
                                 PCNT_CHANNEL_EDGE_ACTION_INCREASE,  /* pos edge */
                                 PCNT_CHANNEL_EDGE_ACTION_DECREASE); /* neg edge */
    pcnt_channel_set_level_action(s_chan,
                                  PCNT_CHANNEL_LEVEL_ACTION_KEEP,    /* B high */
                                  PCNT_CHANNEL_LEVEL_ACTION_INVERSE);/* B low  */

    /* Enable + start */
    ESP_ERROR_CHECK(pcnt_unit_enable(s_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(s_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(s_unit));

    s_initialized = true;
    ESP_LOGI(TAG, "Encoder baslatildi: A=GPIO%d B=GPIO%d (quadrature 4x)",
             phase_a_gpio, phase_b_gpio);
    return ESP_OK;
}

int16_t encoder_get_delta(void)
{
    if (!s_initialized || s_unit == NULL) {
        return 0;
    }

    int count = 0;
    esp_err_t ret = pcnt_unit_get_count(s_unit, &count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "pcnt_unit_get_count hatasi: %s", esp_err_to_name(ret));
        return 0;
    }

    /* Sayaci sifirla (delta bir sonraki cagri icin sifirdan baslar) */
    pcnt_unit_clear_count(s_unit);

    /* int -> int16_t (limitler -32768..32767, tek okumada tasmaz) */
    if (count > 32767) {
        count = 32767;
    } else if (count < -32768) {
        count = -32768;
    }
    return (int16_t)count;
}

void encoder_clear(void)
{
    if (s_initialized && s_unit != NULL) {
        pcnt_unit_clear_count(s_unit);
    }
}

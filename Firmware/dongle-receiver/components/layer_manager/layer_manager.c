// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file layer_manager.c
 * @brief Katman secimi + tuş esleme implementasyonu (Bölüm 5.1).
 *
 * Kisa basis (< LONG_PRESS_MS): katmani bir ileri tasi (0->1->2->3->0->...).
 * Uzun basis (>= LONG_PRESS_MS): katmani 0'a sifirla (iptal).
 */

#include "layer_manager.h"

#include "esp_log.h"

static const char *TAG = "layer";

#define LAYER_COUNT 4
#define LONG_PRESS_MS 2000u

static uint8_t s_current_layer = 0;
static bool s_prev_sw2 = false;
static uint32_t s_press_start_ms = 0;

void layer_manager_init(void)
{
    s_current_layer = 0;
    s_prev_sw2 = false;
    s_press_start_ms = 0;
    ESP_LOGI(TAG, "Layer manager baslatildi (layer=0)");
}

uint8_t layer_manager_update(bool sw2_pressed, uint32_t now_ms)
{
    /* Rising edge: butona yeni basildi */
    if (sw2_pressed && !s_prev_sw2) {
        s_press_start_ms = now_ms;
    }

    /* Falling edge: buton birakildi, basis suresine gore karar ver */
    if (!sw2_pressed && s_prev_sw2) {
        uint32_t held_ms = now_ms - s_press_start_ms;
        if (held_ms >= LONG_PRESS_MS) {
            /* Uzun basis: sifirla */
            if (s_current_layer != 0) {
                s_current_layer = 0;
                ESP_LOGI(TAG, "Katman sifirlandi (uzun basis, %lu ms)",
                         (unsigned long)held_ms);
            }
        } else {
            /* Kisa basis: bir ileri tasi */
            s_current_layer = (uint8_t)((s_current_layer + 1) % LAYER_COUNT);
            ESP_LOGI(TAG, "Katman degisti: %u (basis %lu ms)",
                     s_current_layer, (unsigned long)held_ms);
        }
    }

    s_prev_sw2 = sw2_pressed;
    return s_current_layer;
}

uint8_t layer_manager_get_current(void)
{
    return s_current_layer;
}

uint16_t layer_manager_get_keycode(uint8_t layer, uint8_t key_index,
                                   const uint16_t key_mappings[4][12])
{
    if (layer >= 4 || key_index >= 12 || key_mappings == NULL) {
        return 0;
    }
    return key_mappings[layer][key_index];
}
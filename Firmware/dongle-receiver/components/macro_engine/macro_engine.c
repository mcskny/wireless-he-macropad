// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file macro_engine.c
 * @brief Asenkron makro yurutucu implementasyonu (Bölüm 5.2).
 */

#include "macro_engine.h"

#include <string.h>
#include "esp_log.h"

static const char *TAG = "macro";

/** Aktif makro adimlari (kopya, baslatilda kopyalanir). */
static macro_step_t s_steps[MACRO_MAX_STEPS];
static uint8_t s_step_count = 0;
static uint8_t s_current_step = 0;
static uint32_t s_next_step_time_ms = 0;
static bool s_running = false;

void macro_engine_init(void)
{
    memset(s_steps, 0, sizeof(s_steps));
    s_step_count = 0;
    s_current_step = 0;
    s_next_step_time_ms = 0;
    s_running = false;
    ESP_LOGI(TAG, "Macro engine baslatildi");
}

bool macro_engine_start(const macro_step_t *steps, uint8_t count)
{
    if (s_running) {
        ESP_LOGW(TAG, "Makro zaten calisiyor, yeni baslatma reddedildi");
        return false;
    }
    if (steps == NULL || count == 0 || count > MACRO_MAX_STEPS) {
        return false;
    }

    memcpy(s_steps, steps, count * sizeof(macro_step_t));
    s_step_count = count;
    s_current_step = 0;
    s_running = true;

    /* Ilk adimi hemen isle */
    if (s_steps[0].keycode == 0) {
        /* keycode 0 = tum tuşlari birak */
        ESP_LOGI(TAG, "Makro adim 0: tum tuşlar birak");
    } else {
        ESP_LOGI(TAG, "Makro adim 0: tuş bas 0x%04X", s_steps[0].keycode);
    }
    /* TinyUSB entegrasyonu: usb_hid_send_keyboard cagrilacak */

    s_next_step_time_ms = 0;  /* ilk tick'te now_ms + delay set edilir */
    return true;
}

bool macro_engine_tick(uint32_t now_ms)
{
    if (!s_running) {
        return false;
    }

    /* Ilk tick: next_step_time set et */
    if (s_next_step_time_ms == 0) {
        s_next_step_time_ms = now_ms + s_steps[s_current_step].delay_ms;
        return true;
    }

    /* Sure dolmadi mi? */
    if (now_ms < s_next_step_time_ms) {
        return true;  /* hala calisiyor, bekle */
    }

    /* Siradaki adima gec */
    s_current_step++;
    if (s_current_step >= s_step_count) {
        /* Makro bitti */
        s_running = false;
        ESP_LOGI(TAG, "Makro tamamlandi (%u adim)", s_step_count);
        return false;
    }

    /* Adimi isle */
    const macro_step_t *step = &s_steps[s_current_step];
    if (step->keycode == 0) {
        ESP_LOGI(TAG, "Makro adim %u: tum tuşlar birak", s_current_step);
    } else {
        ESP_LOGI(TAG, "Makro adim %u: tuş bas 0x%04X", s_current_step, step->keycode);
    }
    /* TinyUSB entegrasyonu: usb_hid_send_keyboard cagrilacak */

    s_next_step_time_ms = now_ms + step->delay_ms;
    return true;
}

bool macro_engine_is_running(void)
{
    return s_running;
}

void macro_engine_stop(void)
{
    if (s_running) {
        ESP_LOGI(TAG, "Makro durduruldu (adim %u/%u)", s_current_step, s_step_count);
    }
    s_running = false;
    s_current_step = 0;
    s_next_step_time_ms = 0;
}

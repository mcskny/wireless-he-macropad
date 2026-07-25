// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file watchdog.c
 * @brief C6 baglanti watchdog implementasyonu.
 */

#include "watchdog.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "watchdog";

/** Timeout suresi (ms). */
static uint32_t s_timeout_ms = 1000;

/** Son feed zamani (ms). portMUX ile korunur. */
static uint32_t s_last_feed_ms = 0;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

static bool s_initialized = false;

esp_err_t watchdog_init(uint32_t timeout_ms)
{
    if (s_initialized) {
        return ESP_OK;
    }
    s_timeout_ms = (timeout_ms > 0) ? timeout_ms : 1000;

    portENTER_CRITICAL(&s_lock);
    s_last_feed_ms = 0;  /* 0 = hic feed yapilmadi */
    portEXIT_CRITICAL(&s_lock);

    s_initialized = true;
    ESP_LOGI(TAG, "Watchdog baslatildi: timeout=%lu ms", (unsigned long)s_timeout_ms);
    return ESP_OK;
}

void watchdog_feed(void)
{
    if (!s_initialized) {
        return;
    }
    /* esp_timer_get_time cagirmak yerine, main döngü now_ms'i feed'e geçebilirdi
       ama RX callback'ten cagrildiginda now_ms yok. esp_timer kullan. */
    extern int64_t esp_timer_get_time(void);
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);

    portENTER_CRITICAL(&s_lock);
    s_last_feed_ms = now;
    portEXIT_CRITICAL(&s_lock);
}

bool watchdog_is_expired(uint32_t now_ms)
{
    if (!s_initialized) {
        return true;  /* init edilmemis = expired (guvenli taraf) */
    }

    uint32_t last;
    portENTER_CRITICAL(&s_lock);
    last = s_last_feed_ms;
    portEXIT_CRITICAL(&s_lock);

    if (last == 0) {
        /* Hic feed yapilmadi: baglanti yok say, expired */
        return true;
    }

    return (now_ms - last) >= s_timeout_ms;
}

uint32_t watchdog_get_last_feed_ms(void)
{
    uint32_t last;
    portENTER_CRITICAL(&s_lock);
    last = s_last_feed_ms;
    portEXIT_CRITICAL(&s_lock);
    return last;
}

// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file watchdog.h
 * @brief C6 baglanti watchdog (Bölüm 3).
 *
 * C6'dan paket gelmezse 1000ms sonra timeout. Main döngü timeout olunca
 * USB HID'e "tum tuşlar birakildi" raporu gonderir.
 *
 * Thread-safety: feed() RX callback'ten (wifi task), check() main task'tan.
 * Shared state (last_feed_ms) portMUX ile korunur (AI_GUIDELINES.md kural 17).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Watchdog baslatir, timeout ayarlar, sayaci sifirlar.
 * @param timeout_ms Timeout suresi (ms, default 1000).
 * @return ESP_OK.
 */
esp_err_t watchdog_init(uint32_t timeout_ms);

/**
 * @brief C6'dan paket geldiginde cagrilmali (sayaci sifirlar).
 * RX callback veya main döngüde paket alininca cagrilmali.
 */
void watchdog_feed(void);

/**
 * @brief Timeout oldu mu?
 * @param now_ms Su anki zaman (ms, esp_timer_get_time()/1000).
 * @return true = timeout (tum tuşlar birakilmali), false = baglanti OK.
 */
bool watchdog_is_expired(uint32_t now_ms);

/**
 * @brief Son feed zamanini doner (ms).
 */
uint32_t watchdog_get_last_feed_ms(void);

#ifdef __cplusplus
}
#endif

// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file macro_engine.h
 * @brief Asenkron non-blocking makro yurutucu (Bölüm 5.2).
 *
 * Makro: tuş bas/birak + delay sequence. Main döngü her ms'de tick cagirir,
 * sure dolunca siradaki adim islenir. Blocking yapmaz (non-blocking).
 *
 * Makro tanimlari NVS'de saklanir (kullanici: hepsi NVS + web). Her tuşa bir
 * makro atanabilir; tuş basilince makro varsa baslatilir.
 *
 * @note STUB: makro NVS yukleme + gercek adim isleme simdilik log-only.
 *       nvs_store'a makro blob eklenecek, web'den yuklenecek.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maksimum makro adim sayisi. */
#define MACRO_MAX_STEPS 64

/**
 * @brief Makro adimi: bir tuş bas/birak + bekleme.
 */
typedef struct {
    uint16_t keycode;  /**< HID keycode (0 = tum tuşlari birak, !=0 = bu tuşu bas) */
    uint16_t delay_ms; /**< Bu adimdan sonra bekleme suresi (ms) */
} macro_step_t;

/**
 * @brief Makro motorunu baslatir.
 */
void macro_engine_init(void);

/**
 * @brief Makroyu baslatir (non-blocking).
 * @param steps Makro adimlari arrayi.
 * @param count Adim sayisi.
 * @return true = baslatildi, false = zaten calisiyor veya gecersiz.
 */
bool macro_engine_start(const macro_step_t *steps, uint8_t count);

/**
 * @brief Main döngüde her ms'de cagrilmali (Bölüm 5.2).
 *
 * Sure dolunca siradaki adimi isler. Non-blocking: hemen doner.
 *
 * @param now_ms Su anki zaman (ms).
 * @return true = makro hala calisiyor, false = bitti veya calismiyor.
 */
bool macro_engine_tick(uint32_t now_ms);

/**
 * @brief Makro calisiyor mu?
 */
bool macro_engine_is_running(void);

/**
 * @brief Makroyu durdur (tum tuşlari birak).
 */
void macro_engine_stop(void);

#ifdef __cplusplus
}
#endif

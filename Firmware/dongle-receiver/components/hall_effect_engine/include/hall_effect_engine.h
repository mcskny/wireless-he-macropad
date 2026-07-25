// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file hall_effect_engine.h
 * @brief Hall Effect isleme motoru (Bölüm 4).
 *
 * C6'dan gelen 12 analog degeri isleyerek tuş basili/birakildi durumlarini
 * ve joystick eksen degerlerini uretir.
 *
 * Adimlar (Bölüm 4):
 *   4.1 Dinamik normalizasyon: (adc - min) / (max - min) -> [0.0, 1.0]
 *   4.2 Geleneksel tetik (actuation + histerezis) veya Rapid Trigger (RT)
 *   4.3 Snap Tap (SOCD): zıt yon çiftleri, son basilan aktif
 *   4.4 Joystick: unipolar (×255) veya bipolar (pozitif-negatif × 32767)
 *
 * @note Tum hesaplamalar float (S3 FPU var). Performans: 12 tus ~50us.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "system_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Hall Effect motorunu baslatir (state sifirla).
 */
esp_err_t hall_engine_init(void);

/**
 * @brief 12 analog degeri isle, tuş durumlarini + joystick guncelle (Bölüm 4).
 *
 * Akis:
 *   1. Her tuş: normalize → traditional/RT → pressed
 *   2. Snap Tap çözümleme (zıt çiftler)
 *   3. Joystick dönüşümü (X/Y eksenleri)
 *
 * @param analog      C6'dan gelen 12 kanal ADC (0-4095).
 * @param cal         Kalibrasyon yapilandirmasi (NVS'den).
 * @param keys        12 tuş runtime durumu (in/out: highest/lowest_pos guncellenir).
 * @param active_keys Cikis: 12 tuş basili durumu (true = basili).
 * @param joy_x       Cikis: joystick X ekseni (-32768..32767).
 * @param joy_y       Cikis: joystick Y ekseni (-32768..32767).
 */
void hall_engine_process(const uint16_t analog[S3_KEY_COUNT],
                         const calibration_config_t *cal,
                         key_state_t keys[S3_KEY_COUNT],
                         bool active_keys[S3_KEY_COUNT],
                         int16_t *joy_x,
                         int16_t *joy_y);

/**
 * @brief Tum tuşlarini sifirla (birakma). Watchdog timeout icin.
 */
void hall_engine_release_all(key_state_t keys[S3_KEY_COUNT],
                             bool active_keys[S3_KEY_COUNT],
                             int16_t *joy_x,
                             int16_t *joy_y);

#ifdef __cplusplus
}
#endif

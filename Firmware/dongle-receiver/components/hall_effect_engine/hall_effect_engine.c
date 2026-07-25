// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file hall_effect_engine.c
 * @brief Hall Effect motoru implementasyonu (Bölüm 4).
 */

#include "hall_effect_engine.h"

#include <math.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "hall";

/** Geleneksel mod histerezis (Kconfig'ten, default 0.1). */
#define HALL_HYSTERESIS_DEFAULT  0.1f

/** RT sinirlar (Kconfig'ten, default 0.0 ve 1.0). */
#define HALL_RT_MIN_DEFAULT      0.0f
#define HALL_RT_MAX_DEFAULT      1.0f

static bool s_initialized = false;

esp_err_t hall_engine_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }
    s_initialized = true;
    ESP_LOGI(TAG, "Hall Effect motoru baslatildi");
    return ESP_OK;
}

/**
 * @brief Bölüm 4.1: Dinamik normalizasyon.
 * @return [0.0, 1.0] arasi normalized konum.
 */
static float hall_normalize(uint16_t adc, uint16_t min_adc, uint16_t max_adc)
{
    if (max_adc <= min_adc) {
        return 0.0f;  /* gecersiz kalibrasyon */
    }
    float pos = (float)(adc - min_adc) / (float)(max_adc - min_adc);
    if (pos < 0.0f) pos = 0.0f;
    if (pos > 1.0f) pos = 1.0f;
    return pos;
}

/**
 * @brief Bölüm 4.2: Geleneksel tetik (actuation point + histerezis).
 */
static bool hall_process_traditional(float position, float actuation,
                                     float hysteresis, bool currently_pressed)
{
    if (!currently_pressed) {
        /* Basili degil: actuation'i gecerse bas */
        if (position >= actuation) {
            return true;
        }
    } else {
        /* Basili: actuation - hysteresis altina düşerse birak */
        if (position < (actuation - hysteresis)) {
            return false;
        }
    }
    return currently_pressed;
}

/**
 * @brief Bölüm 4.2: Rapid Trigger (RT) - dinamik ust/dip takip.
 */
static bool hall_process_rapid(float position, float rt_press, float rt_release,
                               key_state_t *key)
{
    const float rt_min = HALL_RT_MIN_DEFAULT;
    const float rt_max = HALL_RT_MAX_DEFAULT;

    /* Sinir kontrol: min altinda → birak, max üstünde → bas */
    if (position <= rt_min) {
        key->highest_pos = position;
        key->lowest_pos = position;
        return false;
    }
    if (position >= rt_max) {
        key->highest_pos = position;
        key->lowest_pos = position;
        return true;
    }

    /* Guvenli alan: dinamik takip */
    if (key->pressed) {
        /* Basili: en derin nokta takip */
        if (position > key->highest_pos) {
            key->highest_pos = position;
        }
        /* Sr kadar yukari cikti → birak */
        if (position < (key->highest_pos - rt_release)) {
            key->pressed = false;
            key->lowest_pos = position;
        }
    } else {
        /* Birakilmis: en yuksek nokta takip */
        if (position < key->lowest_pos) {
            key->lowest_pos = position;
        }
        /* Sp kadar asagi indi → bas */
        if (position > (key->lowest_pos + rt_press)) {
            key->pressed = true;
            key->highest_pos = position;
        }
    }
    return key->pressed;
}

/**
 * @brief Bölüm 4.3: Snap Tap (SOCD) çözümleme.
 *
 * Zıt yön çifti ikisi de basili ise: son basilan aktif, diger pasif.
 * Biri birakilirsa diger (hala basili ise) aktif.
 */
static void hall_resolve_snap_tap(bool active_keys[S3_KEY_COUNT],
                                  const uint8_t snap_pairs[S3_SNAP_TAP_PAIR_MAX],
                                  int8_t *last_active_pair)
{
    for (int i = 0; i < S3_SNAP_TAP_PAIR_MAX; i++) {
        uint8_t a = snap_pairs[i];
        uint8_t b;
        /* Çift: snap_pairs[i] = a, snap_pairs[i+1] = b (çift indeks) */
        if (i % 2 != 0) continue;  /* tek indeks atla */
        if (a >= S3_KEY_COUNT) continue;
        if (i + 1 >= S3_SNAP_TAP_PAIR_MAX) continue;
        b = snap_pairs[i + 1];
        if (b >= S3_KEY_COUNT) continue;

        /* Iki tuş basili ise: son basilan aktif */
        if (active_keys[a] && active_keys[b]) {
            if (last_active_pair[i / 2] == a) {
                active_keys[b] = false;
            } else {
                active_keys[a] = false;
                last_active_pair[i / 2] = b;
            }
        } else if (active_keys[a]) {
            last_active_pair[i / 2] = a;
        } else if (active_keys[b]) {
            last_active_pair[i / 2] = b;
        }
    }
}

/**
 * @brief Bölüm 4.4: Joystick analog dönüşümü.
 *
 * bipolar: (V_pozitif - V_negatif) × 32767
 */
static void hall_compute_joystick(const key_state_t keys[S3_KEY_COUNT],
                                  const uint8_t joystick_axes[S3_JOYSTICK_AXIS_COUNT],
                                  int16_t *joy_x, int16_t *joy_y)
{
    /* joystick_axes[0]=X+, [1]=X-, [2]=Y+, [3]=Y- (tuş indeksleri) */
    float x_pos = 0.0f, x_neg = 0.0f, y_pos = 0.0f, y_neg = 0.0f;

    if (joystick_axes[0] < S3_KEY_COUNT) x_pos = keys[joystick_axes[0]].position;
    if (joystick_axes[1] < S3_KEY_COUNT) x_neg = keys[joystick_axes[1]].position;
    if (joystick_axes[2] < S3_KEY_COUNT) y_pos = keys[joystick_axes[2]].position;
    if (joystick_axes[3] < S3_KEY_COUNT) y_neg = keys[joystick_axes[3]].position;

    /* bipolar: (pozitif - negatif) × 32767 */
    float x = (x_pos - x_neg) * 32767.0f;
    float y = (y_pos - y_neg) * 32767.0f;

    /* Clamp int16 araligina */
    if (x > 32767.0f) x = 32767.0f;
    if (x < -32768.0f) x = -32768.0f;
    if (y > 32767.0f) y = 32767.0f;
    if (y < -32768.0f) y = -32768.0f;

    *joy_x = (int16_t)x;
    *joy_y = (int16_t)y;
}

/** Snap Tap icin son aktif tuş takibi (her çift için). */
static int8_t s_last_active_pair[S3_SNAP_TAP_PAIR_MAX / 2] = {0};

void hall_engine_process(const uint16_t analog[S3_KEY_COUNT],
                         const calibration_config_t *cal,
                         key_state_t keys[S3_KEY_COUNT],
                         bool active_keys[S3_KEY_COUNT],
                         int16_t *joy_x,
                         int16_t *joy_y)
{
    if (!s_initialized || analog == NULL || cal == NULL || keys == NULL
        || active_keys == NULL) {
        return;
    }

    /* 4.1 + 4.2: Her tuş icin normalize + trigger modu */
    for (int i = 0; i < S3_KEY_COUNT; i++) {
        /* 4.1 Normalizasyon */
        keys[i].position = hall_normalize(analog[i], cal->min_adc[i], cal->max_adc[i]);

        /* 4.2 Trigger modu: traditional veya RT */
        if (cal->trigger_mode[i] == TRIGGER_MODE_RAPID) {
            keys[i].pressed = hall_process_rapid(
                keys[i].position,
                cal->rt_press_sensitivity[i],
                cal->rt_release_sensitivity[i],
                &keys[i]);
        } else {
            keys[i].pressed = hall_process_traditional(
                keys[i].position,
                cal->actuation_point[i],
                HALL_HYSTERESIS_DEFAULT,
                keys[i].pressed);
        }

        active_keys[i] = keys[i].pressed;
    }

    /* 4.3 Snap Tap çözümleme */
    hall_resolve_snap_tap(active_keys, cal->snap_tap_pairs, s_last_active_pair);

    /* 4.4 Joystick dönüşümü */
    if (joy_x != NULL && joy_y != NULL) {
        hall_compute_joystick(keys, cal->joystick_axes, joy_x, joy_y);
    }
}

void hall_engine_release_all(key_state_t keys[S3_KEY_COUNT],
                             bool active_keys[S3_KEY_COUNT],
                             int16_t *joy_x,
                             int16_t *joy_y)
{
    if (keys != NULL) {
        for (int i = 0; i < S3_KEY_COUNT; i++) {
            keys[i].pressed = false;
            keys[i].highest_pos = 0.0f;
            keys[i].lowest_pos = 0.0f;
        }
    }
    if (active_keys != NULL) {
        memset(active_keys, 0, S3_KEY_COUNT * sizeof(bool));
    }
    if (joy_x != NULL) *joy_x = 0;
    if (joy_y != NULL) *joy_y = 0;
    /* Snap tap takibi sifirla */
    memset(s_last_active_pair, -1, sizeof(s_last_active_pair));
}

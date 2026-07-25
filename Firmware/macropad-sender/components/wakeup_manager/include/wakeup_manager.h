// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file wakeup_manager.h
 * @brief Light/deep sleep + wakeup kaynagi yonetimi (Bölüm 4.1, 4.4).
 *
 * Iki wakeup kaynagi:
 *   1. GPIO wake (Diot-OR IO4): 12 dijital tuştan herhangi biri basildiginda
 *      aninda uyanir. Light sleep ve deep sleep icin.
 *   2. Timer wake (periyodik): encoder ve SR buton (SW2/ENC_BTN) polling icin.
 *      Sadece light sleep'te.
 *
 * @note Diot-OR hatti yalnizca 12 dijital tusu OR'lar. Encoder ve SR butonlari
 *       bu hatta bagli DEGILDIR; bu yüzden periyodik timer wake ile poll edilir.
 *
 * @note IO4 (Diot-OR) RTC GPIO olmali (light/deep sleep GPIO wake icin).
 *       ESP32-C6'da GPIO0-7 RTC GPIO'dur, IO4 = RTC GPIO 4.
 *
 * @note Tuş polarity: Diot-OR active low varsayilir (tuş basili = LOW).
 *       Donanima gore degistirilebilir (wakeup_manager_init parametresi).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Wakeup kaynaklarini yapilandirir (Diot-OR GPIO wake).
 *
 * @param diot_or_gpio Diot-OR pin (IO4). RTC GPIO olmali.
 * @param active_low   true: tuş basili = LOW (pull-up ile). false: basili = HIGH.
 * @return ESP_OK veya GPIO/sleep yapilandirma hatasi.
 */
esp_err_t wakeup_manager_init(int diot_or_gpio, bool active_low);

/**
 * @brief Light sleep'e girer: GPIO wake (tuş) + timer wake (polling).
 *
 * Akis:
 *   1. Timer wake: poll_period_ms sonra uyan (encoder/SR buton poll).
 *   2. GPIO wake: tuş basildiginda aninda uyan.
 *   3. esp_light_sleep_start().
 *
 * @param poll_period_ms Timer wake periyodu (ms). 0 = timer wake yok.
 * @return ESP_OK (uyandi) veya sleep hatasi.
 *
 * @note Uyandiktan sonra wakeup_manager_woken_by_gpio() ile nedeni sorgula.
 */
esp_err_t wakeup_manager_light_sleep(uint32_t poll_period_ms);

/**
 * @brief Deep sleep'e girer: sadece GPIO wake (tuş) ile uyanir.
 *
 * Kritik kapanma (Bölüm 4.4) icin. Uyandiginda sistem resetlenir (deep sleep
 * sonrasi app_main bastan calisir). main.c wakeup cause kontrol edip
 * battery_dead rutinini devam ettirir.
 *
 * @note Bu fonksiyon DONMEZ. esp_deep_sleep_start() sonrasi reset.
 */
void wakeup_manager_deep_sleep(void);

/**
 * @brief Son uyandinin nedeni tuş (GPIO wake) miydi?
 * @return true = tuş ile uyandi, false = timer ile uyandi.
 */
bool wakeup_manager_woken_by_gpio(void);

#ifdef __cplusplus
}
#endif

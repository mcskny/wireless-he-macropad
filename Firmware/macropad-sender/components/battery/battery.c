// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file battery.c
 * @brief Pil yonetimi implementasyonu: flag'ler + animasyonlar (Bölüm 4.4).
 */

#include "battery.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "neopixel.h"
#include "power_ctrl.h"
#include "shift_out_74hc595.h"

static const char *TAG = "battery";

/**
 * @brief Ilk N neopikseli belirtilen renkte yakar, kalanlari sondurur.
 */
static void neopixel_light_first_n(int n, uint8_t r, uint8_t g, uint8_t b)
{
    /* NEOPIXEL_LED_COUNT 12, ama neopixel.h'den alamayiz (constant degil).
       neopixel_init led_count ile cagrildi, 12 LED. Hard-coded 12. */
    for (int i = 0; i < n && i < 12; i++) {
        neopixel_set_pixel(i, r, g, b);
    }
    for (int i = n; i < 12; i++) {
        neopixel_set_pixel(i, 0, 0, 0);
    }
    neopixel_refresh();
}

/**
 * @brief N blink: LED MOSFET ac, N kez yak/sondur, MOSFET kapat.
 */
static void blink_pattern(int first_n, uint8_t r, uint8_t g, uint8_t b, int count)
{
    power_ctrl_led_power(true);
    /* Parlaklik animasyon icin tam; power_ctrl_apply brightness'u etkilemesin */
    neopixel_set_brightness(100);

    for (int i = 0; i < count; i++) {
        neopixel_light_first_n(first_n, r, g, b);
        vTaskDelay(pdMS_TO_TICKS(BATTERY_BLINK_ON_MS));
        neopixel_clear();
        vTaskDelay(pdMS_TO_TICKS(BATTERY_BLINK_OFF_MS));
    }

    power_ctrl_led_power(false);
}

/**
 * @brief Pil <%50: 6 neopiksel Sari 3 blink (Bölüm 4.4).
 */
static void battery_check_50(uint8_t percent, battery_flags_t *flags)
{
    if (percent < 50 && !flags->flag_50) {
        ESP_LOGW(TAG, "Pil <%50, animasyon (6 Sari blink)");
        blink_pattern(6, 255, 255, 0, BATTERY_BLINK_COUNT);  /* Sari */
        flags->flag_50 = true;
    }
}

/**
 * @brief Pil <%25: 3 neopiksel Sari 3 blink + parlaklik %30 cap (Bölüm 4.4).
 */
static void battery_check_25(uint8_t percent, battery_flags_t *flags)
{
    if (percent < 25 && !flags->flag_25) {
        ESP_LOGW(TAG, "Pil <%25, animasyon (3 Sari blink, parlaklik %30 cap)");
        blink_pattern(3, 255, 255, 0, BATTERY_BLINK_COUNT);  /* Sari */
        flags->flag_25 = true;
        /* Parlaklik cap main.c tarafindan power_ctrl_apply_brightness ile uygulanir */
    }
}

/**
 * @brief Pil <%15: flag_15 set (Bölüm 4.4). Layer LED blink main.c 5 dk'da.
 */
static void battery_check_15(uint8_t percent, battery_flags_t *flags)
{
    if (percent < 15 && !flags->flag_15) {
        ESP_LOGW(TAG, "Pil <%15, flag_15 set (layer LED 5 dk'da blink)");
        flags->flag_15 = true;
    }
}

/**
 * @brief Pil <%5: 12 neopiksel Kirmizi 3 blink + battery_dead (Bölüm 4.4).
 */
static bool battery_check_5(uint8_t percent, battery_flags_t *flags,
                            system_state_t *state)
{
    if (percent < 5 && !flags->flag_5) {
        ESP_LOGE(TAG, "Pil <%5 KRITIK! battery_dead=true (12 Kirmizi blink)");
        blink_pattern(12, 255, 0, 0, BATTERY_BLINK_COUNT);  /* Kirmizi */
        flags->flag_5 = true;
        state->battery_dead = true;
        return true;
    }
    return false;
}

void battery_init(void)
{
    ESP_LOGI(TAG, "Battery module baslatildi (flag'ler sifir)");
}

bool battery_update(uint8_t percent, system_state_t *state)
{
    if (state == NULL) {
        return false;
    }

    /* Yuzde clamp */
    if (percent > 100) {
        percent = 100;
    }

    battery_check_50(percent, &state->battery_flags);
    battery_check_25(percent, &state->battery_flags);
    battery_check_15(percent, &state->battery_flags);

    if (battery_check_5(percent, &state->battery_flags, state)) {
        return true;  /* battery_dead set edildi */
    }
    return false;
}

void battery_blink_layer(uint8_t active_layer)
{
    /* Layer LED: D8-D11 = shift_out bit 0-3.
       active_layer 0-3 → ilgili bit set. */
    for (int i = 0; i < BATTERY_LAYER_BLINK_COUNT; i++) {
        shift_out_74hc595_write((uint32_t)(1u << active_layer));
        vTaskDelay(pdMS_TO_TICKS(BATTERY_BLINK_ON_MS));
        shift_out_74hc595_write(0);
        vTaskDelay(pdMS_TO_TICKS(BATTERY_BLINK_OFF_MS));
    }
}

uint8_t battery_adc_to_percent(uint16_t adc_raw)
{
    /* STUB: lineer map adc → yüzde. Kalibrasyon Kconfig ile yapilacak.
       Varsayim: adc_min=0 (0V), adc_max=4095 (3.3V). DONANIMA BAGLI, degistir. */
    const uint16_t adc_min = 0;
    const uint16_t adc_max = 4095;

    if (adc_raw <= adc_min) {
        return 0;
    }
    if (adc_raw >= adc_max) {
        return 100;
    }
    uint32_t pct = ((uint32_t)(adc_raw - adc_min) * 100u) / (uint32_t)(adc_max - adc_min);
    return (uint8_t)pct;
}

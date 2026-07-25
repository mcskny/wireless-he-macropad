// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file main.c
 * @brief ESP32-S3 Dongle ana giris noktasi. Bölüm 2 state machine.
 *
 * Iki mod:
 *   - NORMAL_MODE: ESP-NOW RX + Hall motoru + USB HID 1000Hz
 *   - CONFIG_AP_MODE: Wi-Fi AP + WebSocket kalibrasyon
 *
 * Config_Mode_Request (C6 status bit veya web exit) ile geçiş.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "driver/gpio.h"

#include "pinout.h"
#include "system_state.h"

#include "nvs_store.h"
#include "espnow_rx.h"
#include "packet_decoder.h"
#include "watchdog.h"
#include "hall_effect_engine.h"
#include "layer_manager.h"
#include "macro_engine.h"
#include "usb_hid.h"
#include "usb_power.h"
#include "config_ap.h"

static const char *TAG = "s3dongle";

/* --- Paket zamanlama istatistikleri (1 saniyede bir rapor) --- */
static uint32_t s_last_packet_us = 0;
static uint32_t s_interval_min_us = UINT32_MAX;
static uint32_t s_interval_max_us = 0;
static uint64_t s_interval_sum_us = 0;
static uint32_t s_interval_count = 0;
static uint32_t s_last_report_ms = 0;
static uint32_t s_last_report_packets_lost = 0;
static uint32_t s_packets_reordered = 0;
static uint32_t s_last_report_packets_reordered = 0;

/** Sira karisikligi icin makul ust sinir: bunun ustundeki farklar
 *  gercek kayip yerine "reorder/duplicate" sayilir (yanlis pozitifi onler). */
#define MAX_PLAUSIBLE_LOSS 50

/** C6 MAC string parse. */
static void parse_mac(const char *str, uint8_t mac[6])
{
    if (sscanf(str, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
               &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
        ESP_LOGW(TAG, "MAC parse hatasi, default FF");
        memset(mac, 0xFF, 6);
    }
}

/**
 * @brief Tum bilesenleri baslatir (Bölüm 1).
 */
static esp_err_t init_subsystems(s3_system_state_t *state)
{
    memset(state, 0, sizeof(*state));
    state->mode = S3_MODE_NORMAL;

    /* 1. NVS + kalibrasyon (Bölüm 1.1) */
    ESP_RETURN_ON_ERROR(nvs_store_init(), TAG, "nvs_store_init");
    ESP_RETURN_ON_ERROR(nvs_store_load_calibration(&state->calibration), TAG, "load_calib");

    /* 2. ESP-NOW RX (Bölüm 1.2 + 3) */
    uint8_t c6_mac[6] = {0};
    parse_mac(CONFIG_C6_PEER_MAC, c6_mac);
    ESP_RETURN_ON_ERROR(espnow_rx_init(c6_mac, CONFIG_ESPNOW_CHANNEL), TAG, "espnow_rx");

    /* 3. Watchdog (Bölüm 3) */
    ESP_RETURN_ON_ERROR(watchdog_init(CONFIG_C6_WATCHDOG_TIMEOUT_MS), TAG, "watchdog");

    /* 4. Hall engine + layer + macro (Bölüm 4 + 5) */
    ESP_RETURN_ON_ERROR(hall_engine_init(), TAG, "hall_engine");
    layer_manager_init();
    macro_engine_init();

    /* 5. USB HID + power (Bölüm 6) */
    ESP_RETURN_ON_ERROR(usb_hid_init(), TAG, "usb_hid");
    ESP_RETURN_ON_ERROR(usb_power_init(), TAG, "usb_power");

    /* 6. BOOT button (GPIO0) - CONFIG_AP gecisi icin */
    gpio_config_t boot_btn_cfg = {
        .pin_bit_mask = (1ULL << PIN_BOOT_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&boot_btn_cfg), TAG, "boot_button");

    ESP_LOGI(TAG, "Tum bilesenler baslatildi (NORMAL_MODE)");
    return ESP_OK;
}

/**
 * @brief BOOT button (GPIO0) kısa basış kontrolü.
 *
 * Debounce + kısa basış algılama (<1sn). Kısa basış → true (config toggle).
 *
 * @param now_ms Su anki zaman (ms).
 * @return true = kısa basış algılandı (CONFIG_AP toggle).
 */
static bool check_boot_button(uint32_t now_ms)
{
    static bool s_prev_pressed = false;  /* idle: HIGH (pull-up) */
    static uint32_t s_press_ms = 0;

    bool pressed = (gpio_get_level(PIN_BOOT_BUTTON) == 0);  /* LOW = pressed */

    if (!s_prev_pressed && pressed) {
        /* HIGH → LOW: basılma */
        s_press_ms = now_ms;
    } else if (s_prev_pressed && !pressed) {
        /* LOW → HIGH: bırakma */
        uint32_t duration = now_ms - s_press_ms;
        s_prev_pressed = pressed;
        if (duration >= 50 && duration < 1000) {
            ESP_LOGI(TAG, "BOOT button kısa basış (%lu ms) → config toggle",
                     (unsigned long)duration);
            return true;
        }
    }
    s_prev_pressed = pressed;
    return false;
}

/**
 * @brief Sira numarasi farkindan kayip/reorder ayrimi yapar.
 *
 * counter 8-bit wrap-around'lu bir sayac. Normal durumda ardisik paketler
 * arasinda counter 1 artar (diff=1, kayip=0). Paket kaybolduysa diff>1
 * olur (kayip = diff-1). Ancak yuksek hizda ESP-NOW paketleri bazen
 * siralamayi bozarak (out-of-order) dongle'a ulasabilir; bu durumda
 * counter GERIYE gidebilir (diff kucuk/negatif gorunur, wrap sonrasi
 * yanlislikla 250+ gibi devasa "kayip" hesaplanir). Bunu onlemek icin:
 *   - diff makul bir aralikta (1..MAX_PLAUSIBLE_LOSS) ise gercek kayip say.
 *   - diff bu aralik disindaysa (asiri buyuk ya da <=0) reorder/duplicate
 *     say, kayip sayacina ekleme (yanlis pozitifi onler).
 */
static void update_sequence_stats(s3_system_state_t *state, uint8_t counter,
                                  bool is_first_packet)
{
    if (is_first_packet) {
        state->last_sequence_number = counter;
        return;
    }

    int16_t diff = (int16_t)counter - (int16_t)state->last_sequence_number;
    if (diff <= 0) {
        diff += 256;  /* wrap-around */
    }

    if (diff >= 1 && diff <= MAX_PLAUSIBLE_LOSS) {
        state->packets_lost += (uint32_t)(diff - 1);
    } else {
        /* Mantiksiz buyuk fark: sira karisikligi/duplicate, kayip sayma */
        s_packets_reordered++;
    }

    state->last_sequence_number = counter;
}

/**
 * @brief NORMAL_MODE: bir döngü adimi (Bölüm 2-6).
 * @return true = CONFIG_AP_MODE'a geçiş isteği, false = devam.
 */
static bool normal_mode_step(s3_system_state_t *state)
{
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);

    /* 1. ESP-NOW RX: paket bekle (1ms = 1000Hz) */
    espnow_rx_packet_t pkt;
    esp_err_t ret = espnow_rx_get_packet(&pkt, 1);

    if (ret == ESP_OK) {
        /* 2. Watchdog feed (Bölüm 3) */
        watchdog_feed();
        state->c6_connection_watchdog = 0;

        /* Paketler-arasi sure olcumu (istatistik icin) */
        uint32_t now_us = (uint32_t)esp_timer_get_time();
        if (s_last_packet_us != 0) {
            uint32_t interval_us = now_us - s_last_packet_us;
            if (interval_us < s_interval_min_us) s_interval_min_us = interval_us;
            if (interval_us > s_interval_max_us) s_interval_max_us = interval_us;
            s_interval_sum_us += interval_us;
            s_interval_count++;
        }
        s_last_packet_us = now_us;

        /* 3. Packet decode (Bölüm 3) */
        uint8_t counter, status;
        uint16_t buttons;
        int8_t enc_delta;
        uint16_t analog[12] = {0};

        bool has_analog = (pkt.len == 23);
        if (has_analog) {
            packet_decode_full(pkt.data, &counter, &status, &buttons,
                               &enc_delta, analog);
        } else {
            packet_decode_fast(pkt.data, &counter, &status, &buttons,
                               &enc_delta);
        }

        /* Seq kayip/reorder takibi (duzeltilmis) */
        bool is_first_packet = (state->packets_received == 0);
        update_sequence_stats(state, counter, is_first_packet);
        state->packets_received++;

        /* Status parse: SW2, config mode request */
        power_mode_t c6_mode;
        bool sw2_pressed, enc_btn, flag_15, battery_dead, charging, charge_complete;
        packet_parse_status(status, &c6_mode, &sw2_pressed, &enc_btn,
                            &flag_15, &battery_dead, &charging, &charge_complete);

        /* Config mode request (Bölüm 2: Config_Mode_Request biti) */
        /* Simdilik: SW2 + ENC_BTN ayni anda basili → config mode (placeholder) */
        if (sw2_pressed && enc_btn) {
            ESP_LOGI(TAG, "Config mode request (SW2+ENC_BTN)");
            return true;  /* CONFIG_AP_MODE'a geç */
        }

        /* 4. Hall Effect motoru (Bölüm 4, sadece 23B pakette) */
        if (has_analog) {
            hall_engine_process(analog, &state->calibration, state->keys,
                                state->active_keys, &state->joystick_x,
                                &state->joystick_y);
        }

        /* 5. Layer update (Bölüm 5.1: SW2 kisa/uzun basis) */
        state->current_layer_index = layer_manager_update(sw2_pressed, now_ms);

        /* 6. USB HID report (Bölüm 6) */
        if (!macro_engine_is_running()) {
            usb_hid_send_keyboard(state->active_keys,
                                  state->calibration.key_mappings,
                                  state->current_layer_index);
        }
        usb_hid_send_gamepad(state->joystick_x, state->joystick_y);
    } else {
        /* Paket yok: watchdog check (Bölüm 3) */
        if (watchdog_is_expired(now_ms)) {
            /* C6 bağlantı koptu: tum tuşlari birak */
            hall_engine_release_all(state->keys, state->active_keys,
                                    &state->joystick_x, &state->joystick_y);
            usb_hid_release_all();
        }
    }

    /* 7. Macro tick (Bölüm 5.2) */
    macro_engine_tick(now_ms);

    /* 8. USB suspend check (Bölüm 6.3) */
    if (usb_hid_is_suspended() && !usb_power_is_suspended()) {
        usb_power_on_suspend();
    } else if (!usb_hid_is_suspended() && usb_power_is_suspended()) {
        usb_power_on_resume();
    }

    /* Saniyede bir istatistik raporu */
    if (now_ms - s_last_report_ms >= 1000) {
        uint32_t avg_us = (s_interval_count > 0)
                           ? (uint32_t)(s_interval_sum_us / s_interval_count)
                           : 0;
        float hz = (avg_us > 0) ? (1000000.0f / (float)avg_us) : 0.0f;
        uint32_t loss_window = state->packets_lost - s_last_report_packets_lost;
        uint32_t reorder_window = s_packets_reordered - s_last_report_packets_reordered;

        ESP_LOGI(TAG, "Istatistik: paket=%lu ort=%lu us (%.1f Hz) min=%lu us max=%lu us kayip=%lu reorder=%lu",
                 (unsigned long)s_interval_count,
                 (unsigned long)avg_us, hz,
                 (unsigned long)((s_interval_min_us == UINT32_MAX) ? 0 : s_interval_min_us),
                 (unsigned long)s_interval_max_us,
                 (unsigned long)loss_window,
                 (unsigned long)reorder_window);

        s_interval_min_us = UINT32_MAX;
        s_interval_max_us = 0;
        s_interval_sum_us = 0;
        s_interval_count = 0;
        s_last_report_ms = now_ms;
        s_last_report_packets_lost = state->packets_lost;
        s_last_report_packets_reordered = s_packets_reordered;
    }

    return false;
}

/**
 * @brief CONFIG_AP_MODE: bir döngü adimi (Bölüm 2).
 * @return true = NORMAL_MODE'a dönüş, false = devam.
 */
static bool config_ap_mode_step(s3_system_state_t *state)
{
    /* exit isteği kontrol */
    if (config_ap_exit_requested()) {
        return true;  /* NORMAL_MODE'a dön */
    }

    /* ADC akışı: son analog degerleri gonder */
    static uint32_t last_adc_send_ms = 0;
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    if ((now_ms - last_adc_send_ms) >= 10) {  /* 100 Hz ADC akışı */
        last_adc_send_ms = now_ms;
        uint16_t analog[12] = {0};
        for (int i = 0; i < 12; i++) {
            /* Son position'dan ADC'ye geri don (yaklasik) */
            analog[i] = (uint16_t)(state->keys[i].position * 4095.0f);
        }
        config_ap_send_adc(analog);
    }

    vTaskDelay(pdMS_TO_TICKS(10));
    return false;
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32-S3 Dongle (tam) ===");
    ESP_LOGI(TAG, "IDF: %s  Target: %s", esp_get_idf_version(), CONFIG_IDF_TARGET);

    static s3_system_state_t state;
    if (init_subsystems(&state) != ESP_OK) {
        ESP_LOGE(TAG, "Init basarisiz");
        return;
    }

    ESP_LOGI(TAG, "Ana döngü basliyor (Bölüm 2 state machine)");

    while (1) {
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        bool boot_short_press = check_boot_button(now_ms);

        if (state.mode == S3_MODE_NORMAL) {
            /* NORMAL_MODE: boot button veya C6 config request → CONFIG_AP */
            if (boot_short_press || normal_mode_step(&state)) {
                ESP_LOGI(TAG, "CONFIG_AP_MODE'a geciliyor");
                config_ap_start(CONFIG_AP_SSID, CONFIG_AP_PASSWORD, CONFIG_AP_CHANNEL);
                state.mode = S3_MODE_CONFIG_AP;
            }
        } else {
            /* CONFIG_AP_MODE: boot button veya web exit → NORMAL */
            if (boot_short_press || config_ap_mode_step(&state)) {
                ESP_LOGI(TAG, "NORMAL_MODE'a donuluyor");
                config_ap_stop();
                uint8_t c6_mac[6] = {0};
                parse_mac(CONFIG_C6_PEER_MAC, c6_mac);
                espnow_rx_init(c6_mac, CONFIG_ESPNOW_CHANNEL);
                watchdog_feed();
                state.mode = S3_MODE_NORMAL;
            }
        }
    }
}
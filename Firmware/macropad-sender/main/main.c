// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

/**
 * @file main.c
 * @brief ESP32-C6 Macropad ana giris noktasi. Bölüm 2 ana döngü.
 *
 * Öncelik sirasi (Bölüm 2.1):
 *   1. Şarj denetimi (Bölüm 5)
 *   2. Kritik pil (Bölüm 4.4 deep sleep)
 *   3. Kombinasyon (Bölüm 6 - ATLANDI)
 *   4. Mod degisim (SW2 + encoder, Bölüm 2.1.4)
 *   5. Parlaklik degisim (Tuş12 + encoder, Bölüm 2.1.5)
 *   6. Sensör okuma + paket gönder (Bölüm 3)
 *   7. Pil ölçüm (10 sn'de bir, Bölüm 4.4)
 *   8. Güç modu sleep (Bölüm 4.1/4.2)
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_attr.h"

#include "pinout.h"
#include "system_state.h"

#include "nvs_store.h"
#include "spi_bus.h"
#include "mcp3202.h"
#include "mux_cd74hc4067.h"
#include "shift_in_sn74hc165.h"
#include "shift_out_74hc595.h"
#include "encoder.h"
#include "neopixel.h"
#include "power_ctrl.h"
#include "battery.h"
#include "charger_if.h"
#include "packet.h"
#include "espnow_link.h"
#include "wakeup_manager.h"

static const char *TAG = "macropad";

/** PCB'deki iki SN74HC165: toplam 16 paralel giriş. */
#define SHIFT_IN_BITS 16
#define SHIFT_OUT_BITS 8
#define ENCODER_COUNTS_PER_DETENT 4

/* U29 H..A ilk okunur (sr_val[15:8]); U30 H..A sonra ([7:0]).
 * TCS40DLR açık drençtir: algılamada LOW olur, pull-up idle HIGH verir.
 * U29 A..H = keys 0..7, U30 A..D = keys 8..11, E=encoder, F=layer. */
#define SR_SW2_BIT       5
#define SR_ENC_BTN_BIT   4
#define SR_KEY_ACTIVE_LOW 1

static bool s_analog_powered;
static bool s_brightness_key_was_pressed;
static int16_t s_encoder_subdetent_counts;
RTC_DATA_ATTR static bool s_critical_battery_latch;

/** MAC string parse: "AA:BB:CC:DD:EE:FF" -> uint8_t[6]. */
static void parse_mac(const char *str, uint8_t mac[6])
{
    if (sscanf(str, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
               &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
        ESP_LOGW(TAG, "MAC parse hatasi, default FF kullaniliyor");
        memset(mac, 0xFF, 6);
    }
}

/**
 * @brief Tum bilesenleri baslatir (Bölüm 1.1 + 1.2).
 */
static esp_err_t init_subsystems(system_state_t *state)
{
    memset(state, 0, sizeof(*state));

    /* 1. NVS + kalici ayarlar */
    ESP_RETURN_ON_ERROR(nvs_store_init(), TAG, "nvs_store_init");

    uint8_t mode_u8 = 0;
    nvs_store_get_u8("pw_mode", CONFIG_ACTIVE_POWER_MODE_DEFAULT, &mode_u8);
    state->config.active_power_mode =
        (mode_u8 <= POWER_MODE_AGGRESSIVE) ? (power_mode_t)mode_u8 : POWER_MODE_HYBRID;
    nvs_store_get_u8("brightness", CONFIG_BASE_BRIGHTNESS_DEFAULT,
                     &state->config.base_brightness);
    if (state->config.base_brightness > 100) state->config.base_brightness = 100;
    nvs_store_get_u8("layer", CONFIG_ACTIVE_LAYER_DEFAULT,
                     &state->config.active_layer);
    if (state->config.active_layer > 3) state->config.active_layer = 0;

    /* 2. SPI bus + cevresel */
    ESP_RETURN_ON_ERROR(spi_bus_init(PIN_SPI_SCK), TAG, "spi_bus");
    ESP_RETURN_ON_ERROR(mcp3202_init(PIN_ADC_CS, PIN_SPI_MISO, PIN_SPI_MOSI), TAG, "mcp3202");
    ESP_RETURN_ON_ERROR(mux_cd74hc4067_init(PIN_MUX_S0, PIN_MUX_S1, PIN_MUX_S2, PIN_MUX_S3), TAG, "mux");
    ESP_RETURN_ON_ERROR(shift_in_sn74hc165_init(PIN_SR_LATCH, PIN_SR_DATA, SHIFT_IN_BITS), TAG, "shift_in");
    ESP_RETURN_ON_ERROR(shift_out_74hc595_init(PIN_LED_SR_RCLK, PIN_LED_SR_SER, SHIFT_OUT_BITS), TAG, "shift_out");

    /* 3. Encoder */
    ESP_RETURN_ON_ERROR(encoder_init(PIN_ENC_A, PIN_ENC_B), TAG, "encoder");

    /* 4. Neopixel + power */
    ESP_RETURN_ON_ERROR(neopixel_init(PIN_NEOPIXEL_DATA, NEOPIXEL_LED_COUNT), TAG, "neopixel");
    ESP_RETURN_ON_ERROR(power_ctrl_init(PIN_ANALOG_PWR_EN, PIN_TPS_EN), TAG, "power_ctrl");

    /* 5. Battery + charger */
    battery_init();
    ESP_RETURN_ON_ERROR(charger_if_init(PIN_VBUS_SENSE, PIN_CHG_STAT), TAG, "charger_if");

    /* 6. ESP-NOW */
    uint8_t peer_mac[6] = {0};
    parse_mac(CONFIG_S3_PEER_MAC, peer_mac);
    ESP_RETURN_ON_ERROR(
        espnow_link_init(peer_mac, CONFIG_ESPNOW_CHANNEL, CONFIG_ESPNOW_FAILSAFE_THRESHOLD),
        TAG, "espnow_link");

    /* 7. Wakeup manager */
    ESP_RETURN_ON_ERROR(wakeup_manager_init(PIN_WAKEUP_DIOT_OR, true), TAG, "wakeup");

    /* Runtime state (Bölüm 1.1) */
    state->link.is_connected_to_s3 = true;
    state->link.consecutive_failed_packets = 0;
    state->link.search_mode = false;
    state->battery_dead = s_critical_battery_latch;
    state->charging = false;
    state->charge_complete = false;
    state->inactivity_timer_ms = 0;
    state->packet_counter = 0;

    ESP_LOGI(TAG, "Tum bilesenler baslatildi. Mode=%d Bright=%u%%",
             state->config.active_power_mode, state->config.base_brightness);
    return ESP_OK;
}

/**
 * @brief Bölüm 5: Şarj modu denetimi.
 * @return true = şarj modunda (ana döngü continue), false = normal calisma.
 */
static bool step_charge_check(system_state_t *state)
{
    bool charging = charger_if_is_charging();
    if (!charging) {
        /* USB cekildi: şarj tamamlandi animasyonu varsa söndür */
        if (state->charge_complete) {
            neopixel_clear();
            power_ctrl_led_power(false);
            state->charge_complete = false;
        }
        state->charging = false;
        return false;
    }

    /* İlk şarj algilama: flag sıfırla + başlangıç animasyonu */
    if (!state->charging) {
        state->charging = true;
        charger_if_clear_battery_flags(&state->battery_flags);
        charger_if_play_charge_start_animation();
    }

    /* Şarj tamamlandi kontrolü */
    if (charger_if_is_charge_complete() && !state->charge_complete) {
        state->charge_complete = true;
        charger_if_play_charge_complete_animation();
    }
    vTaskDelay(pdMS_TO_TICKS(20)); /* Stub/charger polling de CPU'yu döndürmesin. */
    return true;  /* şarj modunda kal */
}

/**
 * @brief Bölüm 4.4: Kritik pil kilidi. battery_dead ise deep sleep.
 * @return true = deep sleep'e girildi (döngü çıkar), false = devam.
 */
static bool step_critical_battery(system_state_t *state)
{
    if (!state->battery_dead) {
        return false;
    }
    ESP_LOGW(TAG, "Kritik kilitle: deep sleep (Bölüm 4.4)");
    /* ESP-NOW + analog guc kapat */
    power_ctrl_analog_power(false);
    power_ctrl_led_power(false);
    s_critical_battery_latch = true;
    wakeup_manager_deep_sleep();  /* donmez */
    return true;
}

/**
 * @brief Bölüm 2.1.4: SW2 basili + encoder → mod degisimi.
 */
static void step_mode_change(system_state_t *state, bool sw2_pressed, int16_t enc_delta)
{
    if (!sw2_pressed || enc_delta == 0) {
        return;
    }

    int new_mode = (int)state->config.active_power_mode;
    new_mode += enc_delta;
    /* Sinirlar: 0-2 */
    if (new_mode < 0) new_mode = 0;
    if (new_mode > POWER_MODE_AGGRESSIVE) new_mode = POWER_MODE_AGGRESSIVE;

    if (new_mode != (int)state->config.active_power_mode) {
        state->config.active_power_mode = (power_mode_t)new_mode;
        nvs_store_set_u8("pw_mode", (uint8_t)new_mode);
        ESP_LOGI(TAG, "Mod degisti: %d (encoder delta=%d)", new_mode, enc_delta);
        /* Bölüm 4.5 LED geri bildirimi (3 sn bloklar) */
        power_ctrl_mode_feedback(state->config.active_power_mode,
                                 state->config.base_brightness,
                                 state->battery_flags.flag_25);
    }
}

/**
 * @brief Bölüm 2.1.5: Tuş12 basili + encoder → parlaklik degisimi.
 * @note Tuş12 = digital_buttons bit11 (U30 D girisi, aktif-LOW).
 */
static void step_brightness_change(system_state_t *state,
                                   uint16_t digital_buttons,
                                   int16_t enc_delta)
{
    bool key12_pressed = (digital_buttons & (1u << 11)) != 0;
    if (key12_pressed && enc_delta == 0) {
        s_brightness_key_was_pressed = true;
        return;
    }
    if (!key12_pressed) {
        if (s_brightness_key_was_pressed) {
            nvs_store_set_u8("brightness", state->config.base_brightness);
            s_brightness_key_was_pressed = false;
        }
        return;
    }
    if (enc_delta == 0) return;

    int b = (int)state->config.base_brightness;
    b += (int)enc_delta * 5;  /* encoder detent'i basina %5 */
    if (b < 0) b = 0;
    if (b > 100) b = 100;

    if (b != (int)state->config.base_brightness) {
        state->config.base_brightness = (uint8_t)b;
        /* LED MOSFET ac, anlik parlaklik göster */
        power_ctrl_led_power(true);
        neopixel_set_brightness((uint8_t)b);
        neopixel_fill(255, 255, 255);  /* beyaz */
        neopixel_refresh();
        ESP_LOGI(TAG, "Parlaklik: %u%%", b);
    }
    s_brightness_key_was_pressed = true;
}

/**
 * @brief Bölüm 3: Sensör okuma + paket olustur + ESP-NOW gonder.
 */
static void step_read_and_send(system_state_t *state)
{
    /* 1. Dijital tuş + SW2 + ENC_BTN (shift register) */
    uint32_t sr_val = shift_in_sn74hc165_read();
    uint16_t digital_buttons = 0;
    static const uint8_t key_bits[12] = {8, 9, 10, 11, 0, 1, 2, 3, 4, 5, 6, 7};
    for (int i = 0; i < 12; ++i) {
        bool raw = (sr_val & (1u << key_bits[i])) != 0;
        if (raw == (SR_KEY_ACTIVE_LOW == 0)) digital_buttons |= (uint16_t)(1u << i);
    }
    bool sw2_pressed = (sr_val & (1u << SR_SW2_BIT)) != 0;
    bool enc_btn_pressed = (sr_val & (1u << SR_ENC_BTN_BIT)) != 0;
    if (SR_KEY_ACTIVE_LOW) {
        sw2_pressed = !sw2_pressed;
        enc_btn_pressed = !enc_btn_pressed;
    }

    /* 2. Encoder delta */
    /* PCNT is 4x quadrature: accumulate partial edges and act once per detent. */
    s_encoder_subdetent_counts += encoder_get_delta();
    int16_t enc_delta = s_encoder_subdetent_counts / ENCODER_COUNTS_PER_DETENT;
    s_encoder_subdetent_counts -= enc_delta * ENCODER_COUNTS_PER_DETENT;

    /* 3. Mod/parlaklik degisim kontrolü (Bölüm 2.1.4 + 2.1.5) */
    step_mode_change(state, sw2_pressed, enc_delta);
    step_brightness_change(state, digital_buttons, enc_delta);

    /* 4. Analog veri (sadece analog aktifken) */
    uint16_t analog[PACKET_ANALOG_CHANNELS] = {0};
    bool search_mode = espnow_link_is_search_mode();
    bool analog_active = state->config.active_power_mode == POWER_MODE_AGGRESSIVE ||
        (state->config.active_power_mode == POWER_MODE_HYBRID &&
         (s_analog_powered || digital_buttons != 0));
    if (state->config.active_power_mode == POWER_MODE_HYBRID && digital_buttons != 0) {
        state->inactivity_timer_ms = 0;
    }
    if (analog_active && !search_mode) {
        if (!s_analog_powered) {
            power_ctrl_analog_power(true);
            s_analog_powered = true;
            /* Sensor rail and analog front-end settle after power-up. */
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        for (int ch = 0; ch < PACKET_ANALOG_CHANNELS; ch++) {
            mux_cd74hc4067_select((uint8_t)ch);
            /* settle delay (~1us yeterli ama guvenli) */
            esp_rom_delay_us(2);
            analog[ch] = mcp3202_read_channel(CONFIG_MACROPAD_ADC_CHANNEL);
        }
    } else {
        if (s_analog_powered || search_mode) power_ctrl_analog_power(false);
        s_analog_powered = false;
        analog_active = false;
    }

    /* 5. system_status olustur */
    uint8_t status = packet_make_status(
        state->config.active_power_mode,
        sw2_pressed,
        enc_btn_pressed,
        state->battery_flags.flag_15,
        state->battery_dead,
        state->charging,
        state->charge_complete);

    /* 6. Paket olustur + gonder */
    uint8_t pkt[PACKET_SIZE_FULL];
    if (analog_active && !search_mode) {
        packet_build_full(pkt, state->packet_counter, status,
                          digital_buttons, (int8_t)enc_delta, analog);
        espnow_link_send(pkt, PACKET_SIZE_FULL);
    } else {
        packet_build_fast(pkt, state->packet_counter, status,
                          digital_buttons, (int8_t)enc_delta);
        espnow_link_send(pkt, PACKET_SIZE_FAST);
    }
    state->packet_counter++;  /* u8 wrap */

    /* 7. Link state senkron */
    state->link.is_connected_to_s3 = espnow_link_is_connected();
    state->link.search_mode = espnow_link_is_search_mode();
    state->link.consecutive_failed_packets = espnow_link_get_failed_count();
}

/**
 * @brief Bölüm 4.4: Periyodik pil ölçümü (10 sn'de bir).
 */
static void step_battery_periodic(system_state_t *state, uint32_t now_ms)
{
#if !CONFIG_BATTERY_ADC_CALIBRATED || (CONFIG_BATTERY_ADC_FULL_RAW <= CONFIG_BATTERY_ADC_EMPTY_RAW)
    (void)state;
    (void)now_ms;
    return; /* Uncalibrated ADC must never cause a false critical-battery lock. */
#else
    static uint32_t last_measure_ms = 0;
    if ((now_ms - last_measure_ms) < CONFIG_BATTERY_MEASURE_PERIOD_MS) {
        return;
    }
    last_measure_ms = now_ms;

    /* Pil voltaji: sensor rayi gerekliyse kisa sure ac, olc ve kapat. */
    bool was_powered = s_analog_powered;
    if (!was_powered) {
        power_ctrl_analog_power(true);
        s_analog_powered = true;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    mux_cd74hc4067_select(MUX_CHANNEL_BATTERY);
    esp_rom_delay_us(2);
    uint16_t adc_raw = mcp3202_read_channel(CONFIG_MACROPAD_ADC_CHANNEL);
    uint8_t percent = battery_adc_to_percent(adc_raw);

    ESP_LOGI(TAG, "Pil: %u%% (adc=%u)", percent, adc_raw);
    if (battery_update(percent, state)) {
        /* battery_dead set edildi */
        ESP_LOGW(TAG, "Kritik pil! deep sleep hazirlaniyor");
    }
    if (!was_powered) {
        power_ctrl_analog_power(false);
        s_analog_powered = false;
    }
#endif
}

/**
 * @brief Bölüm 4.1/4.2: Güç moduna göre sleep.
 */
static void step_power_sleep(system_state_t *state)
{
    /* Agresif mod: sleep yok, 1 kHz */
    if (state->config.active_power_mode == POWER_MODE_AGGRESSIVE) {
        vTaskDelay(pdMS_TO_TICKS(1));  /* ~1 ms = 1 kHz */
        return;
    }

    /* Hibrit/Dijital: arama modunda 1 Hz, degilse light sleep + poll */
    if (espnow_link_is_search_mode()) {
        vTaskDelay(pdMS_TO_TICKS(1000));  /* 1 Hz */
        return;
    }

    /* Dijital mod: light sleep (Diot-OR wake + timer poll) */
    if (state->config.active_power_mode == POWER_MODE_DIGITAL) {
        power_ctrl_analog_power(false);
        wakeup_manager_light_sleep(CONFIG_WAKEUP_POLL_PERIOD_MS);
        return;
    }

    /* Hibrit mod: inaktivite varsa light sleep, yoksa 1 kHz */
    if (state->inactivity_timer_ms >= CONFIG_HYBRID_INACTIVITY_TIMEOUT_MS) {
        power_ctrl_analog_power(false);
        s_analog_powered = false;
        wakeup_manager_light_sleep(CONFIG_WAKEUP_POLL_PERIOD_MS);
        state->inactivity_timer_ms = 0;
    } else {
        vTaskDelay(pdMS_TO_TICKS(1));  /* 1 kHz */
        if (s_analog_powered) state->inactivity_timer_ms += 1;
        else wakeup_manager_light_sleep(CONFIG_WAKEUP_POLL_PERIOD_MS);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32-C6 Macropad F5 (tam) ===");
    ESP_LOGI(TAG, "IDF: %s  Target: %s", esp_get_idf_version(), CONFIG_IDF_TARGET);

    static system_state_t state;
    if (init_subsystems(&state) != ESP_OK) {
        ESP_LOGE(TAG, "Init basarisiz, sistem durduruldu");
        return;
    }

    /* Deep sleep'ten uyanma kontrolü (Bölüm 4.4 kritik kilitle) */
    if (wakeup_manager_woken_by_gpio() && state.battery_dead) {
#if CONFIG_BATTERY_ADC_CALIBRATED && (CONFIG_BATTERY_ADC_FULL_RAW > CONFIG_BATTERY_ADC_EMPTY_RAW)
        power_ctrl_analog_power(true);
        vTaskDelay(pdMS_TO_TICKS(5));
        mux_cd74hc4067_select(MUX_CHANNEL_BATTERY);
        esp_rom_delay_us(2);
        uint16_t recovery_adc = mcp3202_read_channel(CONFIG_MACROPAD_ADC_CHANNEL);
        power_ctrl_analog_power(false);
        if (battery_adc_to_percent(recovery_adc) > 5) {
            ESP_LOGI(TAG, "Pil guvenli seviyeye sarj olmus; kritik kilit kalkiyor");
            state.battery_dead = false;
            s_critical_battery_latch = false;
        } else
#endif
        {
            ESP_LOGW(TAG, "Kritik pil kilidi: katman LED'iyle uyari");
            power_ctrl_led_power(true);
            battery_blink_layer(state.config.active_layer);
            vTaskDelay(pdMS_TO_TICKS(5000));
            power_ctrl_led_power(false);
            wakeup_manager_deep_sleep();
            return;
        }
    }

    ESP_LOGI(TAG, "Ana döngü basliyor (Bölüm 2)");

    while (1) {
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);

        /* 1. Şarj denetimi (Bölüm 5) */
        if (step_charge_check(&state)) {
            continue;
        }
        state.charging = false;

        /* 2. Kritik pil (Bölüm 4.4) */
        if (step_critical_battery(&state)) {
            break;  /* deep sleep */
        }

        /* 3. Kombinasyon (Bölüm 6) - ATLANDI */

        /* 4-6. Sensör okuma + mod/parlaklik + paket (Bölüm 2.1.4, 2.1.5, 3) */
        step_read_and_send(&state);

        /* 7. Pil ölçüm (Bölüm 4.4, 10 sn) */
        step_battery_periodic(&state, now_ms);

        /* Sleep on critical battery only after the new measurement is handled. */
        if (step_critical_battery(&state)) {
            break;
        }

        /* 8. Güç modu sleep (Bölüm 4.1/4.2) */
        step_power_sleep(&state);
    }
}

// SPDX-License-Identifier: MIT-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file usb_hid.c
 * @brief TinyUSB composite USB HID: 6KRO klavye + Consumer + Gamepad (Bölüm 6).
 *
 * Tek HID interface, 3 report ID:
 *   - Report ID 1: Keyboard (6KRO, modifier + 6 keycode)
 *   - Report ID 2: Consumer Control (medya tuþlari)
 *   - Report ID 3: Gamepad (X/Y 16-bit analog eksenler)
 *
 * bInterval = 1 (1000Hz poll). Rapor sadece degisimde gonderilir (Bölüm 6.1).
 */

#include "usb_hid.h"

#include <string.h>
#include "esp_log.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"

static const char *TAG = "usb_hid";

/** Report ID'ler (HID descriptor ile eslesmeli). */
#define HID_REPORT_ID_KEYBOARD  1
#define HID_REPORT_ID_CONSUMER  2
#define HID_REPORT_ID_GAMEPAD   3

/** USB poll interval (ms) - bInterval. */
#define USB_HID_POLL_INTERVAL   1

/* =========================================================================
 * TinyUSB descriptor'lar
 * ========================================================================= */

/** HID report descriptor: Keyboard + Consumer + Gamepad (report ID'ler ile). */
static const uint8_t s_hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_REPORT_ID_KEYBOARD)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(HID_REPORT_ID_CONSUMER)),
    TUD_HID_REPORT_DESC_GAMEPAD(HID_REPORT_ID(HID_REPORT_ID_GAMEPAD)),
};

/** String descriptor'lar. */
static const char *s_string_descriptor[] = {
    (char[]){0x09, 0x04},  // 0: English
    "S3 Dongle",           // 1: Manufacturer
    "S3 Macropad",         // 2: Product
    "123456",              // 3: Serial
    "HID Interface",       // 4: HID
};

/** Configuration descriptor total length. */
#define TUSB_DESC_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

/** Configuration descriptor: 1 config, 1 HID interface. */
static const uint8_t s_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(s_hid_report_descriptor),
                       0x81, 16, USB_HID_POLL_INTERVAL),
};

/* =========================================================================
 * TinyUSB HID callback'leri (Bölüm 6)
 * ========================================================================= */

/** USB mount durumu (tud_mounted() ile sorgulanir). */
/** USB suspend durumu (Bölüm 6.3). */
static bool s_suspended = false;

/** Onceki raporlar (degisim tespiti icin, Bölüm 6.1). */
static uint8_t s_prev_keycode[6] = {0};
static uint8_t s_prev_modifier = 0;
static int16_t s_prev_joy_x = 0;
static int16_t s_prev_joy_y = 0;

/** TinyUSB: HID report descriptor isteði. */
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return s_hid_report_descriptor;
}

/** TinyUSB: GET_REPORT control isteði (kullanilmiyor). */
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type,
                                uint8_t *buffer, uint16_t reqlen)
{
    (void)instance; (void)report_id; (void)report_type;
    (void)buffer; (void)reqlen;
    return 0;
}

/** TinyUSB: SET_REPORT control isteði (kullanilmiyor). */
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                            hid_report_type_t report_type,
                            uint8_t const *buffer, uint16_t bufsize)
{
    (void)instance; (void)report_id; (void)report_type;
    (void)buffer; (void)bufsize;
}

/** TinyUSB: USB suspend (Bölüm 6.3). */
void tud_suspend_cb(bool remote_wakeup_en)
{
    s_suspended = true;
    (void)remote_wakeup_en;
    ESP_LOGI(TAG, "USB suspend (Bölüm 6.3)");
}

/** TinyUSB: USB resume (Bölüm 6.3). */
void tud_resume_cb(void)
{
    s_suspended = false;
    ESP_LOGI(TAG, "USB resume (Bölüm 6.3)");
}

/* =========================================================================
 * usb_hid API
 * ========================================================================= */

esp_err_t usb_hid_init(void)
{
    ESP_LOGI(TAG, "TinyUSB HID baslatiliyor (6KRO + Consumer + Gamepad)");

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    tusb_cfg.descriptor.device = NULL;
    tusb_cfg.descriptor.full_speed_config = s_configuration_descriptor;
    tusb_cfg.descriptor.string = s_string_descriptor;
    tusb_cfg.descriptor.string_count = sizeof(s_string_descriptor) / sizeof(s_string_descriptor[0]);
#if (TUD_OPT_HIGH_SPEED)
    tusb_cfg.descriptor.high_speed_config = s_configuration_descriptor;
#endif

    esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_driver_install hatasi: %s", esp_err_to_name(ret));
        return ret;
    }

    memset(s_prev_keycode, 0, sizeof(s_prev_keycode));
    s_prev_modifier = 0;
    s_prev_joy_x = 0;
    s_prev_joy_y = 0;
    s_suspended = false;

    ESP_LOGI(TAG, "TinyUSB HID baslatildi (bInterval=%u = %lu Hz)",
             USB_HID_POLL_INTERVAL, 1000UL / USB_HID_POLL_INTERVAL);
    return ESP_OK;
}

esp_err_t usb_hid_send_keyboard(const bool active_keys[12],
                                const uint16_t key_mappings[4][12],
                                uint8_t layer)
{
    if (active_keys == NULL || key_mappings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (layer >= 4) {
        layer = 0;
    }
    if (!tud_mounted() || s_suspended) {
        return ESP_ERR_INVALID_STATE;
    }

    /* 6KRO: maksimum 6 keycode + modifier */
    uint8_t keycode[6] = {0};
    uint8_t modifier = 0;
    uint8_t count = 0;

    for (int i = 0; i < 12 && count < 6; i++) {
        if (active_keys[i]) {
            uint16_t code = key_mappings[layer][i];
            if (code == 0) continue;

            /* Modifier keycode'lari (0x22-0x29: Ctrl/Shift/Alt/GUI) ayikla */
            if (code >= 0x22 && code <= 0x29 && code != 0) {
                modifier |= (1u << (code - 0x22));
            } else if (code >= 0x04 && code <= 0xA4) {
                keycode[count++] = (uint8_t)code;
            }
        }
    }

    /* Degisim kontrolu (Bölüm 6.1: sadece degisimde gonder) */
    if (memcmp(keycode, s_prev_keycode, 6) == 0 && modifier == s_prev_modifier) {
        return ESP_OK;  /* durum ayni, gonderme */
    }
    memcpy(s_prev_keycode, keycode, 6);
    s_prev_modifier = modifier;

    /* TinyUSB keyboard report (report ID 1) */
    if (!tud_hid_keyboard_report(HID_REPORT_ID_KEYBOARD, modifier, keycode)) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t usb_hid_send_gamepad(int16_t joy_x, int16_t joy_y)
{
    if (!tud_mounted() || s_suspended) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Degisim kontrolu */
    if (joy_x == s_prev_joy_x && joy_y == s_prev_joy_y) {
        return ESP_OK;
    }
    s_prev_joy_x = joy_x;
    s_prev_joy_y = joy_y;

    /* Gamepad report: X/Y int8 (-128..127), joystick 16-bit'i 8'e indir.
       Generic tud_hid_report kullanilir (tud_hid_gamepad_report link hatasi veriyor). */
    struct TU_ATTR_PACKED {
        int8_t x, y, z, rz, rx, ry;
        uint8_t hat;
        uint32_t buttons;
    } gp_report;
    memset(&gp_report, 0, sizeof(gp_report));
    gp_report.x = (int8_t)(joy_x >> 8);
    gp_report.y = (int8_t)(joy_y >> 8);

    if (!tud_hid_report(HID_REPORT_ID_GAMEPAD, &gp_report, sizeof(gp_report))) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t usb_hid_send_consumer(uint16_t usage)
{
    if (!tud_mounted() || s_suspended) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Consumer Control report: 16-bit usage code.
       TinyUSB'de tud_hid_consumer_report yok, generic tud_hid_report kullanilir. */
    struct __attribute__((packed)) {
        uint16_t usage;
    } consumer_report;
    consumer_report.usage = usage;

    if (!tud_hid_report(HID_REPORT_ID_CONSUMER, &consumer_report, sizeof(consumer_report))) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t usb_hid_release_all(void)
{
    if (!tud_mounted() || s_suspended) {
        return ESP_ERR_INVALID_STATE;
    }

    bool any = false;
    for (int i = 0; i < 6; i++) {
        if (s_prev_keycode[i] != 0) { any = true; break; }
    }
    if (s_prev_modifier != 0) any = true;

    if (any) {
        memset(s_prev_keycode, 0, 6);
        s_prev_modifier = 0;
        tud_hid_keyboard_report(HID_REPORT_ID_KEYBOARD, 0, NULL);
        ESP_LOGW(TAG, "Tum tuþlar birakildi (watchdog timeout)");
    }

    if (s_prev_joy_x != 0 || s_prev_joy_y != 0) {
        s_prev_joy_x = 0;
        s_prev_joy_y = 0;
        struct TU_ATTR_PACKED {
            int8_t x, y, z, rz, rx, ry;
            uint8_t hat;
            uint32_t buttons;
        } gp_report;
        memset(&gp_report, 0, sizeof(gp_report));
        tud_hid_report(HID_REPORT_ID_GAMEPAD, &gp_report, sizeof(gp_report));
    }
    return ESP_OK;
}

bool usb_hid_is_suspended(void)
{
    return s_suspended;
}

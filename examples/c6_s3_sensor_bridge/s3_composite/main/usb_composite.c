#include "usb_composite.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tusb.h"

#define REPORT_KEYBOARD 1
#define REPORT_GAMEPAD 2
#define REPORT_CONSUMER 3
static const uint8_t report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_KEYBOARD)),
    TUD_HID_REPORT_DESC_GAMEPAD(HID_REPORT_ID(REPORT_GAMEPAD)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(REPORT_CONSUMER)),
};
#define TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN)
static const uint8_t config_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 3, 0, TOTAL_LEN, 0, 100),
    TUD_CDC_DESCRIPTOR(0, 4, 0x81, 8, 0x02, 0x82, 64),
    TUD_HID_DESCRIPTOR(2, 5, false, sizeof(report_descriptor), 0x83, 16, 1),
};
static const char *strings[] = {
    (const char[]){0x09, 0x04}, "Wireless HE", "S3 Sensor Bridge Lab",
    "WHE-S3-LAB", "Sensor telemetry", "Neutral HID",
};
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance; return report_descriptor;
}
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
        hid_report_type_t type, uint8_t *buffer, uint16_t length) {
    (void)instance; (void)report_id; (void)type; (void)buffer; (void)length; return 0;
}
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
        hid_report_type_t type, uint8_t const *buffer, uint16_t length) {
    (void)instance; (void)report_id; (void)type; (void)buffer; (void)length;
}
esp_err_t whe_usb_init(void) {
    tinyusb_config_t config = TINYUSB_DEFAULT_CONFIG();
    config.descriptor.full_speed_config = config_descriptor;
    config.descriptor.string = strings;
    config.descriptor.string_count = sizeof(strings) / sizeof(strings[0]);
    return tinyusb_driver_install(&config);
}
bool whe_usb_write_line(const char *line, size_t length) {
    if (!line || !length || !tud_cdc_n_connected(0) ||
        tud_cdc_n_write_available(0) < length) return false;
    if (tud_cdc_n_write(0, line, (uint32_t)length) != length) return false;
    tud_cdc_n_write_flush(0);
    return true;
}

bool whe_usb_connected(void) { return tud_cdc_n_connected(0); }
bool whe_usb_host_mounted(void) { return tud_mounted(); }
int whe_usb_read_byte(void) { unsigned char c; return tud_cdc_n_read(0,&c,1)==1 ? c : -1; }
bool whe_usb_keyboard(const uint8_t keys[6], uint8_t modifier) {
    return tud_hid_ready() && tud_hid_keyboard_report(REPORT_KEYBOARD,modifier,keys);
}
bool whe_usb_consumer(uint16_t usage) {
    uint8_t report[2]={(uint8_t)usage,(uint8_t)(usage>>8)};
    return tud_hid_ready() && tud_hid_report(REPORT_CONSUMER,report,sizeof(report));
}

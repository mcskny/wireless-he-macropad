#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#define TUD_OPT_HIGH_SPEED 0
#define TUD_HID_REPORT_DESC_KEYBOARD(...) 1
#define TUD_HID_REPORT_DESC_CONSUMER(...) 2
#define TUD_HID_REPORT_DESC_GAMEPAD(...) 3
#define HID_REPORT_ID(x) (x)
#define TUD_CONFIG_DESC_LEN 1
#define TUD_HID_DESC_LEN 1
#define TUD_DESC_CONFIG_ATT_REMOTE_WAKEUP 1
#define TUD_CONFIG_DESCRIPTOR(...) 1
#define TUD_HID_DESCRIPTOR(...) 1
#define TU_ATTR_PACKED __attribute__((packed))
typedef struct {struct {const void *device,*full_speed_config,*high_speed_config,*string;int string_count;} descriptor;} tinyusb_config_t;
static inline int tinyusb_driver_install(const tinyusb_config_t *c){(void)c;return 0;}
bool tud_mounted(void);
bool tud_hid_keyboard_report(uint8_t,uint8_t,const uint8_t *);
bool tud_hid_report(uint8_t,const void *,uint16_t);

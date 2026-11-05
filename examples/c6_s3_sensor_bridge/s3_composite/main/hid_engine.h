#pragma once
#include <stdbool.h>
#include <stdint.h>
#define WHE_HID_KEYS 12
#define WHE_HID_TIMEOUT_US 150000

typedef struct { uint16_t released,pressed; uint8_t usage,modifiers,point,hysteresis,usage2,modifiers2,point2; bool configured,active,active2; bool rapid; uint8_t rapid_press,rapid_release,rapid_min,rapid_max; float highest,lowest; } whe_hid_key_t;
typedef struct { whe_hid_key_t keys[WHE_HID_KEYS]; bool enabled; int64_t last_valid_us; bool has_sample; bool socd_enabled; uint8_t socd_a,socd_b; int8_t socd_last; } whe_hid_engine_t;
typedef struct { uint8_t modifier, keys[6]; uint16_t consumer; } whe_hid_report_t;
void whe_hid_reset(whe_hid_engine_t *e);
bool whe_hid_set(whe_hid_engine_t *e,unsigned channel,unsigned released,unsigned pressed,unsigned usage,unsigned point,unsigned hysteresis);
bool whe_hid_set_full(whe_hid_engine_t *e,unsigned channel,unsigned released,unsigned pressed,unsigned usage,unsigned modifiers,unsigned point,unsigned hysteresis,unsigned usage2,unsigned modifiers2,unsigned point2);
bool whe_hid_enable(whe_hid_engine_t *e,bool on);
void whe_hid_sample(whe_hid_engine_t *e,const uint16_t values[WHE_HID_KEYS],int64_t now_us);
whe_hid_report_t whe_hid_report(whe_hid_engine_t *e,int64_t now_us,bool host_ready);

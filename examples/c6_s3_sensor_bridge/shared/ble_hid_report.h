// Host-independent HOGP report encoding shared with the existing S3 HID engine.
// Transport/security/subscription and delivery are the BLE backend's responsibility.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "hid_engine.h"

#define WHE_BLE_KEYBOARD_REPORT_LEN 8u
#define WHE_BLE_CONSUMER_REPORT_LEN 2u

static inline void whe_ble_encode_reports(const whe_hid_report_t *input,bool connected,
                                          uint8_t keyboard[WHE_BLE_KEYBOARD_REPORT_LEN],
                                          uint8_t consumer[WHE_BLE_CONSUMER_REPORT_LEN]){
    memset(keyboard,0,WHE_BLE_KEYBOARD_REPORT_LEN);
    memset(consumer,0,WHE_BLE_CONSUMER_REPORT_LEN);
    if(!connected || !input)return;
    keyboard[0]=input->modifier;
    memcpy(keyboard+2,input->keys,sizeof(input->keys));
    consumer[0]=(uint8_t)input->consumer;
    consumer[1]=(uint8_t)(input->consumer>>8);
}

#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
void whe_ble_start(void);
bool whe_ble_ready(void);
esp_err_t whe_ble_keyboard(uint8_t modifiers,const uint8_t keys[6]);
esp_err_t whe_ble_consumer(uint16_t usage);

void whe_ble_pairing_start(void);

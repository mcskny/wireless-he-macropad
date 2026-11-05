#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Called only for authenticated, selected BLE connection. */
bool whe_studio_apply(const uint8_t *data,size_t length);
int whe_studio_gatt_init(void);
void whe_studio_disconnected(void);
uint8_t whe_studio_current_layer(void);

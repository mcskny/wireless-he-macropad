#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
esp_err_t whe_usb_init(void);
bool whe_usb_write_line(const char *line, size_t length);

bool whe_usb_connected(void);
bool whe_usb_host_mounted(void);
int whe_usb_read_byte(void);
bool whe_usb_keyboard(const uint8_t keys[6], uint8_t modifier);
bool whe_usb_consumer(uint16_t usage);

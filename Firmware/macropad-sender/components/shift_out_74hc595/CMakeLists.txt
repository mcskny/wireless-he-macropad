# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

# Component: shift_out_74hc595
# Sorumluluk: 74HC595 layer LED (D8-D11) surme (bit-bang, ortak SCK)

idf_component_register(
    SRCS "shift_out_74hc595.c"
    INCLUDE_DIRS "include"
    REQUIRES spi_bus esp_driver_gpio
)

# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

# Component: spi_bus
# Sorumluluk: Ortak SCK pininin bit-bang yonetimi (tamamen bit-bang, HW SPI yok)

idf_component_register(
    SRCS "spi_bus.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio
)

# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

# Component: mcp3202
# Sorumluluk: MCP3202 12-bit ADC bit-bang SPI okuma (Bölüm 3.1.3)

idf_component_register(
    SRCS "mcp3202.c"
    INCLUDE_DIRS "include"
    REQUIRES spi_bus esp_driver_gpio
)

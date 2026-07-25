# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

# Component: wakeup_manager
# Sorumluluk: Light/deep sleep + GPIO wake (Diot-OR) + timer wake (polling)

idf_component_register(
    SRCS "wakeup_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_hw_support
)

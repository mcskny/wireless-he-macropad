# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

# Component: usb_power
# Sorumluluk: USB suspend/resume yonetimi + LED (Bölüm 6.3)

idf_component_register(
    SRCS "usb_power.c"
    INCLUDE_DIRS "include"
)

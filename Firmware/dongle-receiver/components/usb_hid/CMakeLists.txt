# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

# Component: usb_hid
# Sorumluluk: TinyUSB composite USB HID - 6KRO + Consumer + Gamepad (Bölüm 6)

idf_component_register(
    SRCS "usb_hid.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_tinyusb
)

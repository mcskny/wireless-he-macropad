# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-C6 Macropad. All rights reserved.

# Component: packet
# Sorumluluk: 5B/23B ESP-NOW paket olusturma + 12x12-bit bit-packing (Bölüm 3.2)

idf_component_register(
    SRCS "packet.c"
    INCLUDE_DIRS "include"
    REQUIRES system_state
)

# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

# Component: packet_decoder
# Sorumluluk: C6 5B/23B paket cozme + 12x12-bit bit-unpacking (Bölüm 3)

idf_component_register(
    SRCS "packet_decoder.c"
    INCLUDE_DIRS "include"
    REQUIRES system_state
)

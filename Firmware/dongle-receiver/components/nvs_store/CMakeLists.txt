# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

# Component: nvs_store
# Sorumluluk: NVS kalibrasyon blob + default degerler (Bölüm 1.1)

idf_component_register(
    SRCS "nvs_store.c"
    INCLUDE_DIRS "include"
    REQUIRES system_state nvs_flash
)

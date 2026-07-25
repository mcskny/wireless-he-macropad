# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

# main/ bileseni - ana durum makinesi (Bölüm 2) ve global state.
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "include"
    REQUIRES
        system_state
        esp_timer
        esp_rom
        esp_driver_gpio
        nvs_store
        espnow_rx
        packet_decoder
        hall_effect_engine
        usb_hid
        macro_engine
        layer_manager
        config_ap
        watchdog
        usb_power
)

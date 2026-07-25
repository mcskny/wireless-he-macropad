# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

# Component: system_state
# Sorumluluk: S3 dongle sistem geneli tipler (s3_mode_t, calibration_config_t,
# key_state_t, s3_system_state_t). Header-only tip saglayici.

idf_component_register(
    SRCS "system_state.c"
    INCLUDE_DIRS "include"
)

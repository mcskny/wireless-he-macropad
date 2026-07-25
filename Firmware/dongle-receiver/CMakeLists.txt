# SPDX-License-Identifier: MIT-only
# Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.
#
# ESP32-S3 Dongle (Xiao ESP32-S3) - ana proje CMake dosyasi.
# Build: idf.py build  |  Flash: idf.py flash monitor

# Minimum CMake surumu (ESP-IDF v6 gereksinimi)
cmake_minimum_required(VERSION 3.16)

# ESP-IDF build sistemini dahil et. Bu, components/ altindaki tum bilesenleri
# otomatik olarak tarar ve build'e dahil eder.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(esp32s3_dongle)

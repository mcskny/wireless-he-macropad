@echo off
setlocal enabledelayedexpansion
title ESP32 Wireless Macropad - Docker Build System

echo.
echo ===================================================
echo   WIRELESS MACROPAD - DOCKER BUILD SYSTEM
echo ===================================================
echo.

:: 1. Check if Docker daemon is running
docker info >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Docker Desktop is not running!
    echo Please start Docker Desktop and try again.
    echo.
    pause
    exit /b 1
)

:: 2. Handle "clean" argument (e.g. build_docker.bat clean)
if "%1"=="clean" (
    echo [INFO] Cleaning build directories...
    if exist "Firmware\macropad-sender\build" rmdir /s /q "Firmware\macropad-sender\build"
    if exist "Firmware\dongle-receiver\build" rmdir /s /q "Firmware\dongle-receiver\build"
    echo [OK] Build directories cleaned successfully!
    echo.
    exit /b 0
)

:: 3. Build Transmitter (Keyboard) Firmware
echo [1/2] Building Transmitter Firmware (Keyboard)...
echo ---------------------------------------------------
docker run --rm -v "%cd%:/project" -w /project espressif/idf:latest idf.py -C Firmware/macropad-sender build
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Transmitter firmware build failed!
    pause
    exit /b 1
)

echo.
:: 4. Build Receiver (Dongle) Firmware
echo [2/2] Building Receiver Firmware (Dongle)...
echo ---------------------------------------------------
docker run --rm -v "%cd%:/project" -w /project espressif/idf:latest idf.py -C Firmware/dongle-receiver build
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Receiver firmware build failed!
    pause
    exit /b 1
)

echo.
echo ===================================================
echo  SUCCESS! Both firmwares compiled cleanly.
echo ===================================================
echo.
echo Generated Binary (.bin) Locations:
echo  - Transmitter : Firmware/macropad-sender/build/
echo  - Receiver    : Firmware/dongle-receiver/build/
echo.
pause
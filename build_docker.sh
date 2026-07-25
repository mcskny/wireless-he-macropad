#!/bin/bash
set -e

echo "==================================================="
echo "  WIRELESS MACROPAD - DOCKER BUILD SYSTEM"
echo "==================================================="
echo ""

# Check if Docker is running
if ! docker info >/dev/null 2>&1; then
    echo "[ERROR] Docker daemon is not running!"
    exit 1
fi

# Handle clean argument
if [ "$1" == "clean" ]; then
    echo "[INFO] Cleaning build directories..."
    rm -rf firmware/macropad-sender/build firmware/dongle-receiver/build
    echo "[OK] Build directories cleaned successfully!"
    exit 0
fi

echo "[1/2] Building Transmitter Firmware (Keyboard)..."
echo "---------------------------------------------------"
docker run --rm -v "${PWD}:/project" -w /project espressif/idf:latest idf.py -C firmware/macropad-sender build

echo ""
echo "[2/2] Building Receiver Firmware (Dongle)..."
echo "---------------------------------------------------"
docker run --rm -v "${PWD}:/project" -w /project espressif/idf:latest idf.py -C firmware/dongle-receiver build

echo ""
echo "==================================================="
echo "  SUCCESS! Both firmwares compiled cleanly."
echo "==================================================="
echo "Generated Binaries:"
echo "  - Transmitter : firmware/macropad-sender/build/"
echo "  - Receiver    : firmware/dongle-receiver/build/"
#!/bin/bash
set -e

echo "=========================================="
echo "1/2: Building Transmitter Firmware (Keyboard)..."
echo "=========================================="
docker run --rm -v "${PWD}:/project" -w /project espressif/idf:latest idf.py -C firmware/transmitter build

echo "=========================================="
echo "2/2: Building Receiver Firmware (Dongle)..."
echo "=========================================="
docker run --rm -v "${PWD}:/project" -w /project espressif/idf:latest idf.py -C firmware/receiver build

echo "✅ Both firmwares built successfully!"
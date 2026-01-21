#!/bin/bash
# Flash script for ESP32-S3 Simple Thermostat - N16 (16MB) - Latest Build
# Usage: ./latest_flash_N16.sh [port]
# Default port: /dev/ttyACM0

PORT=${1:-/dev/ttyACM0}
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
LATEST_BUILD="$SCRIPT_DIR/N16/build_20260121-125805"

echo "[FLASH] Using port: $PORT"
echo "[FLASH] Flashing ESP32-S3 N16 (16MB) - Latest Build (20260121-125805)..."

esptool.py --chip esp32s3 --port "$PORT" --baud 460800 --before default_reset --after hard_reset write_flash -z \
    --flash_mode dio --flash_freq 80m --flash_size 16MB \
    0x0 "$LATEST_BUILD/bootloader.bin" \
    0x8000 "$LATEST_BUILD/partitions.bin" \
    0x10000 "$LATEST_BUILD/firmware.bin"

if [ $? -eq 0 ]; then
    echo "[FLASH] Successfully flashed N16!"
else
    echo "[FLASH] Flashing N16 failed!"
    exit 1
fi

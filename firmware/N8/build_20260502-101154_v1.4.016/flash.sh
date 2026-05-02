#!/bin/bash
# Flash script for ESP32-S3 Simple Thermostat - esp32-s3-wroom-1-n8 (8MB)
# Firmware version: 1.4.016
# Usage: ./flash.sh [port]
# Default port: /dev/ttyACM0

PORT=${1:-/dev/ttyACM0}
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "[FLASH] Using port: $PORT"
echo "[FLASH] Flashing ESP32-S3 esp32-s3-wroom-1-n8 (8MB), version 1.4.016, build 20260502-101154..."

esptool.py --chip esp32s3 --port "$PORT" --baud 460800 --before default_reset --after hard_reset write_flash -z \
    --flash_mode dio --flash_freq 80m --flash_size 8MB \
    0x0 "$SCRIPT_DIR/bootloader_v1.4.016_20260502-101154.bin" \
    0x8000 "$SCRIPT_DIR/partitions_v1.4.016_20260502-101154.bin" \
    0x10000 "$SCRIPT_DIR/firmware_v1.4.016_20260502-101154.bin"

if [ $? -eq 0 ]; then
    echo "[FLASH] Successfully flashed!"
else
    echo "[FLASH] Flashing failed!"
    exit 1
fi

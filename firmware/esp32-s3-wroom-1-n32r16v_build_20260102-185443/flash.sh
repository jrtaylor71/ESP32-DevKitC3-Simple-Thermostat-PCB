#!/bin/bash
# Flash script for ESP32-S3 Simple Thermostat - esp32-s3-wroom-1-n32r16v (32MB)
# Usage: ./flash.sh [port]
# Default port: /dev/ttyACM0

PORT=${1:-/dev/ttyACM0}
# Resolve symlinks to get the actual script directory
SCRIPT_PATH="$(readlink -f "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(dirname "$SCRIPT_PATH")"

echo "[FLASH] Using port: $PORT"
echo "[FLASH] Flashing ESP32-S3 esp32-s3-wroom-1-n32r16v (32MB)..."

esptool.py --chip esp32s3 --port "$PORT" --baud 460800 --before default_reset --after hard_reset write_flash -z \
    --flash_mode dio --flash_freq 80m --flash_size 32MB \
    0x0 "$SCRIPT_DIR/bootloader.bin" \
    0x8000 "$SCRIPT_DIR/partitions.bin" \
    0x10000 "$SCRIPT_DIR/firmware.bin"

if [ $? -eq 0 ]; then
    echo "[FLASH] Successfully flashed!"
else
    echo "[FLASH] Flashing failed!"
    exit 1
fi

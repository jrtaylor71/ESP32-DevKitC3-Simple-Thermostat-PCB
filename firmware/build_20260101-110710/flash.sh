#!/bin/bash
# Flash script for ESP32-S3 Simple Thermostat
# Usage: ./flash.sh [port]
# Default port: /dev/ttyACM0

PORT=${1:-/dev/ttyACM0}
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "[FLASH] Using port: $PORT"
echo "[FLASH] Flashing ESP32-S3 from ELF file..."

esptool.py --chip esp32s3 --port "$PORT" --baud 460800 --before default_reset --after hard_reset write_flash \
    --flash_mode dio --flash_freq 80m --flash_size detect \
    "$SCRIPT_DIR/firmware.elf"

if [ $? -eq 0 ]; then
    echo "[FLASH] Successfully flashed!"
else
    echo "[FLASH] Flashing failed!"
    exit 1
fi

#!/bin/bash
# Post-build firmware organization script

BUILD_DIR=".pio/build/esp32-s3-wroom-1-n16"
PROJECT_DIR="$(pwd)"
FIRMWARE_DIR="$PROJECT_DIR/firmware"

# Create firmware directory if it doesn't exist
mkdir -p "$FIRMWARE_DIR"

# Create version directory with build date and time
BUILD_TIMESTAMP=$(date +"%Y%m%d-%H%M%S")
VERSION_DIR="$FIRMWARE_DIR/build_$BUILD_TIMESTAMP"

mkdir -p "$VERSION_DIR"

# Copy bootloader, firmware, partitions, and ELF file
if [ -f "$BUILD_DIR/bootloader.bin" ]; then
    cp "$BUILD_DIR/bootloader.bin" "$VERSION_DIR/"
    echo "[POST-BUILD] Copied bootloader.bin to build_$BUILD_TIMESTAMP/"
fi

if [ -f "$BUILD_DIR/firmware.bin" ]; then
    cp "$BUILD_DIR/firmware.bin" "$VERSION_DIR/"
    echo "[POST-BUILD] Copied firmware.bin to build_$BUILD_TIMESTAMP/"
fi

if [ -f "$BUILD_DIR/partitions.bin" ]; then
    cp "$BUILD_DIR/partitions.bin" "$VERSION_DIR/"
    echo "[POST-BUILD] Copied partitions.bin to build_$BUILD_TIMESTAMP/"
fi

if [ -f "$BUILD_DIR/firmware.elf" ]; then
    cp "$BUILD_DIR/firmware.elf" "$VERSION_DIR/"
    echo "[POST-BUILD] Copied firmware.elf to build_$BUILD_TIMESTAMP/"
fi

# Create a flash script for easy esptool flashing
FLASH_SCRIPT="$VERSION_DIR/flash.sh"
cat > "$FLASH_SCRIPT" << 'FLASHEOF'
#!/bin/bash
# Flash script for ESP32-S3 Simple Thermostat
# Usage: ./flash.sh [port]
# Default port: /dev/ttyACM0

PORT=${1:-/dev/ttyACM0}
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "[FLASH] Using port: $PORT"
echo "[FLASH] Flashing ESP32-S3 from ELF file..."

esptool.py --chip esp32s3 --port "$PORT" --baud 460800 --before default_reset --after hard_reset write_flash -z \
    --flash_mode dio --flash_freq 80m --flash_size 16MB \
    0x0 "$SCRIPT_DIR/bootloader.bin" \
    0x8000 "$SCRIPT_DIR/partitions.bin" \
    0xe000 "$BOOT_APP0" \
    0x10000 "$SCRIPT_DIR/firmware.bin"

if [ $? -eq 0 ]; then
    echo "[FLASH] Successfully flashed!"
else
    echo "[FLASH] Flashing failed!"
    exit 1
fi
FLASHEOF

chmod +x "$FLASH_SCRIPT"
echo "[POST-BUILD] Created flash.sh script"

# Update latest symlinks
cd "$FIRMWARE_DIR"
rm -f latest_bootloader.bin latest_firmware.bin latest_firmware.elf latest_partitions.bin latest_flash.sh
ln -s "build_$BUILD_TIMESTAMP/bootloader.bin" latest_bootloader.bin
ln -s "build_$BUILD_TIMESTAMP/firmware.bin" latest_firmware.bin
ln -s "build_$BUILD_TIMESTAMP/firmware.elf" latest_firmware.elf
ln -s "build_$BUILD_TIMESTAMP/partitions.bin" latest_partitions.bin
ln -s "build_$BUILD_TIMESTAMP/flash.sh" latest_flash.sh

echo "[POST-BUILD] Firmware files organized in: firmware/build_$BUILD_TIMESTAMP"

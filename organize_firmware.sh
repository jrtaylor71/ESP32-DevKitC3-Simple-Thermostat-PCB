#!/bin/bash
# Post-build firmware organization script
# Organizes firmware for all ESP32-S3 variants

PROJECT_DIR="$(pwd)"
FIRMWARE_DIR="$PROJECT_DIR/firmware"

# Array of all ESP32-S3 variants to organize
VARIANTS=(
    "esp32-s3-wroom-1-n4:4MB"
    "esp32-s3-wroom-1-n8:8MB"
    "esp32-s3-wroom-1-n16:16MB"
    "esp32-s3-wroom-1-n32r16v:32MB"
)

# Create firmware directory if it doesn't exist
mkdir -p "$FIRMWARE_DIR"

# Create version directory with build date and time
BUILD_TIMESTAMP=$(date +"%Y%m%d-%H%M%S")

echo "[POST-BUILD] Organizing firmware builds from $BUILD_TIMESTAMP"

# Process each variant
for VARIANT_INFO in "${VARIANTS[@]}"; do
    IFS=':' read -r VARIANT FLASH_SIZE <<< "$VARIANT_INFO"
    BUILD_DIR=".pio/build/$VARIANT"
    
    # Check if this variant was built
    if [ ! -d "$BUILD_DIR" ]; then
        echo "[POST-BUILD] Skipping $VARIANT ($FLASH_SIZE) - not built"
        continue
    fi
    
    VERSION_DIR="$FIRMWARE_DIR/${VARIANT}_build_$BUILD_TIMESTAMP"
    mkdir -p "$VERSION_DIR"
    
    echo "[POST-BUILD] Processing $VARIANT ($FLASH_SIZE)..."
    
    # Copy bootloader, firmware, partitions, and ELF file
    if [ -f "$BUILD_DIR/bootloader.bin" ]; then
        cp "$BUILD_DIR/bootloader.bin" "$VERSION_DIR/"
    fi
    
    if [ -f "$BUILD_DIR/firmware.bin" ]; then
        cp "$BUILD_DIR/firmware.bin" "$VERSION_DIR/"
    fi
    
    if [ -f "$BUILD_DIR/partitions.bin" ]; then
        cp "$BUILD_DIR/partitions.bin" "$VERSION_DIR/"
    fi
    
    if [ -f "$BUILD_DIR/firmware.elf" ]; then
        cp "$BUILD_DIR/firmware.elf" "$VERSION_DIR/"
    fi
    
    # Create a flash script for easy esptool flashing
    FLASH_SCRIPT="$VERSION_DIR/flash.sh"
    cat > "$FLASH_SCRIPT" << FLASHEOF
#!/bin/bash
# Flash script for ESP32-S3 Simple Thermostat - $VARIANT ($FLASH_SIZE)
# Usage: ./flash.sh [port]
# Default port: /dev/ttyACM0

PORT=\${1:-/dev/ttyACM0}
SCRIPT_DIR="\$( cd "\$( dirname "\${BASH_SOURCE[0]}" )" && pwd )"

echo "[FLASH] Using port: \$PORT"
echo "[FLASH] Flashing ESP32-S3 $VARIANT ($FLASH_SIZE)..."

esptool.py --chip esp32s3 --port "\$PORT" --baud 460800 --before default_reset --after hard_reset write_flash -z \\
    --flash_mode dio --flash_freq 80m --flash_size $FLASH_SIZE \\
    0x0 "\$SCRIPT_DIR/bootloader.bin" \\
    0x8000 "\$SCRIPT_DIR/partitions.bin" \\
    0x10000 "\$SCRIPT_DIR/firmware.bin"

if [ \$? -eq 0 ]; then
    echo "[FLASH] Successfully flashed!"
else
    echo "[FLASH] Flashing failed!"
    exit 1
fi
FLASHEOF
    
    chmod +x "$FLASH_SCRIPT"
    echo "[POST-BUILD] ✓ $VARIANT ($FLASH_SIZE) organized in ${VARIANT}_build_$BUILD_TIMESTAMP/"
done

# Update latest symlinks for default variant (N16)
cd "$FIRMWARE_DIR"
DEFAULT_VARIANT="esp32-s3-wroom-1-n16"
if [ -d "${DEFAULT_VARIANT}_build_$BUILD_TIMESTAMP" ]; then
    rm -f latest_bootloader.bin latest_firmware.bin latest_firmware.elf latest_partitions.bin latest_flash.sh
    ln -s "${DEFAULT_VARIANT}_build_$BUILD_TIMESTAMP/bootloader.bin" latest_bootloader.bin
    ln -s "${DEFAULT_VARIANT}_build_$BUILD_TIMESTAMP/firmware.bin" latest_firmware.bin
    ln -s "${DEFAULT_VARIANT}_build_$BUILD_TIMESTAMP/firmware.elf" latest_firmware.elf
    ln -s "${DEFAULT_VARIANT}_build_$BUILD_TIMESTAMP/partitions.bin" latest_partitions.bin
    ln -s "${DEFAULT_VARIANT}_build_$BUILD_TIMESTAMP/flash.sh" latest_flash.sh
    echo "[POST-BUILD] Updated 'latest' symlinks to default variant (N16)"
fi

echo "[POST-BUILD] All firmware files organized!"

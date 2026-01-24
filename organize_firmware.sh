#!/bin/bash
# Post-build firmware organization script
# Organizes firmware for all ESP32-S3 variants

PROJECT_DIR="$(pwd)"
FIRMWARE_DIR="$PROJECT_DIR/firmware"

# Array of all ESP32-S3 variants to organize
VARIANTS=(
    "esp32-s3-wroom-1-n8:8MB:N8"
    "esp32-s3-wroom-1-n16:16MB:N16"
    "esp32-s3-wroom-1-n32r16v:32MB:N32"
)

# Default number of builds to keep (per variant)
KEEP_BUILDS=1

# Parse optional arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        -k|--keep)
            if [[ -n "$2" && "$2" =~ ^[0-9]+$ && "$2" -ge 1 ]]; then
                KEEP_BUILDS="$2"
                shift 2
            else
                echo "[POST-BUILD] Invalid keep value: '$2' (must be integer >= 1)"
                exit 1
            fi
            ;;
        *)
            echo "[POST-BUILD] Unknown argument: $1"
            echo "Usage: ./organize_firmware.sh [--keep N]"
            echo "  --keep, -k   Number of builds to retain per variant (default: 1)"
            exit 1
            ;;
    esac
done

# Create firmware directory if it doesn't exist
mkdir -p "$FIRMWARE_DIR"

# Create version directory with build date and time
BUILD_TIMESTAMP=$(date +"%Y%m%d-%H%M%S")

echo "[POST-BUILD] Organizing firmware builds from $BUILD_TIMESTAMP (keeping $KEEP_BUILDS per variant)"

# Process each variant
for VARIANT_INFO in "${VARIANTS[@]}"; do
    IFS=':' read -r VARIANT FLASH_SIZE CHIP_NAME <<< "$VARIANT_INFO"
    BUILD_DIR=".pio/build/$VARIANT"
    
    # Check if this variant was built
    if [ ! -d "$BUILD_DIR" ]; then
        echo "[POST-BUILD] Skipping $VARIANT ($FLASH_SIZE) - not built"
        continue
    fi
    
    # Create chip-specific directory and date/time subdirectory
    CHIP_DIR="$FIRMWARE_DIR/$CHIP_NAME"
    VERSION_DIR="$CHIP_DIR/build_$BUILD_TIMESTAMP"
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
    echo "[POST-BUILD] ✓ $VARIANT ($FLASH_SIZE) organized in $CHIP_NAME/build_$BUILD_TIMESTAMP/"
    
    # Clean up old builds - keep only N most recent ones
    BUILDS=$(ls -1d "$CHIP_DIR"/build_* 2>/dev/null | sort -r)
    BUILD_COUNT=$(echo "$BUILDS" | wc -l)
    if [ "$BUILD_COUNT" -gt "$KEEP_BUILDS" ]; then
        START_INDEX=$((KEEP_BUILDS + 1))
        BUILDS_TO_DELETE=$(echo "$BUILDS" | tail -n +"$START_INDEX")
        echo "[POST-BUILD] Cleaning old builds for $CHIP_NAME (keeping $KEEP_BUILDS, removing $((BUILD_COUNT - KEEP_BUILDS)))..."
        while IFS= read -r old_build; do
            if [ -d "$old_build" ]; then
                echo "[POST-BUILD]   Removing: $(basename "$old_build")"
                rm -rf "$old_build"
            fi
        done <<< "$BUILDS_TO_DELETE"
    fi
    
    # Create/update latest_flash script for this chip variant
    LATEST_FLASH="$FIRMWARE_DIR/latest_flash_$CHIP_NAME.sh"
    cat > "$LATEST_FLASH" << LATESTEOF
#!/bin/bash
# Flash script for ESP32-S3 Simple Thermostat - $CHIP_NAME ($FLASH_SIZE) - Latest Build
# Usage: ./latest_flash_$CHIP_NAME.sh [port]
# Default port: /dev/ttyACM0

PORT=\${1:-/dev/ttyACM0}
SCRIPT_DIR="\$( cd "\$( dirname "\${BASH_SOURCE[0]}" )" && pwd )"
LATEST_BUILD="\$SCRIPT_DIR/$CHIP_NAME/build_$BUILD_TIMESTAMP"

echo "[FLASH] Using port: \$PORT"
echo "[FLASH] Flashing ESP32-S3 $CHIP_NAME ($FLASH_SIZE) - Latest Build ($BUILD_TIMESTAMP)..."

esptool.py --chip esp32s3 --port "\$PORT" --baud 460800 --before default_reset --after hard_reset write_flash -z \\
    --flash_mode dio --flash_freq 80m --flash_size $FLASH_SIZE \\
    0x0 "\$LATEST_BUILD/bootloader.bin" \\
    0x8000 "\$LATEST_BUILD/partitions.bin" \\
    0x10000 "\$LATEST_BUILD/firmware.bin"

if [ \$? -eq 0 ]; then
    echo "[FLASH] Successfully flashed $CHIP_NAME!"
else
    echo "[FLASH] Flashing $CHIP_NAME failed!"
    exit 1
fi
LATESTEOF
    chmod +x "$LATEST_FLASH"
    echo "[POST-BUILD] Updated latest_flash_$CHIP_NAME.sh"
done

echo "[POST-BUILD] Firmware organization complete!"
echo "[POST-BUILD] To flash a specific variant, use: ./firmware/latest_flash_[N8|N16|N32].sh"

echo "[POST-BUILD] All firmware files organized!"

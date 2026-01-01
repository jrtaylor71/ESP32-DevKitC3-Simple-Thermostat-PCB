#!/bin/bash
# Build wrapper script - builds and organizes firmware for all ESP32-S3 variants
# Usage: ./build.sh [clean] [variant]
# Examples:
#   ./build.sh                    - Build all variants
#   ./build.sh clean              - Clean and build all variants
#   ./build.sh n16                - Build only 16MB variant
#   ./build.sh clean n4           - Clean and build only 4MB variant

cd "$(dirname "$0")"

CLEAN_BUILD=false
SPECIFIC_VARIANT=""

# Parse arguments
for arg in "$@"; do
    case $arg in
        clean)
            CLEAN_BUILD=true
            ;;
        n4)
            SPECIFIC_VARIANT="esp32-s3-wroom-1-n4"
            ;;
        n8)
            SPECIFIC_VARIANT="esp32-s3-wroom-1-n8"
            ;;
        n16)
            SPECIFIC_VARIANT="esp32-s3-wroom-1-n16"
            ;;
        n32r16v)
            SPECIFIC_VARIANT="esp32-s3-wroom-1-n32r16v"
            ;;
        *)
            echo "[BUILD] Unknown argument: $arg"
            echo "Usage: ./build.sh [clean] [n4|n8|n16|n32r16v]"
            exit 1
            ;;
    esac
done

if [ "$CLEAN_BUILD" = true ]; then
    echo "[BUILD] Cleaning..."
    if [ -n "$SPECIFIC_VARIANT" ]; then
        pio run --target clean -e "$SPECIFIC_VARIANT"
    else
        pio run --target clean
    fi
fi

echo "[BUILD] Building firmware..."
if [ -n "$SPECIFIC_VARIANT" ]; then
    echo "[BUILD] Building variant: $SPECIFIC_VARIANT"
    pio run -e "$SPECIFIC_VARIANT"
else
    echo "[BUILD] Building all variants..."
    pio run
fi

if [ $? -eq 0 ]; then
    echo "[BUILD] Build successful, organizing firmware..."
    ./organize_firmware.sh
else
    echo "[BUILD] Build failed!"
    exit 1
fi

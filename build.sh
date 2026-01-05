#!/bin/bash
# Build wrapper script - builds and organizes firmware for all ESP32-S3 variants
# Usage: 
#   ./build.sh              - Interactive mode (prompts for variant)
#   ./build.sh 1            - Build 8MB variant
#   ./build.sh 2            - Build 16MB variant (default)
#   ./build.sh 3            - Build 32MB variant
#   ./build.sh all          - Build all variants
#   ./build.sh 2 clean      - Clean and build 16MB variant
#   ./build.sh 1 quiet      - Build quietly (suppress output)
#   ./build.sh cleanlibs    - Remove all downloaded libraries and packages

cd "$(dirname "$0")"

CLEAN_BUILD=false
VARIANT_CHOICE=""
QUIET_BUILD=false
CLEAN_LIBS=false

# Parse arguments
for arg in "$@"; do
    case $arg in
        clean)
            CLEAN_BUILD=true
            ;;
        cleanlibs)
            CLEAN_LIBS=true
            ;;
        quiet|-q)
            QUIET_BUILD=true
            ;;
        1|2|3|all)
            VARIANT_CHOICE="$arg"
            ;;
        *)
            echo "[BUILD] Unknown argument: $arg"
            echo "Usage: ./build.sh [1-3|all] [clean] [quiet] [cleanlibs]"
            echo "  1        = 8MB variant"
            echo "  2        = 16MB variant (default)"
            echo "  3        = 32MB variant"
            echo "  all      = All variants"
            echo "  clean    = Clean before building"
            echo "  quiet    = Suppress output"
            echo "  cleanlibs = Remove all downloaded libraries and packages"
            exit 1
            ;;
    esac
done

# Handle cleanlibs - remove entire .pio directory and exit
if [ "$CLEAN_LIBS" = true ]; then
    echo "[BUILD] Removing entire .pio directory (libraries and packages)..."
    rm -rf .pio
    echo "[BUILD] Done. Next build will re-download all libraries."
    exit 0
fi

# Interactive variant selection (only if no variant specified on command line)
if [ -z "$VARIANT_CHOICE" ]; then
    echo "========================================="
    echo "  ESP32-S3 Thermostat Build Script"
    echo "========================================="
    echo ""
    echo "Select which variant to build:"
    echo "  1) 8MB  (esp32-s3-wroom-1-n8)"
    echo "  2) 16MB (esp32-s3-wroom-1-n16) [Default]"
    echo "  3) 32MB (esp32-s3-wroom-1-n32r16v)"
    echo "  4) All variants"
    echo ""
    read -p "Enter choice [1-4] (default: 2): " VARIANT_CHOICE
fi

# Set choice to default if empty
if [ -z "$VARIANT_CHOICE" ]; then
    VARIANT_CHOICE="2"
fi

SPECIFIC_VARIANT=""

case $VARIANT_CHOICE in
    1)
        SPECIFIC_VARIANT="esp32-s3-wroom-1-n8"
        echo "[BUILD] Selected: 8MB variant"
        ;;
    2)
        SPECIFIC_VARIANT="esp32-s3-wroom-1-n16"
        echo "[BUILD] Selected: 16MB variant (default)"
        ;;
    3)
        SPECIFIC_VARIANT="esp32-s3-wroom-1-n32r16v"
        echo "[BUILD] Selected: 32MB variant"
        ;;
    4|all)
        SPECIFIC_VARIANT=""
        echo "[BUILD] Selected: All variants"
        ;;
    *)
        echo "[BUILD] Invalid choice. Building default 16MB variant."
        SPECIFIC_VARIANT="esp32-s3-wroom-1-n16"
        ;;
esac

echo ""

if [ "$CLEAN_BUILD" = true ]; then
    echo "[BUILD] Cleaning..."
    if [ -n "$SPECIFIC_VARIANT" ]; then
        # Clean specific variant using -e flag
        if [ "$QUIET_BUILD" = true ]; then
            pio run --target clean -e "$SPECIFIC_VARIANT" > /dev/null 2>&1
        else
            pio run --target clean -e "$SPECIFIC_VARIANT"
        fi
    else
        # Clean all variants
        if [ "$QUIET_BUILD" = true ]; then
            pio run --target clean > /dev/null 2>&1
        else
            pio run --target clean
        fi
    fi
fi

echo "[BUILD] Building firmware..."

if [ -n "$SPECIFIC_VARIANT" ]; then
    echo "[BUILD] Building variant: $SPECIFIC_VARIANT"
    # Build specific variant using -e flag
    if [ "$QUIET_BUILD" = true ]; then
        pio run -e "$SPECIFIC_VARIANT" > /dev/null 2>&1
    else
        pio run -e "$SPECIFIC_VARIANT"
    fi
    BUILD_RESULT=$?
    
    if [ $BUILD_RESULT -ne 0 ]; then
        echo "[BUILD] Build failed!"
        exit 1
    fi
else
    echo "[BUILD] Building all variants..."
    # Build all variants (all are now uncommented in platformio.ini)
    if [ "$QUIET_BUILD" = true ]; then
        pio run > /dev/null 2>&1
    else
        pio run
    fi
    BUILD_RESULT=$?
    
    if [ $BUILD_RESULT -ne 0 ]; then
        echo "[BUILD] Build failed!"
        exit 1
    fi
fi

if [ $? -eq 0 ]; then
    echo "[BUILD] Build successful, organizing firmware..."
    ./organize_firmware.sh
else
    echo "[BUILD] Build failed!"
    exit 1
fi

#!/bin/bash
# Patch OneWire library for ESP32-S3 GPIO41 support
# 
# Issue: OneWire library v2.3.7 has hardcoded pin <= 33 check for output mode
# This prevents GPIO pins > 33 from being used on ESP32-S3 (which supports GPIO34-48)
#
# This patch is automatically applied during build, but can be run manually if needed.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=================================="
echo "OneWire ESP32-S3 GPIO41+ Patch"
echo "=================================="

# Find all OneWire library instances
ONEWIRE_FILES=$(find .pio/libdeps -name "OneWire_direct_gpio.h" 2>/dev/null)

if [ -z "$ONEWIRE_FILES" ]; then
    echo "ERROR: OneWire library not found. Run 'pio lib install' first."
    exit 1
fi

PATCHED_COUNT=0
ALREADY_PATCHED=0

for FILE in $ONEWIRE_FILES; do
    echo ""
    echo "Checking: $FILE"
    
    # Check if already patched
    if grep -q "ESP32-S3/S2/C3 support outputs on pins > 33" "$FILE"; then
        echo "  ✓ Already patched"
        ALREADY_PATCHED=$((ALREADY_PATCHED + 1))
        continue
    fi
    
    # Check if this is the line we need to patch
    if grep -q "if ( digitalPinIsValid(pin) && pin <= 33 ) // pins above 33 can be only inputs" "$FILE"; then
        echo "  → Applying patch..."
        
        # Create backup
        cp "$FILE" "$FILE.backup"
        
        # Apply patch using sed
        sed -i '/static inline __attribute__((always_inline))/,/void directModeOutput(IO_REG_TYPE pin)/{
            :a
            N
            /if ( digitalPinIsValid(pin) && pin <= 33 ) \/\/ pins above 33 can be only inputs/!ba
            s/if ( digitalPinIsValid(pin) \&\& pin <= 33 ) \/\/ pins above 33 can be only inputs/\/\/ ESP32-S3\/S2\/C3 support outputs on pins > 33, original ESP32 only up to 33\n    #if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6)\n        const uint8_t maxOutputPin = 48;  \/\/ ESP32-S3\/S2\/C3\/C6 support GPIO outputs up to pin 48\n    #else\n        const uint8_t maxOutputPin = 33;  \/\/ Original ESP32 only supports outputs up to pin 33\n    #endif\n    \n    if ( digitalPinIsValid(pin) \&\& pin <= maxOutputPin )/
        }' "$FILE"
        
        # Also update comment below
        sed -i 's/else \/\/ already validated to pins <= 33/else \/\/ pins 32-48 for ESP32-S3, or 32-33 for original ESP32/' "$FILE"
        
        echo "  ✓ Patched successfully (backup: $FILE.backup)"
        PATCHED_COUNT=$((PATCHED_COUNT + 1))
    else
        echo "  ⚠ Unexpected file format - patch may need updating"
    fi
done

echo ""
echo "=================================="
echo "Summary:"
echo "  Patched:         $PATCHED_COUNT"
echo "  Already patched: $ALREADY_PATCHED"
echo "=================================="

if [ $PATCHED_COUNT -gt 0 ]; then
    echo ""
    echo "Patch applied successfully!"
    echo "Rebuild your project: ./build.sh all"
fi

#!/bin/bash
# Build wrapper script - builds and organizes firmware
# Usage: ./build.sh [clean]

cd "$(dirname "$0")"

if [ "$1" == "clean" ]; then
    echo "[BUILD] Cleaning..."
    pio run --target clean
fi

echo "[BUILD] Building firmware..."
pio run

if [ $? -eq 0 ]; then
    echo "[BUILD] Build successful, organizing firmware..."
    ./organize_firmware.sh
else
    echo "[BUILD] Build failed!"
    exit 1
fi

@echo off
REM Flash script for ESP32-S3 Simple Thermostat - N16 (16MB) - Latest Build
REM Usage: latest_flash_N16.bat [port]
REM Default port: COM3

set PORT=%1
if "%PORT%"=="" set PORT=COM3

set SCRIPT_DIR=%~dp0
set LATEST_BUILD=%SCRIPT_DIR%N16\build_20260124-102128

echo [FLASH] Using port: %PORT%
echo [FLASH] Flashing ESP32-S3 N16 (16MB) - Latest Build (20260124-102128)...

esptool.py --chip esp32s3 --port %PORT% --baud 460800 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 16MB 0x0 "%LATEST_BUILD%\bootloader.bin" 0x8000 "%LATEST_BUILD%\partitions.bin" 0x10000 "%LATEST_BUILD%\firmware.bin"

if %errorlevel% equ 0 (
    echo [FLASH] Successfully flashed N16!
) else (
    echo [FLASH] Flashing N16 failed!
    exit /b 1
)

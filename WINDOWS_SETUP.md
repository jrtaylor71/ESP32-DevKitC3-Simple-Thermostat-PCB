# Windows Setup

This guide covers the required installs for Windows users to build and flash the ESP32-S3 thermostat firmware.

Last validated with firmware/documentation baseline v1.5.001 (May 2026).

## Required Installs

1. **Python 3.10+ (64-bit)**
   - Needed for `esptool.py` and PlatformIO.
   - During install, check **“Add Python to PATH.”**

2. **PlatformIO (Recommended)**
   - Install **Visual Studio Code**
   - Install the **PlatformIO IDE** extension
   - PlatformIO will install `esptool.py` automatically.

3. **USB-to-UART Driver (if needed)**
   - Most ESP32-S3 DevKit boards use **CP210x** or **CH340**.
   - Install the matching driver if the COM port does not appear.

## Optional Tools

- **Git for Windows** (to clone the repository)
- **Windows Terminal** (nicer CLI, optional)

## Git Bash (Optional)

If you install **Git for Windows**, you also get **Git Bash**. You can use it to run the Linux-style `.sh` flash scripts on Windows if you prefer.

Example:
- `./latest_flash_N16.sh COM4`

Notes:
- The `.bat` files are the simplest option for Windows.
- Git Bash still accepts Windows-style `COMx` ports when running the scripts.

## Verifying Installation

- Confirm Python:
  - `python --version`
- Confirm COM port shows in Device Manager:
  - **Ports (COM & LPT)** → note the COM number (e.g., COM4)

## Flashing on Windows

Use the provided batch files from the `firmware` folder:

- `latest_flash_N8.bat [COMx]`
- `latest_flash_N16.bat [COMx]`
- `latest_flash_N32.bat [COMx]`

Example:
- `latest_flash_N16.bat COM4`

If the board doesn’t respond, hold **BOOT** while connecting USB, then retry.

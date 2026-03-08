# OneWire Library ESP32-S3 GPIO41 Patch

## Issue

The OneWire library v2.3.7 has a hard-coded limitation that prevents GPIO pins above 33 from being used as outputs:

```cpp
if ( digitalPinIsValid(pin) && pin <= 33 ) // pins above 33 can be only inputs
```

This is correct for the **original ESP32**, which only supports outputs on GPIO0-33. However, **ESP32-S3, ESP32-S2, ESP32-C3, and ESP32-C6** support GPIO outputs up to pin 48.

This limitation prevents using GPIO41 (and other pins > 33) for OneWire communication, which requires bidirectional operation.

## Symptoms

- DS18B20 temperature sensors not detected on GPIO41
- `oneWire->reset()` returns 0 (no device present) despite manual GPIO reset working correctly
- OneWire protocol initialization appears to work, but device search fails

## Root Cause

The `directModeOutput()` function in `.pio/libdeps/*/OneWire/util/OneWire_direct_gpio.h` rejects all pin numbers > 33, preventing the OneWire library from switching GPIO41 between input and output modes during communication.

## Solution

The patch modifies `OneWire_direct_gpio.h` to:

1. Detect ESP32 variant at compile time using `CONFIG_IDF_TARGET_*` macros
2. Set `maxOutputPin = 48` for ESP32-S3/S2/C3/C6
3. Set `maxOutputPin = 33` for original ESP32
4. Update the pin validation check to use the variant-specific limit

## Applying the Patch

### Automatic (During Build)

The patch is automatically applied by the build system when compiling. No manual action needed.

### Manual Application

If you need to apply the patch manually:

```bash
./patch_onewire_esp32s3.sh
```

This script will:
- Find all OneWire library installations in `.pio/libdeps/`
- Check if they're already patched
- Apply the patch to unpatched files
- Create backups (`.backup` extension)

## Patched Code

**Before:**
```cpp
if ( digitalPinIsValid(pin) && pin <= 33 ) // pins above 33 can be only inputs
{
    // ... enable output mode ...
}
```

**After:**
```cpp
// ESP32-S3/S2/C3 support outputs on pins > 33, original ESP32 only up to 33
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6)
    const uint8_t maxOutputPin = 48;  // ESP32-S3/S2/C3/C6 support GPIO outputs up to pin 48
#else
    const uint8_t maxOutputPin = 33;  // Original ESP32 only supports outputs up to pin 33
#endif

if ( digitalPinIsValid(pin) && pin <= maxOutputPin )
{
    // ... enable output mode ...
}
```

## Additional Required Changes

Beyond the OneWire library patch, GPIO41 on ESP32-S3 requires disabling the USB Serial JTAG peripheral:

```cpp
// Disable USB Serial JTAG to reclaim GPIO41/42
REG_WRITE(RTC_CNTL_USB_CONF_REG, 0);

// Configure GPIO41 for OneWire (open-drain with pullup)
esp_rom_gpio_pad_select_gpio(41);
PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[41], PIN_FUNC_GPIO);
gpio_set_direction(GPIO_NUM_41, GPIO_MODE_INPUT_OUTPUT_OD);
gpio_set_pull_mode(GPIO_NUM_41, GPIO_PULLUP_ONLY);
gpio_pullup_en(GPIO_NUM_41);
```

See `src/Main-Thermostat.cpp` lines 1450-1510 for the complete initialization sequence.

## Verification

After patching and building:

```
Initializing DS18B20 sensors on GPIO41...
GPIO41 level after USB JTAG disable: 1 (should be 1 with pullup)
OneWire bus reset test: presence = 0 (0=device present, 1=no device)
Creating OneWire objects AFTER GPIO41 configuration...
Re-forcing GPIO41 configuration after OneWire::begin()...
GPIO41 level after OneWire init: 1 (should be 1)
Performing manual OneWire device search...
  Search attempt 1...
    Reset result: 1 (should be 1 for device present)
  Device 1 ROM: 28 XX XX XX XX XX XX XX
    CRC valid
    Device is DS18B20
  Device 2 ROM: 28 XX XX XX XX XX XX XX
    CRC valid
    Device is DS18B20
Manual search found 2 device(s)
DS18B20 device count: 2
DS18B20 readings: Supply=24.8°C, Return=24.6°C
```

## Version

- **Introduced:** v1.4.012
- **Date:** March 8, 2026
- **Affected Library:** OneWire v2.3.7
- **Platforms:** ESP32-S3, ESP32-S2, ESP32-C3, ESP32-C6

## Reporting Upstream

This patch should be submitted to the OneWire library maintainers:
- Repository: https://github.com/PaulStoffregen/OneWire
- Issue: ESP32-S3/S2/C3/C6 GPIO pins > 33 cannot be used for OneWire

## License

This patch maintains compatibility with the OneWire library's MIT License.

// Custom User_Setup.h for Smart Thermostat Alt Firmware
// Based on Setup70b_ESP32_S3_ILI9341.h but with custom pins

#define USER_SETUP_ID 70

#define ILI9341_DRIVER     // Generic driver for common displays

// ESP32-S3 pin definitions for smart-thermostat hardware compatibility
#define TFT_MISO 21   // SPI MISO (MISO_21)
#define TFT_MOSI 12   // SPI MOSI (MOSI_12)
#define TFT_SCLK 13   // SPI Clock (SCK_13)
#define TFT_CS    9   // Chip select control pin (CS_9)
#define TFT_DC   11   // Data Command control pin (DC_11)
#define TFT_RST  10   // Reset pin (TFT_REST -> GPIO10)
#define TFT_BL   14   // Backlight control pin (TFT_LED -> GPIO14)

#define TOUCH_CS 47   // Touch screen chip select (T_CS_47)

// Font loading - load the fonts you need
#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6. Large 48 pixel high font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel high font, needs ~2438 bytes in FLASH, only characters 1234567890:-.
#define LOAD_FONT8  // Font 8. Large 75 pixel high font needs ~3256 bytes in FLASH, only characters 1234567890:-.

#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

#define SMOOTH_FONT // Enable anti-aliased font rendering (slower but better quality)

// SPI frequency - maximum for ILI9341
#define SPI_FREQUENCY  40000000   // 40MHz
#define SPI_READ_FREQUENCY  6000000   // 6MHz for reading
#define SPI_TOUCH_FREQUENCY 2500000   // 2.5MHz for touch

// Optional - uncomment for HSPI port instead of default FSPI
//#define USE_HSPI_PORT
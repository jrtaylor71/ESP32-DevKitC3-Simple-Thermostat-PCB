/**
 * HardwarePins.h
 * 
 * Hardware pin definitions for ESP32-S3 Simple Thermostat
 * Board: ESP32-S3-DevKitC-1 V1.1 (16MB Flash, No PSRAM)
 * 
 * Centralized hardware abstraction layer for all GPIO assignments.
 * Modify this file when changing PCB layout or hardware configuration.
 */

#ifndef HARDWARE_PINS_H
#define HARDWARE_PINS_H

// =============================================================================
// SERIAL PORT DEFINITIONS
// =============================================================================

// USB Serial Ports (ESP32-S3-DevKitC-1 V1.1)
// - /dev/ttyACM0: Native USB CDC (built-in USB peripheral, no GPIO pins)
// - /dev/ttyACM1: USB-to-UART bridge chip on UART0 (GPIO43 TX, GPIO44 RX)

// UART0 - USB-to-UART Bridge (appears as /dev/ttyACM1)
#define UART0_TX_PIN 43  // Connected to USB-UART bridge chip
#define UART0_RX_PIN 44  // Connected to USB-UART bridge chip

// UART1 - LD2410 Motion Sensor (configurable)
#define LD2410_TX_PIN 16  // ESP32 TX -> LD2410 RX (data to sensor)
#define LD2410_RX_PIN 15  // ESP32 RX -> LD2410 TX (data from sensor)

// UART2 - Available but pins conflict with other peripherals
// NOTE: UART2 default pins (GPIO17 TX, GPIO18 RX) are used for buzzer and motion detect
// If UART2 is needed, reassign buzzer and motion detect pins

// =============================================================================
// TFT DISPLAY - ILI9341 (SPI Interface)
// =============================================================================

#define TFT_CS_PIN       10  // Chip select
#define TFT_DC_PIN       11  // Data/Command
#define TFT_RST_PIN      12  // Reset
#define TFT_MOSI_PIN     13  // SPI MOSI
#define TFT_SCLK_PIN     14  // SPI Clock
#define TFT_MISO_PIN     9   // SPI MISO
#define TFT_BACKLIGHT_PIN 33 // PWM backlight control

// =============================================================================
// TOUCH CONTROLLER - XPT2046 (SPI Interface, shared with TFT)
// =============================================================================

#define TOUCH_CS_PIN     21  // Touch chip select
#define TOUCH_IRQ_PIN    -1  // Touch interrupt (not used)

// =============================================================================
// I2C BUS - AHT20 Temperature/Humidity Sensor
// =============================================================================

#define I2C_SDA_PIN      36  // I2C Data
#define I2C_SCL_PIN      35  // I2C Clock

// =============================================================================
// ONEWIRE BUS - DS18B20 Hydronic Temperature Sensor
// =============================================================================

#define ONEWIRE_PIN      34  // DS18B20 data line

// =============================================================================
// RELAY OUTPUTS - HVAC Control (Active HIGH)
// =============================================================================

#define HEAT_RELAY_1_PIN  5   // Heat Stage 1
#define HEAT_RELAY_2_PIN  7   // Heat Stage 2
#define COOL_RELAY_1_PIN  6   // Cool Stage 1
#define COOL_RELAY_2_PIN  39  // Cool Stage 2
#define FAN_RELAY_PIN     4   // Fan Control

// =============================================================================
// STATUS LED OUTPUTS - PWM Capable for Dimming
// =============================================================================

#define LED_FAN_PIN      37  // Fan status LED (green)
#define LED_HEAT_PIN     38  // Heat status LED (red)
#define LED_COOL_PIN     2   // Cool status LED (blue)

// =============================================================================
// BUZZER OUTPUT - 5V Buzzer through 2N7002 MOSFET
// =============================================================================

#define BUZZER_PIN       17  // Buzzer control (PWM for tones)

// =============================================================================
// MOTION SENSOR - LD2410 24GHz mmWave Radar
// =============================================================================

#define LD2410_MOTION_PIN 18  // Motion detection output (digital)
// LD2410 serial interface uses UART1 (GPIO15 RX, GPIO16 TX) - see UART section above

// =============================================================================
// LIGHT SENSOR - LDR (Analog Input)
// =============================================================================

#define LIGHT_SENSOR_PIN  8   // Analog light level sensor

// =============================================================================
// BOOT BUTTON - Factory Reset Trigger
// =============================================================================

#define BOOT_BUTTON      0   // Built-in boot button (active LOW)

// =============================================================================
// PWM CHANNEL ASSIGNMENTS
// =============================================================================

#define PWM_CHANNEL          0  // TFT backlight
#define PWM_CHANNEL_HEAT     1  // Heat LED dimming
#define PWM_CHANNEL_COOL     2  // Cool LED dimming
#define PWM_CHANNEL_FAN      3  // Fan LED dimming
#define PWM_CHANNEL_BUZZER   4  // Buzzer tone generation

// PWM Configuration
#define PWM_FREQ         5000   // 5kHz PWM frequency
#define PWM_RESOLUTION   8      // 8-bit resolution (0-255)

// =============================================================================
// HARDWARE NOTES
// =============================================================================

/*
 * SERIAL PORT ARCHITECTURE:
 * 
 * The ESP32-S3-DevKitC-1 has TWO USB ports that appear on the host:
 * 
 * 1. /dev/ttyACM0 - Native USB CDC (USB OTG peripheral built into ESP32-S3)
 *    - Uses internal USB D+/D- lines (no external GPIO)
 *    - Best for programming and high-speed data transfer
 *    - Always available when USB cable is connected
 * 
 * 2. /dev/ttyACM1 - USB-to-UART Bridge (separate chip on DevKitC board)
 *    - Uses UART0 (GPIO43 TX, GPIO44 RX) connected to bridge chip
 *    - Traditional serial port behavior
 *    - Useful for debugging when USB CDC is unavailable
 * 
 * UART1 is configured for LD2410 sensor communication (GPIO15/16)
 * UART2 is available but GPIO17/18 are allocated to buzzer and motion detect
 * 
 * PIN CONFLICTS TO AVOID:
 * - GPIO17 (buzzer) conflicts with UART2 TX
 * - GPIO18 (motion detect) conflicts with UART2 RX
 * - If UART2 is needed, reassign buzzer and motion sensor pins
 * 
 * SPI BUS SHARING:
 * - ILI9341 display and XPT2046 touch share MOSI, MISO, SCLK
 * - Separate CS pins (TFT_CS_PIN and TOUCH_CS_PIN) for device selection
 * 
 * I2C BUS:
 * - Single I2C bus on GPIO35/36 for AHT20 sensor
 * - Can add additional I2C devices on same bus (different addresses)
 * 
 * ONEWIRE BUS:
 * - GPIO34 supports multiple DS18B20 sensors on same line
 * - Each sensor has unique 64-bit ROM address
 */

#endif // HARDWARE_PINS_H

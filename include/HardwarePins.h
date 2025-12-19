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
#define LD2410_TX_PIN 16  // ESP32 TX -> LD2410 RX (data to sensor) - LD_TX=GPIO16
#define LD2410_RX_PIN 15  // ESP32 RX -> LD2410 TX (data from sensor) - LD_RX=GPIO15

// UART2 - Available but pins conflict with other peripherals
// NOTE: UART2 default pins (GPIO17 TX, GPIO18 RX) are used for buzzer and motion detect
// If UART2 is needed, reassign buzzer and motion detect pins

// =============================================================================
// TFT DISPLAY - ILI9341 (SPI Interface)
// =============================================================================

#define TFT_CS_PIN        9  // Chip select (CS_9)
#define TFT_DC_PIN       11  // Data/Command (DC_11)
#define TFT_RST_PIN      10  // Reset (TFT_REST -> GPIO10)
#define TFT_MOSI_PIN     12  // SPI MOSI (MOSI_12)
#define TFT_SCLK_PIN     13  // SPI Clock (SCK_13)
#define TFT_MISO_PIN     21  // SPI MISO (MISO_21)
#define TFT_BACKLIGHT_PIN 14 // PWM backlight control (TFT_LED -> GPIO14)

// =============================================================================
// TOUCH CONTROLLER - XPT2046 (SPI Interface, shared with TFT)
// =============================================================================

#define TOUCH_CS_PIN     47  // Touch chip select (T_CS_47)
#define TOUCH_IRQ_PIN    48  // Touch interrupt (GPIO48)

// =============================================================================
// I2C BUS - Temperature/Humidity Sensors (AHT20, DHT11, BME280)
// =============================================================================

#define I2C_SDA_PIN      36  // I2C Data (SDA)
#define I2C_SCL_PIN      35  // I2C Clock (SCL)

// Temperature/Humidity Sensor Configuration
// - AHT20: I2C address 0x38 (uses both SDA and SCL)
// - BME280: I2C address 0x76 or 0x77 (uses both SDA and SCL)
// - DHT11: Uses GPIO35 (SCL pin) as 1-wire data line, GPIO36 unused
//
// Note: Only ONE sensor should be populated on the PCB at a time
// Sensor type is auto-detected at startup:
//   1. Try I2C sensors first (AHT20, then BME280)
//   2. If no I2C response, disable I2C and try DHT11 on GPIO35

// =============================================================================
// ONEWIRE BUS - DS18B20 Hydronic Temperature Sensor
// =============================================================================

#define ONEWIRE_PIN      41  // DS18B20 data line (GPIO41)

// =============================================================================
// RELAY OUTPUTS - HVAC Control (Active HIGH)
// =============================================================================

#define HEAT_RELAY_1_PIN  5   // Heat Stage 1 (HEAT_W_5)
#define HEAT_RELAY_2_PIN  7   // Heat Stage 2 (HEAT_W_7)
#define COOL_RELAY_1_PIN  6   // Cool Stage 1 (COOL_Y_6)
#define COOL_RELAY_2_PIN  39  // Cool Stage 2 (COOL-STAGE2_Y2_39)
#define FAN_RELAY_PIN     4   // Fan Control (FAN_G_4)
#define PUMP_RELAY_PIN    40  // Pump Control (PUMP_40)

// =============================================================================
// STATUS LED OUTPUTS - PWM Capable for Dimming
// =============================================================================

#define LED_FAN_PIN      37  // Fan status LED (green) - GPIO37
#define LED_HEAT_PIN     38  // Heat status LED (red) - GPIO38
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

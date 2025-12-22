/* 
 * Firmware Implementation
 * Copyright (c) 2025 Jonn Taylor (jrtaylor@taylordatacom.com)
 * 
 * This firmware implementation provides enhanced features including:
 * - Option C centralized display management with FreeRTOS tasks
 * - Advanced multi-stage HVAC control with intelligent staging
 * - Home Assistant integration with proper temperature precision
 * - Comprehensive MQTT support with auto-discovery
 * - Web-based configuration interface
 * - Professional dual-core architecture for ESP32-S3
 *
 * ESP32-S3 Simple Thermostat Firmware is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ESP32-S3 Simple Thermostat Firmware is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <DHT.h>
#include <Adafruit_BME280.h>
#include <TFT_eSPI.h>
#include <HTTPClient.h>
#include <PubSubClient.h> // Include the MQTT library
#include <esp_task_wdt.h> // Include the watchdog timer library
#include <time.h>
#include <ArduinoJson.h> // Include the ArduinoJson library
#include <OneWire.h>
#include <MyLD2410.h> // LD2410 radar library
#include "WebInterface.h"
#include "WebPages.h"
#include <DallasTemperature.h>
#include <Update.h> // For OTA firmware update
#include "esp_heap_caps.h" // Heap diagnostics
#include "Weather.h" // Weather integration module
#include "HardwarePins.h" // Hardware pin definitions

// Constants
const int SECONDS_PER_HOUR = 3600;
const int WDT_TIMEOUT = 10; // Watchdog timer timeout in seconds

// Temperature/Humidity Sensor Configuration
enum SensorType {
    SENSOR_NONE = 0,
    SENSOR_AHT20 = 1,
    SENSOR_DHT11 = 2,
    SENSOR_BME280 = 3
};

// Sensor instances (only one will be active based on auto-detection)
Adafruit_AHTX0 aht;
DHT dht(I2C_SCL_PIN, DHT11); // DHT11 uses GPIO35 (SCL pin) as data line
Adafruit_BME280 bme;

// Active sensor tracking
SensorType activeSensor = SENSOR_NONE;
String sensorName = "None";
float currentPressure = 0.0; // Barometric pressure (BME280 only, in hPa)

// Hardware pin definitions moved to HardwarePins.h
// BOOT_BUTTON, LD2410 pins, ONE_WIRE_BUS, LIGHT_SENSOR_PIN, TFT_BACKLIGHT_PIN all defined there

// Settings for factory reset
unsigned long bootButtonPressStart = 0; // When the boot button was pressed
const unsigned long FACTORY_RESET_PRESS_TIME = 10000; // 10 seconds in milliseconds
bool bootButtonPressed = false; // Track if boot button is being pressed

// DS18B20 sensor setup (pin defined in HardwarePins.h as ONEWIRE_PIN)
OneWire oneWire(ONEWIRE_PIN);
DallasTemperature ds18b20(&oneWire);
float hydronicTemp = 0.0;
bool hydronicHeatingEnabled = false;

// Hydronic system settings
float hydronicTempLow = 110.0; // Default low temperature for hydronic heating
float hydronicTempHigh = 130.0; // Default high temperature for hydronic heating

// Hydronic alert tracking
bool hydronicLowTempAlertSent = false; // Track if low temp alert has been sent
unsigned long lastHydronicAlertTime = 0; // Track last alert time to prevent spam
bool hydronicLockout = false; // Track hydronic safety lockout state

// Light sensor and display dimming setup (PWM constants now in HardwarePins.h)
// LIGHT_SENSOR_PIN and TFT_BACKLIGHT_PIN are defined in HardwarePins.h
// PWM_CHANNEL, PWM_CHANNEL_HEAT, PWM_CHANNEL_COOL, PWM_CHANNEL_FAN, PWM_CHANNEL_BUZZER defined in HardwarePins.h
// PWM_FREQ and PWM_RESOLUTION defined in HardwarePins.h
const int MIN_BRIGHTNESS = 30; // Minimum backlight brightness (0-255)
const int MAX_BRIGHTNESS = 255; // Maximum backlight brightness (0-255)
const int LIGHT_SENSOR_MIN = 100; // Minimum useful light sensor reading
const int LIGHT_SENSOR_MAX = 3500; // Maximum useful light sensor reading (adjust based on your sensor)
int currentBrightness = MAX_BRIGHTNESS; // Current backlight brightness
int lastLightReading = 0; // Last light sensor reading
unsigned long lastBrightnessUpdate = 0; // Last time brightness was updated
const unsigned long BRIGHTNESS_UPDATE_INTERVAL = 1000; // Update brightness every 1 second

// Display sleep mode settings
bool displaySleepEnabled = true; // Enable/disable display sleep mode
unsigned long displaySleepTimeout = 300000; // Sleep after 5 minutes (300000ms) of inactivity
bool displayIsAsleep = false; // Current display sleep state
unsigned long lastInteractionTime = 0; // Last time user interacted with display
unsigned long lastWakeTime = 0; // Last time display woke from sleep

// Motion sensor variables
MyLD2410 radar(Serial2);
bool motionDetected = false;
unsigned long lastMotionTime = 0;
bool ld2410Connected = false;
bool motionWakeEnabled = true; // Disable until sensor configuration verified working (disabled due to false positives)
unsigned long lastSleepTime = 0; // Last time display went to sleep
unsigned long radarDataTimestamp = 0; // Timestamp when radar data was last updated by readMotionSensor()
const unsigned long MOTION_WAKE_COOLDOWN = 5000; // Don't wake from motion for 5 seconds after sleep
const unsigned long MOTION_WAKE_DEBOUNCE = 2000; // Require 2 seconds of sustained motion to filter brief blips
const int MOTION_WAKE_MAX_DISTANCE = 100; // Only wake on motion within 100cm (close range only)
const unsigned long RADAR_DATA_MAX_AGE = 500; // Consider radar data stale after 500ms
const int MOTION_WAKE_MIN_SIGNAL = 50; // Minimum signal strength - increased to filter noise
const int MOTION_WAKE_MAX_SIGNAL = 100; // Maximum signal

// Weather integration settings
int weatherSource = 0; // 0=disabled, 1=OpenWeatherMap, 2=HomeAssistant
String owmApiKey = "";
String owmCity = "";
String owmState = "";
String owmCountry = "";
String haUrl = "";
String haToken = "";
String haEntityId = "";
int weatherUpdateInterval = 5; // Update interval in minutes (default 5)
Weather weather; // Weather object

// Hybrid staging settings
unsigned long stage1MinRuntime = 300; // Default minimum runtime for first stage in seconds (5 minutes)
float stage2TempDelta = 2.0; // Default temperature delta for second stage activation
unsigned long stage1StartTime = 0; // Time when stage 1 was activated
bool stage1Active = false; // Flag to track if stage 1 is active
bool stage2Active = false; // Flag to track if stage 2 is active
bool stage2HeatingEnabled = false; // Enable/disable 2nd stage heating
bool stage2CoolingEnabled = false; // Enable/disable 2nd stage cooling

// Globals
AsyncWebServer server(80);
TFT_eSPI tft = TFT_eSPI();
WiFiClient espClient;
PubSubClient mqttClient(espClient); // Initialize the MQTT client
Preferences preferences; // Preferences instance
String inputText = "";
bool isEnteringSSID = true;

// Add a new global flag to track WiFi setup mode
bool inWiFiSetupMode = false;

// Keyboard layout constants
const char* KEYBOARD_UPPER[5][10] = {
    {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
    {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},
    {"A", "S", "D", "F", "G", "H", "J", "K", "L", "DEL"},
    {"Z", "X", "C", "V", "B", "N", "M", "SPC", "CLR", "OK"},
    {"!", "@", "#", "$", "%", "^", "&", "*", "(", "SHIFT"}
};

const char* KEYBOARD_LOWER[5][10] = {
    {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
    {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"},
    {"a", "s", "d", "f", "g", "h", "j", "k", "l", "DEL"},
    {"z", "x", "c", "v", "b", "n", "m", "SPC", "CLR", "OK"},
    {"!", "@", "#", "$", "%", "^", "&", "*", "(", "SHIFT"}
};

// Keyboard layout constants
const int KEY_WIDTH = 28;
const int KEY_HEIGHT = 28;
const int KEY_SPACING = 3;
const int KEYBOARD_X_OFFSET = 15;
const int KEYBOARD_Y_OFFSET = 75;

// Dual-core task handle
TaskHandle_t sensorTask;

// GPIO pin definitions moved to HardwarePins.h for centralized hardware abstraction

// Settings
float setTempHeat = 72.0; // Default set temperature for heating in Fahrenheit
float setTempCool = 76.0; // Default set temperature for cooling in Fahrenheit
float setTempAuto = 74.0; // Default set temperature for auto mode
float tempSwing = 1.0;
float autoTempSwing = 3.0;
bool fanRelayNeeded = false;
bool useFahrenheit = true; // Default to Fahrenheit
bool mqttEnabled = false; // Default to MQTT disabled
String wifiSSID = "";
String wifiPassword = "";
int fanMinutesPerHour = 15;                                                                     // Default to 15 minutes per hour
unsigned long lastFanRunTime = 0;                                                               // Time when the fan last ran
unsigned long fanRunDuration = 0;                                                               // Duration for which the fan has run in the current hour
const float tempDifferential = 4.0; // Fixed differential between heat and cool for auto changeover
bool use24HourClock = true; // Default to 24-hour clock

// MQTT settings
String mqttServer = "0.0.0.0"; // Replace with your MQTT server
int mqttPort = 1883;                    // Replace with your MQTT port
String mqttUsername = "mqtt";  // Replace with your MQTT username
String mqttPassword = "password";  // Replace with your MQTT password
String timeZone = "CST6CDT,M3.2.0,M11.1.0"; // Default time zone (Central Standard Time)

// Add a preference for hostname
String hostname = "ESP32-S3-Simple-Thermostat"; // Default hostname

// Version control information
const String sw_version = "1.3.7"; // Software version
const String build_date = __DATE__;  // Compile date
const String build_time = __TIME__;  // Compile time
String version_info = sw_version + " (" + build_date + " " + build_time + ")";

// Modern Material Design Color Scheme
#define COLOR_BACKGROUND   0x1082    // Dark Gray #121212
#define COLOR_PRIMARY      0x1976    // Soft Blue #1976D2
#define COLOR_SECONDARY    0x0497    // Teal #0097A7
#define COLOR_ACCENT       0xFFC1    // Amber #FFC107
#define COLOR_TEXT         0xFFFF    // White #FFFFFF
#define COLOR_TEXT_LIGHT   0xE0E0    // Light Gray #E0E0E0
#define COLOR_SUCCESS      0x4CAF    // Soft Green #4CAF50
#define COLOR_WARNING      0xFF70    // Warm Orange #FF7043
#define COLOR_SURFACE      0x2124    // Slightly lighter gray #212121


bool heatingOn = false;
bool coolingOn = false;
bool fanOn = false;
String thermostatMode = "off"; // Default thermostat mode
String fanMode = "auto"; // Default fan mode

// 7-Day Scheduling System (structures defined in WebPages.h)
DaySchedule weekSchedule[7] = {
    // Sunday
    {{6, 0, 72.0, 76.0, 74.0, true}, {22, 0, 68.0, 78.0, 73.0, true}, true},
    // Monday
    {{6, 0, 72.0, 76.0, 74.0, true}, {22, 0, 68.0, 78.0, 73.0, true}, true},
    // Tuesday
    {{6, 0, 72.0, 76.0, 74.0, true}, {22, 0, 68.0, 78.0, 73.0, true}, true},
    // Wednesday
    {{6, 0, 72.0, 76.0, 74.0, true}, {22, 0, 68.0, 78.0, 73.0, true}, true},
    // Thursday
    {{6, 0, 72.0, 76.0, 74.0, true}, {22, 0, 68.0, 78.0, 73.0, true}, true},
    // Friday
    {{6, 0, 72.0, 76.0, 74.0, true}, {22, 0, 68.0, 78.0, 73.0, true}, true},
    // Saturday
    {{6, 0, 72.0, 76.0, 74.0, true}, {22, 0, 68.0, 78.0, 73.0, true}, true}
};

bool scheduleEnabled = false;        // Master schedule enable/disable
bool scheduleOverride = false;       // Temporary override active
unsigned long overrideEndTime = 0;   // When override expires (0 = permanent)
String activePeriod = "manual";      // Current active period: "day", "night", "manual"
bool scheduleUpdatedFlag = false;    // Flag to indicate schedule needs to be saved

// AHT20 sensor calibration offsets
float tempOffset = 0.0; // Temperature offset in degrees (add to reading)
float humidityOffset = 0.0; // Humidity offset in % (add to reading)

// Add these declarations at the beginning of the file or in the appropriate section for global variables
bool settingsChanged = false;
bool mqttCallbackActive = false;
unsigned long lastMQTTMessageTime = 0;
const unsigned long mqttDebounceTime = 1000; // 1 second debounce time

bool tempHeatChanged = false;
bool tempCoolChanged = false;
bool tempSwingChanged = false;
bool thermostatModeChanged = false;
bool fanModeChanged = false;
bool handlingMQTTMessage = false; // Add this flag

// Add declarations to support hydronic temperature display and sensor error checking
float previousHydronicTemp = 0.0;
bool ds18b20SensorPresent = false;

// Function prototypes
void setupWiFi();
void controlRelays(float currentTemp);
void handleWebRequests();
void updateDisplay(float currentTemp, float currentHumidity);
void saveSettings();
void loadSettings();
void setupMQTT();
void reconnectMQTT();
float convertCtoF(float celsius);
void controlFanSchedule();
void saveWiFiSettings();
void drawKeyboard(bool isUpperCaseKeyboard);
void handleKeyPress(int row, int col);
void drawButtons();
void handleButtonPress(uint16_t x, uint16_t y);
void handleKeyboardTouch(uint16_t x, uint16_t y, bool isUpperCaseKeyboard);
void connectToWiFi();
void enterWiFiCredentials();
void calibrateTouchScreen();
void restoreDefaultSettings();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void sendMQTTData();
void readLightSensor();
void updateDisplayBrightness();

// Schedule function prototypes
void checkSchedule();
void applySchedule(int dayOfWeek, bool isDayPeriod);
void saveScheduleSettings();
void loadScheduleSettings();
String getCurrentPeriod();
int getCurrentDayOfWeek();
void setBrightness(int brightness);
float getCalibratedTemperature(float rawTemp);
float getCalibratedHumidity(float rawHumidity);
void checkDisplaySleep();
void wakeDisplay();
void sleepDisplay();
bool testLD2410Connection();
bool configureLD2410Sensitivity();
void readMotionSensor();
void updateStatusLEDs();
void setHeatLED(bool state);
void setCoolLED(bool state);
void setFanLED(bool state);
void buzzerBeep(int duration = 125);
void buzzerStartupTone();
void publishHomeAssistantDiscovery();
void turnOffAllRelays();
void activateHeating();
void activateCooling();
void handleFanControl();

// Sensor abstraction function prototypes
SensorType detectSensor();
bool initializeSensor(SensorType sensor);
bool readTemperatureHumidity(float &temp, float &humidity, float &pressure);

// Option C - Centralized Display Update System
void displayUpdateTaskFunction(void* parameter);
void updateDisplayIndicators();
void setDisplayUpdateFlag();
TaskHandle_t displayUpdateTask = NULL;
bool displayUpdateRequired = false;
unsigned long displayUpdateInterval = 500; // Update every 500ms
SemaphoreHandle_t displayUpdateMutex = NULL;
SemaphoreHandle_t controlRelaysMutex = NULL;
SemaphoreHandle_t radarSensorMutex = NULL;
SemaphoreHandle_t i2cMutex = NULL; // Protect I2C bus access (AHT20 sensor)

// Diagnostics
void logRuntimeDiagnostics();

// Display indicator states (managed centrally)
struct DisplayIndicators {
    bool heatIndicator = false;
    bool coolIndicator = false;
    bool fanIndicator = false;
    bool autoIndicator = false;
    bool stage1Indicator = false;
    bool stage2Indicator = false;
    unsigned long lastUpdate = 0;
} displayIndicators;

uint16_t calibrationData[5] = { 300, 3700, 300, 3700, 7 }; // Example calibration data

float currentTemp = 0.0;
float currentHumidity = 0.0;
bool isUpperCaseKeyboard = true;
float previousTemp = 0.0;
float previousHumidity = 0.0;
float previousSetTemp = 0.0;
bool firstHourAfterBoot = true; // Flag to track the first hour after bootup
volatile bool mqttFeedbackNeeded = false; // Flag for immediate MQTT feedback on settings change

// Temperature and humidity filtering (exponential moving average)
float filteredTemp = 0.0;              // EMA-filtered temperature
float filteredHumidity = 0.0;          // EMA-filtered humidity
const float tempEMAAlpha = 0.1;        // Temperature smoothing factor (0.1 = 10% new, 90% previous)
const float humidityEMAAlpha = 0.15;   // Humidity smoothing factor (0.15 = 15% new, 85% previous)
bool firstSensorReading = true;        // Flag to initialize filters on first read

// OTA progress tracking (server-side fallback)
volatile size_t otaBytesWritten = 0;      // Bytes written so far during current OTA
volatile size_t otaTotalSize = 0;         // Total size of firmware being uploaded
volatile bool otaInProgress = false;      // True while receiving firmware data
volatile bool otaRebooting = false;       // Set after successful end before reboot
unsigned long otaStartTime = 0;           // millis() when OTA began
unsigned long otaLastUpdateLog = 0;       // For throttled serial logging

// Sensor reading task (runs on core 1)
void sensorTaskFunction(void *parameter) {
    unsigned long lastSensorError = 0;
    const unsigned long SENSOR_ERROR_COOLDOWN = 30000; // 30 second cooldown between reinits
    
    for (;;) {
        // Read sensor every 60 seconds to minimize processing load
        float tempReading, humidityReading, pressureReading;
        
        // Try to read sensor using abstraction layer
        bool readSuccess = readTemperatureHumidity(tempReading, humidityReading, pressureReading);
        
        if (!readSuccess) {
            Serial.println("[SENSOR] Read failed!");
            
            // Try to reinitialize if cooldown has passed
            unsigned long now = millis();
            if (now - lastSensorError > SENSOR_ERROR_COOLDOWN) {
                Serial.printf("[SENSOR] Attempting %s reinit...\n", sensorName.c_str());
                if (initializeSensor(activeSensor)) {
                    Serial.printf("[SENSOR] %s reinitialized successfully\n", sensorName.c_str());
                } else {
                    Serial.printf("[SENSOR] %s reinit failed\n", sensorName.c_str());
                }
                lastSensorError = now;
            }
            vTaskDelay(pdMS_TO_TICKS(60000));
            continue;
        }
        
        // Apply calibration offsets
        float calibratedTemp = getCalibratedTemperature(tempReading);
        float calibratedHumidity = getCalibratedHumidity(humidityReading);
        
        float newTemp = useFahrenheit ? (calibratedTemp * 9.0 / 5.0 + 32.0) : calibratedTemp;
        float newHumidity = calibratedHumidity;
        
        // Update globals if valid
        if (!isnan(newTemp) && !isnan(newHumidity)) {
            // Apply exponential moving average filtering for smoother readings
            if (firstSensorReading) {
                filteredTemp = newTemp;
                filteredHumidity = newHumidity;
                firstSensorReading = false;
            } else {
                // EMA filter: smoothed_value = alpha * new_value + (1 - alpha) * previous_value
                filteredTemp = (tempEMAAlpha * newTemp) + ((1.0 - tempEMAAlpha) * filteredTemp);
                filteredHumidity = (humidityEMAAlpha * newHumidity) + ((1.0 - humidityEMAAlpha) * filteredHumidity);
            }
            
            currentTemp = filteredTemp;
            currentHumidity = filteredHumidity;
            
            // Update pressure if BME280 sensor and valid reading
            if (activeSensor == SENSOR_BME280 && !isnan(pressureReading)) {
                currentPressure = pressureReading;
            }
        }
        
        // Read DS18B20 hydronic temperature sensor if present
        if (ds18b20SensorPresent) {
            ds18b20.requestTemperatures();
            float hydTempC = ds18b20.getTempCByIndex(0);
            if (hydTempC != DEVICE_DISCONNECTED_C && hydTempC != -127.0 && !isnan(hydTempC)) {
                // Valid reading - convert to Fahrenheit if needed
                hydronicTemp = useFahrenheit ? (hydTempC * 9.0 / 5.0 + 32.0) : hydTempC;
            } else {
                // Invalid reading - keep last valid reading, don't update
                Serial.println("[WARNING] DS18B20 sensor reading failed or disconnected");
            }
        }
        
        // Control HVAC relays
        controlRelays(currentTemp);
        
        // 5 second delay for responsive control while minimizing CPU load
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

// Option C: Centralized Display Update Task
void displayUpdateTaskFunction(void* parameter) {
    Serial.println("DISPLAY_TASK: Starting centralized display update task");
    
    for (;;) {
        // Check if display update is required or if enough time has passed
        bool updateNeeded = false;
        unsigned long currentTime = millis();
        
        if (xSemaphoreTake(displayUpdateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            updateNeeded = displayUpdateRequired || 
                          (currentTime - displayIndicators.lastUpdate > displayUpdateInterval);
            
            if (updateNeeded) {
                if (displayUpdateRequired) {
                    Serial.println("[DISPLAY_TASK] Flag-triggered update");
                } else {
                    Serial.println("[DISPLAY_TASK] Timer-triggered update");
                }
                displayUpdateRequired = false;  // Clear the flag
                displayIndicators.lastUpdate = currentTime;
            }
            
            xSemaphoreGive(displayUpdateMutex);
        }
        
        if (updateNeeded) {
            updateDisplayIndicators();
        }
        
        // Run every 50ms to be responsive, but only update display when needed
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

// Update display indicators based on current system state
void updateDisplayIndicators() {
    Serial.println("DISPLAY_UPDATE: Refreshing display indicators");
    
    // Take mutex to read system state safely
    if (xSemaphoreTake(displayUpdateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        // Update indicator states based on current system state
        displayIndicators.heatIndicator = (thermostatMode == "heat") || 
                                         (thermostatMode == "auto" && heatingOn);
        displayIndicators.coolIndicator = (thermostatMode == "cool") || 
                                         (thermostatMode == "auto" && coolingOn);
        displayIndicators.fanIndicator = fanOn;
        displayIndicators.autoIndicator = (thermostatMode == "auto");
        displayIndicators.stage1Indicator = stage1Active;
        displayIndicators.stage2Indicator = stage2Active;
        
        xSemaphoreGive(displayUpdateMutex);
        
        // Update physical LED indicators (hardware I/O outside mutex)
        // LEDs show ACTIVE state (what's actually running), not just mode
        setHeatLED(heatingOn);
        setCoolLED(coolingOn);  
        setFanLED(fanOn);
        
        Serial.print("DISPLAY_UPDATE: Heat=");
        Serial.print(displayIndicators.heatIndicator ? "ON" : "OFF");
        Serial.print(", Cool=");
        Serial.print(displayIndicators.coolIndicator ? "ON" : "OFF");
        Serial.print(", Fan=");
        Serial.print(displayIndicators.fanIndicator ? "ON" : "OFF");
        Serial.print(", Auto=");
        Serial.print(displayIndicators.autoIndicator ? "ON" : "OFF");
        Serial.print(", Stage1=");
        Serial.print(displayIndicators.stage1Indicator ? "ON" : "OFF");
        Serial.print(", Stage2=");
        Serial.println(displayIndicators.stage2Indicator ? "ON" : "OFF");
    } else {
        Serial.println("DISPLAY_UPDATE: Failed to take mutex, skipping update");
    }
}

// Function to request display update from other parts of the code
void setDisplayUpdateFlag() {
    if (xSemaphoreTake(displayUpdateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        displayUpdateRequired = true;
        xSemaphoreGive(displayUpdateMutex);
        Serial.println("[DISPLAY_FLAG_SET] Display update requested from controlRelays");
    } else {
        Serial.println("[DISPLAY_FLAG_FAILED] Could not acquire mutex");
    }
}

// =============================================================================
// TEMPERATURE/HUMIDITY SENSOR ABSTRACTION LAYER
// =============================================================================

// Auto-detect which sensor is connected
SensorType detectSensor() {
    Serial.println("[SENSOR] Starting sensor auto-detection...");
    
    // Initialize I2C bus first
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    delay(100);
    
    // Try AHT20 (I2C address 0x38)
    Serial.println("[SENSOR] Checking for AHT20 at I2C address 0x38...");
    if (aht.begin()) {
        Serial.println("[SENSOR] AHT20 detected!");
        return SENSOR_AHT20;
    }
    
    // Try BME280 (I2C addresses 0x76 or 0x77)
    Serial.println("[SENSOR] Checking for BME280 at I2C address 0x76...");
    if (bme.begin(0x76)) {
        Serial.println("[SENSOR] BME280 detected at address 0x76!");
        return SENSOR_BME280;
    }
    
    Serial.println("[SENSOR] Checking for BME280 at I2C address 0x77...");
    if (bme.begin(0x77)) {
        Serial.println("[SENSOR] BME280 detected at address 0x77!");
        return SENSOR_BME280;
    }
    
    // No I2C sensor found, try DHT11 on GPIO35
    Serial.println("[SENSOR] No I2C sensors found, trying DHT11...");
    Serial.println("[SENSOR] Disabling I2C, switching GPIO35 to DHT11 mode...");
    Wire.end();
    pinMode(I2C_SCL_PIN, INPUT_PULLUP); // Configure GPIO35 as regular GPIO
    dht.begin();
    delay(2000); // DHT11 needs time to stabilize
    
    // Try reading from DHT11
    float testTemp = dht.readTemperature();
    float testHum = dht.readHumidity();
    
    if (!isnan(testTemp) && !isnan(testHum)) {
        Serial.println("[SENSOR] DHT11 detected!");
        return SENSOR_DHT11;
    }
    
    Serial.println("[SENSOR] ERROR: No temperature/humidity sensor detected!");
    return SENSOR_NONE;
}

// Initialize the detected sensor
bool initializeSensor(SensorType sensor) {
    Serial.printf("[SENSOR] Initializing %s sensor...\n", 
                  sensor == SENSOR_AHT20 ? "AHT20" : 
                  sensor == SENSOR_DHT11 ? "DHT11" : 
                  sensor == SENSOR_BME280 ? "BME280" : "NONE");
    
    switch(sensor) {
        case SENSOR_AHT20:
            Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
            if (aht.begin()) {
                Serial.println("[SENSOR] AHT20 initialized successfully");
                sensorName = "AHT20";
                return true;
            }
            Serial.println("[SENSOR] AHT20 initialization failed");
            return false;
            
        case SENSOR_DHT11:
            Wire.end(); // Make sure I2C is disabled
            pinMode(I2C_SCL_PIN, INPUT_PULLUP);
            dht.begin();
            delay(2000);
            Serial.println("[SENSOR] DHT11 initialized successfully");
            sensorName = "DHT11";
            return true;
            
        case SENSOR_BME280:
            Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
            if (bme.begin(0x76) || bme.begin(0x77)) {
                // Configure BME280 for indoor monitoring
                bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                               Adafruit_BME280::SAMPLING_X2,  // temperature
                               Adafruit_BME280::SAMPLING_X16, // pressure
                               Adafruit_BME280::SAMPLING_X1,  // humidity
                               Adafruit_BME280::FILTER_X16,
                               Adafruit_BME280::STANDBY_MS_500);
                Serial.println("[SENSOR] BME280 initialized successfully");
                sensorName = "BME280";
                return true;
            }
            Serial.println("[SENSOR] BME280 initialization failed");
            return false;
            
        default:
            sensorName = "None";
            return false;
    }
}

// Read temperature, humidity, and pressure from active sensor
bool readTemperatureHumidity(float &temp, float &humidity, float &pressure) {
    pressure = NAN; // Default to NAN for sensors without pressure
    
    switch(activeSensor) {
        case SENSOR_AHT20: {
            if (i2cMutex == NULL || xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
                return false;
            }
            
            sensors_event_t humidityEvent, tempEvent;
            bool success = aht.getEvent(&humidityEvent, &tempEvent);
            xSemaphoreGive(i2cMutex);
            
            if (success) {
                temp = tempEvent.temperature;
                humidity = humidityEvent.relative_humidity;
                return true;
            }
            return false;
        }
        
        case SENSOR_DHT11: {
            // DHT11 doesn't need mutex (not on I2C bus)
            temp = dht.readTemperature();
            humidity = dht.readHumidity();
            
            if (isnan(temp) || isnan(humidity)) {
                return false;
            }
            return true;
        }
        
        case SENSOR_BME280: {
            if (i2cMutex == NULL || xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
                return false;
            }
            
            temp = bme.readTemperature();
            humidity = bme.readHumidity();
            pressure = bme.readPressure() / 100.0F; // Convert Pa to hPa
            xSemaphoreGive(i2cMutex);
            
            if (isnan(temp) || isnan(humidity)) {
                return false;
            }
            return true;
        }
        
        default:
            return false;
    }
}

// =============================================================================
// SCHEDULING SYSTEM FUNCTIONS
// =============================================================================

// Get current day of week (0 = Sunday, 1 = Monday, ..., 6 = Saturday)
int getCurrentDayOfWeek() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_wday;
}

// Get current period description
String getCurrentPeriod() {
    if (!scheduleEnabled) return "manual";
    if (scheduleOverride) return "override";
    return activePeriod;
}

// Check if we need to apply a scheduled temperature change
void checkSchedule() {
    if (!scheduleEnabled) return;
    
    // Get current time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    int currentHour = timeinfo.tm_hour;
    int currentMinute = timeinfo.tm_min;
    int currentDayOfWeek = timeinfo.tm_wday;
    
    // Check if override has expired
    if (scheduleOverride && overrideEndTime > 0 && millis() >= overrideEndTime) {
        scheduleOverride = false;
        overrideEndTime = 0;
        Serial.println("SCHEDULE: Override expired, resuming schedule");
    }
    
    // Skip if override is active
    if (scheduleOverride) return;
    
    // Skip if this day is not enabled
    if (!weekSchedule[currentDayOfWeek].enabled) return;
    
    DaySchedule& today = weekSchedule[currentDayOfWeek];
    
    // Convert current time to minutes for easier comparison
    int currentMinutes = currentHour * 60 + currentMinute;
    int dayMinutes = today.day.hour * 60 + today.day.minute;
    int nightMinutes = today.night.hour * 60 + today.night.minute;
    
    // Determine which period we should be in
    String newPeriod = "manual";
    bool shouldApplyDaySchedule = false;
    bool shouldApplyNightSchedule = false;
    
    if (dayMinutes <= nightMinutes) {
        // Normal case: day < night (e.g., 6:00 AM - 10:00 PM)
        if (currentMinutes >= dayMinutes && currentMinutes < nightMinutes) {
            newPeriod = "day";
            shouldApplyDaySchedule = true;
        } else {
            newPeriod = "night";
            shouldApplyNightSchedule = true;
        }
    } else {
        // Cross-midnight case: night < day (e.g., 10:00 PM - 6:00 AM next day)
        if (currentMinutes >= dayMinutes || currentMinutes < nightMinutes) {
            newPeriod = "day";
            shouldApplyDaySchedule = true;
        } else {
            newPeriod = "night";
            shouldApplyNightSchedule = true;
        }
    }
    
    // Apply schedule if period changed
    if (newPeriod != activePeriod) {
        activePeriod = newPeriod;
        if (shouldApplyDaySchedule && today.day.active) {
            applySchedule(currentDayOfWeek, true);
        } else if (shouldApplyNightSchedule && today.night.active) {
            applySchedule(currentDayOfWeek, false);
        }
    }
}

// Apply scheduled temperatures
void applySchedule(int dayOfWeek, bool isDayPeriod) {
    DaySchedule& schedule = weekSchedule[dayOfWeek];
    SchedulePeriod& period = isDayPeriod ? schedule.day : schedule.night;
    
    if (!period.active) return;
    
    // Apply temperatures
    setTempHeat = period.heatTemp;
    setTempCool = period.coolTemp;
    setTempAuto = period.autoTemp;
    
    Serial.printf("SCHEDULE: Applied %s schedule for day %d - Heat: %.1f°F, Cool: %.1f°F, Auto: %.1f°F\n", 
                  isDayPeriod ? "day" : "night", dayOfWeek, setTempHeat, setTempCool, setTempAuto);
    
    // Save settings and update MQTT
    saveSettings();
    if (mqttEnabled && mqttClient.connected()) {
        mqttClient.publish("thermostat/setTempHeat", String(setTempHeat).c_str(), true);
        mqttClient.publish("thermostat/setTempCool", String(setTempCool).c_str(), true);
        mqttClient.publish("thermostat/activePeriod", activePeriod.c_str(), false);
    }
    
    // Request display update
    setDisplayUpdateFlag();
}

// Save schedule settings to preferences
void saveScheduleSettings() {
    preferences.putBool("schedEnabled", scheduleEnabled);
    preferences.putBool("schedOverride", scheduleOverride);
    preferences.putULong("overrideEnd", overrideEndTime);
    preferences.putString("activePeriod", activePeriod);
    
    // Save each day's schedule
    for (int day = 0; day < 7; day++) {
        String dayPrefix = "day" + String(day) + "_";
        
        preferences.putBool((dayPrefix + "enabled").c_str(), weekSchedule[day].enabled);
        
        // Day period
        preferences.putInt((dayPrefix + "d_hour").c_str(), weekSchedule[day].day.hour);
        preferences.putInt((dayPrefix + "d_min").c_str(), weekSchedule[day].day.minute);
        preferences.putFloat((dayPrefix + "d_heat").c_str(), weekSchedule[day].day.heatTemp);
        preferences.putFloat((dayPrefix + "d_cool").c_str(), weekSchedule[day].day.coolTemp);
        preferences.putFloat((dayPrefix + "d_auto").c_str(), weekSchedule[day].day.autoTemp);
        preferences.putBool((dayPrefix + "d_active").c_str(), weekSchedule[day].day.active);
        
        // Night period
        preferences.putInt((dayPrefix + "n_hour").c_str(), weekSchedule[day].night.hour);
        preferences.putInt((dayPrefix + "n_min").c_str(), weekSchedule[day].night.minute);
        preferences.putFloat((dayPrefix + "n_heat").c_str(), weekSchedule[day].night.heatTemp);
        preferences.putFloat((dayPrefix + "n_cool").c_str(), weekSchedule[day].night.coolTemp);
        preferences.putFloat((dayPrefix + "n_auto").c_str(), weekSchedule[day].night.autoTemp);
        preferences.putBool((dayPrefix + "n_active").c_str(), weekSchedule[day].night.active);
    }
    
    Serial.println("SCHEDULE: Settings saved to preferences");
}

// Load schedule settings from preferences
void loadScheduleSettings() {
    scheduleEnabled = preferences.getBool("schedEnabled", false);
    scheduleOverride = preferences.getBool("schedOverride", false);
    overrideEndTime = preferences.getULong("overrideEnd", 0);
    activePeriod = preferences.getString("activePeriod", "manual");
    
    // Check if schedule data exists, if not initialize defaults silently
    bool scheduleExists = preferences.isKey("day0_d_heat");
    if (!scheduleExists) {
        Serial.println("SCHEDULE: First boot detected, initializing default schedule data...");
        saveScheduleSettings(); // Save the compiled-in defaults to NVS
        return; // Skip the individual loading since we just saved defaults
    }
    
    // Load each day's schedule with defaults
    for (int day = 0; day < 7; day++) {
        String dayPrefix = "day" + String(day) + "_";
        
        weekSchedule[day].enabled = preferences.getBool((dayPrefix + "enabled").c_str(), true);
        
        // Day period defaults (6:00 AM, 72°F heat, 76°F cool)
        weekSchedule[day].day.hour = preferences.getInt((dayPrefix + "d_hour").c_str(), 6);
        weekSchedule[day].day.minute = preferences.getInt((dayPrefix + "d_min").c_str(), 0);
        weekSchedule[day].day.heatTemp = preferences.getFloat((dayPrefix + "d_heat").c_str(), 72.0);
        weekSchedule[day].day.coolTemp = preferences.getFloat((dayPrefix + "d_cool").c_str(), 76.0);
        weekSchedule[day].day.autoTemp = preferences.getFloat((dayPrefix + "d_auto").c_str(), 74.0);
        weekSchedule[day].day.active = preferences.getBool((dayPrefix + "d_active").c_str(), true);
        
        // Night period defaults (10:00 PM, 68°F heat, 78°F cool)
        weekSchedule[day].night.hour = preferences.getInt((dayPrefix + "n_hour").c_str(), 22);
        weekSchedule[day].night.minute = preferences.getInt((dayPrefix + "n_min").c_str(), 0);
        weekSchedule[day].night.heatTemp = preferences.getFloat((dayPrefix + "n_heat").c_str(), 68.0);
        weekSchedule[day].night.coolTemp = preferences.getFloat((dayPrefix + "n_cool").c_str(), 78.0);
        weekSchedule[day].night.autoTemp = preferences.getFloat((dayPrefix + "n_auto").c_str(), 73.0);
        weekSchedule[day].night.active = preferences.getBool((dayPrefix + "n_active").c_str(), true);
    }
    
    Serial.printf("SCHEDULE: Settings loaded - Enabled: %s, Override: %s, Active Period: %s\n",
                  scheduleEnabled ? "YES" : "NO", 
                  scheduleOverride ? "YES" : "NO",
                  activePeriod.c_str());
}

void setup()
{
    Serial.begin(115200);
    
    // Print version information at startup
    Serial.println();
    Serial.println("========================================");
    Serial.println("ESP32-S3 Simple Thermostat");
    Serial.print("Version: ");
    Serial.println(sw_version);
    Serial.print("Build Date: ");
    Serial.println(build_date);
    Serial.print("Build Time: ");
    Serial.println(build_time);
    Serial.print("Hostname: ");
    Serial.println(hostname);
    Serial.println("========================================");
    Serial.println();

    // Initialize Preferences
    preferences.begin("thermostat", false);
    loadSettings();
    loadScheduleSettings();

    // Initialize the DHT11 sensor
    // Initialize TFT backlight with PWM (GPIO 14)
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(TFT_BACKLIGHT_PIN, PWM_CHANNEL);
    setBrightness(MAX_BRIGHTNESS); // Start at full brightness
    
    // Initialize light sensor pin
    pinMode(LIGHT_SENSOR_PIN, INPUT);
    
    // Initialize LD2410 motion sensor with pulldown to prevent floating
    pinMode(LD2410_MOTION_PIN, INPUT_PULLDOWN);
    // NOTE: Arduino Serial2.begin uses (baud, config, RX_pin, TX_pin) order
    // LD2410 TX (data out) connects to ESP32 RX (pin 15)
    // LD2410 RX (data in) connects to ESP32 TX (pin 16)
    Serial2.begin(256000, SERIAL_8N1, LD2410_RX_PIN, LD2410_TX_PIN);  // RX=15, TX=16
    delay(500); // Give sensor time to stabilize
    
    // Test LD2410 connection
    ld2410Connected = testLD2410Connection();
    if (ld2410Connected) {
        Serial.println("LD2410: Motion sensor connected successfully");
        // Configure with conservative settings matching original hardware
        configureLD2410Sensitivity();
    } else {
        Serial.println("LD2410: Motion sensor not detected - display control via touch only");
    }
    
    // Create I2C mutex BEFORE initializing I2C bus and devices
    i2cMutex = xSemaphoreCreateMutex();
    if (i2cMutex == NULL) {
        Serial.println("ERROR: Failed to create I2C mutex!");
    } else {
        Serial.println("I2C mutex created successfully");
    }
    
    // Auto-detect and initialize temperature/humidity sensor
    activeSensor = detectSensor();
    if (activeSensor != SENSOR_NONE) {
        if (!initializeSensor(activeSensor)) {
            Serial.println("ERROR: Sensor initialization failed!");
            activeSensor = SENSOR_NONE;
            sensorName = "None";
        } else {
            Serial.printf("SUCCESS: %s sensor ready\n", sensorName.c_str());
        }
    } else {
        Serial.println("ERROR: No temperature/humidity sensor detected!");
    }

    // Initialize the TFT display
    tft.init();
    tft.setRotation(1); // Set the rotation of the display as needed
    tft.fillScreen(COLOR_BACKGROUND);
    tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    tft.setTextSize(3);  // Increased size from 2 to 3
    tft.setCursor(15, 40);  // Better centered for display
    tft.println("ESP32-S3 Simple Thermostat");
    tft.setTextSize(2);  // Increased from 1 to 2 for better readability
    tft.setCursor(20, 110);  // Centered version info
    tft.println("Version: " + sw_version);
    tft.setCursor(25, 135);  // Centered build info
    tft.println("Build: " + build_date);
    tft.setCursor(40, 155);  // Centered build time
    tft.println("Time: " + build_time);
    tft.println();
    tft.setTextSize(2);
    delay(5000); // Allow time to read startup info
    tft.setCursor(60, 180);  // Center loading message
    tft.println("Loading Settings...");

    // Calibrate touch screen
    calibrateTouchScreen();

    // Initialize relay pins
    pinMode(HEAT_RELAY_1_PIN, OUTPUT);
    pinMode(HEAT_RELAY_2_PIN, OUTPUT);
    pinMode(COOL_RELAY_1_PIN, OUTPUT);
    pinMode(COOL_RELAY_2_PIN, OUTPUT);
    pinMode(FAN_RELAY_PIN, OUTPUT);

    // Initialize LED pins with PWM for dimmed operation
    ledcSetup(PWM_CHANNEL_HEAT, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_COOL, PWM_FREQ, PWM_RESOLUTION);
    ledcSetup(PWM_CHANNEL_FAN, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(LED_HEAT_PIN, PWM_CHANNEL_HEAT);
    ledcAttachPin(LED_COOL_PIN, PWM_CHANNEL_COOL);
    ledcAttachPin(LED_FAN_PIN, PWM_CHANNEL_FAN);

    // Initialize buzzer with dedicated PWM channel
    ledcSetup(PWM_CHANNEL_BUZZER, 4000, PWM_RESOLUTION); // 4kHz for buzzer
    ledcAttachPin(BUZZER_PIN, PWM_CHANNEL_BUZZER);

    // Ensure all relays are off during bootup
    digitalWrite(HEAT_RELAY_1_PIN, LOW);
    digitalWrite(HEAT_RELAY_2_PIN, LOW);
    digitalWrite(COOL_RELAY_1_PIN, LOW);
    digitalWrite(COOL_RELAY_2_PIN, LOW);
    digitalWrite(FAN_RELAY_PIN, LOW);

    // Ensure all LEDs are off during bootup
    ledcWrite(PWM_CHANNEL_HEAT, 0);
    ledcWrite(PWM_CHANNEL_COOL, 0);
    ledcWrite(PWM_CHANNEL_FAN, 0);

    // Ensure buzzer is off during bootup
    ledcWrite(PWM_CHANNEL_BUZZER, 0);

    // Load WiFi credentials but don't force connection
    wifiSSID = preferences.getString("wifiSSID", "");
    wifiPassword = preferences.getString("wifiPassword", "");
    // hostname already loaded by loadSettings() - don't override it
    WiFi.setHostname(hostname.c_str()); // Set the WiFi device name
    
    // Clear the "Loading Settings..." message
    tft.fillScreen(COLOR_BACKGROUND);
    
    // Setup WiFi if credentials exist, but don't block if it fails
    bool wifiConnected = false;
    if (wifiSSID != "" && wifiPassword != "")
    {
        WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
        unsigned long startAttemptTime = millis();
        
        // Only try to connect for 5 seconds, allowing operation without WiFi
        Serial.println("Attempting to connect to WiFi...");
        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 5000)
        {
            delay(500);
            Serial.print(".");
        }
        
        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.println("\nConnected to WiFi");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
            wifiConnected = true;
            
            // Only start web server and MQTT if connected
            handleWebRequests();
            server.begin();
            
            if (mqttEnabled)
            {
                setupMQTT();
                reconnectMQTT();
            }
            
            // Initialize weather module
            weather.begin();
            weather.setUseFahrenheit(useFahrenheit);
            weather.setSource((WeatherSource)weatherSource);
            weather.setOpenWeatherMapConfig(owmApiKey, owmCity, owmState, owmCountry);
            weather.setHomeAssistantConfig(haUrl, haToken, haEntityId);
            weather.setUpdateInterval(weatherUpdateInterval * 60000); // Convert minutes to milliseconds
            Serial.println("Weather module initialized");
            Serial.printf("Weather Source: %d (0=Disabled, 1=OpenWeatherMap, 2=HomeAssistant)\n", weatherSource);
            Serial.printf("Weather Update Interval: %d minutes\n", weatherUpdateInterval);
            if (weatherSource == 1) {
                Serial.printf("OpenWeatherMap: City=%s, State=%s, Country=%s, API Key=%s\n", 
                             owmCity.c_str(), owmState.c_str(), owmCountry.c_str(), 
                             owmApiKey.length() > 0 ? "[SET]" : "[NOT SET]");
            } else if (weatherSource == 2) {
                Serial.printf("Home Assistant: URL=%s, Entity=%s, Token=%s\n",
                             haUrl.c_str(), haEntityId.c_str(),
                             haToken.length() > 0 ? "[SET]" : "[NOT SET]");
            }
            
            // Fetch initial weather data
            if (weatherSource != 0) {
                Serial.println("Fetching initial weather data...");
                bool success = weather.update();
                Serial.printf("Initial weather fetch: %s\n", success ? "SUCCESS" : "FAILED");
                if (!success) {
                    Serial.printf("Weather error: %s\n", weather.getLastError().c_str());
                }
            }
        }
        else
        {
            Serial.println("\nFailed to connect to WiFi. Will operate offline.");
        }
    }
    else
    {
        Serial.println("No WiFi credentials found. Operating in offline mode.");
    }
    
    lastInteractionTime = millis();

    // Initialize buttons
    drawButtons();

    // Initialize time from NTP server only if WiFi is connected
    if (wifiConnected)
    {
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        setenv("TZ", timeZone.c_str(), 1);
        tzset();
    }

    // Read initial temperature and humidity
    // Read initial temperature and humidity from AHT20
    // Get initial sensor reading to initialize temperature and humidity values
    float tempReading, humidityReading, pressureReading;
    if (readTemperatureHumidity(tempReading, humidityReading, pressureReading)) {
        float calibratedTemp = getCalibratedTemperature(tempReading);
        float calibratedHumidity = getCalibratedHumidity(humidityReading);
        currentTemp = useFahrenheit ? (calibratedTemp * 9.0 / 5.0 + 32.0) : calibratedTemp;
        currentHumidity = calibratedHumidity;
        
        // Store pressure if BME280 detected
        if (activeSensor == SENSOR_BME280 && !isnan(pressureReading)) {
            currentPressure = pressureReading;
            Serial.printf("Initial pressure reading: %.1f hPa\n", currentPressure);
        }
        
        Serial.printf("Initial readings - Temp: %.1f, Humidity: %.1f%%\n", currentTemp, currentHumidity);
    } else {
        Serial.println("WARNING: Failed to get initial sensor reading");
        // Use fallback values
        currentTemp = 72.0;
        currentHumidity = 50.0;
    }
    
    // Initialize filters with first reading
    filteredTemp = currentTemp;
    filteredHumidity = currentHumidity;
    firstSensorReading = false;

    // Initial display update
    updateDisplay(currentTemp, currentHumidity);

    // Initialize display sleep timing
    lastInteractionTime = millis();

    // Initialize the DS18B20 sensor
    ds18b20.begin();
    
    // Check if DS18B20 sensor is present
    ds18b20.requestTemperatures();
    float tempC = ds18b20.getTempCByIndex(0);
    ds18b20SensorPresent = (tempC != DEVICE_DISCONNECTED_C && tempC != -127.0);
    
    if (ds18b20SensorPresent) {
        Serial.println("DS18B20 sensor detected");
    } else {
        Serial.println("DS18B20 sensor NOT detected");
    }
    
    // Create sensor task on core 1
    xTaskCreatePinnedToCore(
        sensorTaskFunction,  // Task function
        "SensorTask",       // Name
        10000,             // Stack size
        NULL,              // Parameters
        1,                 // Priority
        &sensorTask,       // Task handle
        1                  // Core 1
    );
    
    // Option C: Create display update mutex and task on core 0
    displayUpdateMutex = xSemaphoreCreateMutex();
    if (displayUpdateMutex == NULL) {
        Serial.println("ERROR: Failed to create display update mutex!");
    } else {
        Serial.println("Display update mutex created successfully");
    }
    
    // Create controlRelays mutex for thread-safe relay control
    controlRelaysMutex = xSemaphoreCreateMutex();
    if (controlRelaysMutex == NULL) {
        Serial.println("ERROR: Failed to create controlRelays mutex!");
    } else {
        Serial.println("Control relays mutex created successfully");
    }
    
    // Create radar sensor mutex for thread-safe sensor access
    radarSensorMutex = xSemaphoreCreateMutex();
    if (radarSensorMutex == NULL) {
        Serial.println("ERROR: Failed to create radar sensor mutex!");
    } else {
        Serial.println("Radar sensor mutex created successfully");
    }
    
    xTaskCreatePinnedToCore(
        displayUpdateTaskFunction,  // Task function
        "DisplayUpdateTask",       // Name
        4096,                     // Stack size (smaller than sensor task)
        NULL,                     // Parameters
        2,                        // Priority (higher than sensor task)
        &displayUpdateTask,       // Task handle
        0                         // Core 0 (same as main display operations)
    );
    
    Serial.println("Dual-core thermostat with centralized display updates setup complete");
    
    // Play startup tone to indicate setup is complete
    buzzerStartupTone();
}

void loop()
{
    static unsigned long lastWiFiAttemptTime = 0;
    static unsigned long lastMQTTAttemptTime = 0;
    static unsigned long lastDisplayUpdateTime = 0;
    static unsigned long lastSensorReadTime = 0;
    static unsigned long lastFanScheduleTime = 0;
    static unsigned long lastMQTTDataTime = 0;
    static unsigned long lastScheduleCheckTime = 0;
    static unsigned long lastDiagLogTime = 0;
    const unsigned long sensorReadInterval = 2000;    // Read sensors every 2 seconds

    // Check boot button for factory reset
    bool currentBootButtonState = digitalRead(BOOT_BUTTON) == LOW; // Boot button is active LOW
    
    // Detect boot button press
    if (currentBootButtonState && !bootButtonPressed)
    {
        bootButtonPressed = true;
        bootButtonPressStart = millis();
        Serial.println("Boot button pressed, holding for factory reset...");
    }
    
    // Detect boot button release
    if (!currentBootButtonState && bootButtonPressed)
    {
        bootButtonPressed = false;
        Serial.println("Boot button released");
    }
    
    // Check if boot button has been held long enough for factory reset
    if (bootButtonPressed && (millis() - bootButtonPressStart > FACTORY_RESET_PRESS_TIME))
    {
        Serial.println("Factory reset triggered by boot button!");
        
        // Show reset message on display
        tft.fillScreen(COLOR_BACKGROUND);
        tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
        tft.setTextSize(2);
        tft.setCursor(10, 10);
        tft.println("FACTORY RESET");
        tft.setCursor(10, 40);
        tft.println("Restoring defaults...");
        tft.setCursor(10, 70);
        tft.println("Please wait");
        
        // Restore default settings and reboot immediately
        restoreDefaultSettings();
        // ESP.restart() is called in restoreDefaultSettings()
    }

    // Get current time for all timing operations
    unsigned long currentTime = millis();

    // Feed the watchdog timer less frequently for better performance
    static unsigned long lastWatchdogTime = 0;
    if (currentTime - lastWatchdogTime > 1000) {
        esp_task_wdt_reset();
        lastWatchdogTime = currentTime;
    }

    // Handle touch input with priority - check this first for responsiveness
    uint16_t x, y;
    if (tft.getTouch(&x, &y))
    {
        // Wake display if it's asleep
        if (displayIsAsleep) {
            wakeDisplay();
            // Don't process touch input when waking from sleep, just wake up
            // Don't reset interaction time here - wakeDisplay handles it if needed
        } else if (currentTime - lastWakeTime > 500) {
            // Display is awake and we're past the wake debounce period
            // Ignore touches for 500ms after waking to prevent immediate re-sleep
            lastInteractionTime = millis();
            
            // Always handle button presses - removed serial debug for maximum speed
            handleButtonPress(x, y);
            
            // Only handle keyboard touches if we're in WiFi setup mode
            if (inWiFiSetupMode) {
                handleKeyboardTouch(x, y, isUpperCaseKeyboard);
            }
        }
    }

    // If in WiFi setup mode, skip the normal display updates and sensor readings
    if (inWiFiSetupMode) {
        // Only minimal processing in WiFi setup mode - no delays for maximum touch responsiveness
        return;
    }

    // The rest of the loop function only runs when NOT in WiFi setup mode
    // Sensor reading now handled by background task on core 1

    // Control fan based on schedule - only check every 30 seconds
    if (currentTime - lastFanScheduleTime > 30000) {
        controlFanSchedule();
        lastFanScheduleTime = currentTime;
    }

    // Check temperature schedule - only check every 60 seconds
    if (currentTime - lastScheduleCheckTime > 60000) {
        checkSchedule();
        lastScheduleCheckTime = currentTime;
    }
    
    // Update weather data if enabled and connected to WiFi
    static unsigned long lastWeatherDebug = 0;
    if (weatherSource != 0 && WiFi.status() == WL_CONNECTED) {
        bool updated = weather.update(); // Will only update if interval has passed
        
        // Debug weather status every 60 seconds
        if (millis() - lastWeatherDebug > 60000) {
            Serial.printf("WEATHER: Source=%d, Valid=%d, Temp=%.1f, Condition=%s, Error=%s\n",
                         weatherSource,
                         weather.isDataValid(),
                         weather.getData().temperature,
                         weather.getData().condition.c_str(),
                         weather.getLastError().c_str());
            lastWeatherDebug = millis();
        }
    }

    // Update display brightness based on light sensor - check every 1 second
    updateDisplayBrightness();

    // Check motion sensor every 100ms
    static unsigned long lastMotionCheck = 0;
    if (millis() - lastMotionCheck > 100) {
        lastMotionCheck = millis();
        readMotionSensor();
    }

    // Debug LD2410 status every 30 seconds
    static unsigned long lastLD2410Status = 0;
    if (millis() - lastLD2410Status > 30000) {
        lastLD2410Status = millis();
        if (ld2410Connected) {
            Serial.printf("LD2410: Status - Connected: %s, Motion: %s, Last motion: %lu ms ago\\n",
                          ld2410Connected ? "YES" : "NO",
                          motionDetected ? "ACTIVE" : "INACTIVE",
                          millis() - lastMotionTime);
        } else {
            Serial.println("LD2410: Status - Sensor not detected, display control via touch only");
        }
    }

    // Check if display should go to sleep due to inactivity
    checkDisplaySleep();

    // Periodic display updates - now called from main loop for thread safety with TFT library
    if (currentTime - lastDisplayUpdateTime > displayUpdateInterval)
    {
        updateDisplay(currentTemp, currentHumidity);
        lastDisplayUpdateTime = currentTime;
    }

    // Attempt to connect to WiFi if not connected - check every 30 seconds
    if (WiFi.status() != WL_CONNECTED && currentTime - lastWiFiAttemptTime > 30000)
    {
        connectToWiFi();
        lastWiFiAttemptTime = currentTime;
    }
    
    if (mqttEnabled)
    {
        // Attempt to reconnect to MQTT if not connected and WiFi is connected - check every 15 seconds
        if (WiFi.status() == WL_CONNECTED && !mqttClient.connected() && currentTime - lastMQTTAttemptTime > 15000)
        {
            reconnectMQTT();
            lastMQTTAttemptTime = currentTime;
        }
        mqttClient.loop();
        
        // Send MQTT feedback immediately if settings changed via MQTT
        if (mqttFeedbackNeeded && mqttClient.connected()) {
            Serial.println("[MQTT] Sending immediate feedback for settings change");
            sendMQTTData();
            mqttFeedbackNeeded = false;
            lastMQTTDataTime = currentTime;
        }
        
        // Send MQTT data only every 10 seconds instead of every loop
        if (currentTime - lastMQTTDataTime > 10000) {
            sendMQTTData();
            lastMQTTDataTime = currentTime;
        }
    }
    else
    {
        if (mqttClient.connected())
        {
            mqttClient.disconnect();
        }
    }

    // Control relays more frequently for immediate response to setting changes
    static unsigned long lastRelayControlTime = 0;
    if (currentTime - lastRelayControlTime > 1000) { // Control relays every 1 second
        controlRelays(currentTemp);
        lastRelayControlTime = currentTime;
    }

    // Periodic diagnostics: heap and stack watermarks
    if (currentTime - lastDiagLogTime > 30000) { // every 30 seconds
        logRuntimeDiagnostics();
        lastDiagLogTime = currentTime;
    }

    // LEDs are updated from state-changing functions (activateHeating, activateCooling, turnOffAllRelays)
    // No need for frequent polling - only update when state actually changes
}

void logRuntimeDiagnostics() {
    size_t free8 = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t largest8 = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    size_t minFree8 = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);

    UBaseType_t mainWatermark = uxTaskGetStackHighWaterMark(NULL);
    UBaseType_t sensorWatermark = sensorTask ? uxTaskGetStackHighWaterMark(sensorTask) : 0;
    UBaseType_t displayWatermark = displayUpdateTask ? uxTaskGetStackHighWaterMark(displayUpdateTask) : 0;

    Serial.printf("[DIAG] Heap: free=%uB, largest=%uB, min_free=%uB\n",
                  (unsigned)free8, (unsigned)largest8, (unsigned)minFree8);
    Serial.printf("[DIAG] Stack HWM (words): main=%lu, sensor=%lu, display=%lu\n",
                  (unsigned long)mainWatermark,
                  (unsigned long)sensorWatermark,
                  (unsigned long)displayWatermark);
}

void setupWiFi()
{
    // hostname already loaded by loadSettings() - don't override it with wrong key
    WiFi.setHostname(hostname.c_str()); // Set the WiFi device name

    wifiSSID = preferences.getString("wifiSSID", "");
    wifiPassword = preferences.getString("wifiPassword", "");

    if (wifiSSID != "" && wifiPassword != "")
    {
        WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
        unsigned long startAttemptTime = millis();

        // Only try to connect for 10 seconds
        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000)
        {
            delay(1000);
            Serial.println("Connecting to WiFi...");
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.println("Connected to WiFi");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
        }
        else
        {
            Serial.println("Failed to connect to WiFi");
            enterWiFiCredentials();
        }
    }
    else
    {
        // No WiFi credentials found, prompt user to enter them via touch screen
        Serial.println("No WiFi credentials found. Please enter them via the touch screen.");
        enterWiFiCredentials();
    }
}

void connectToWiFi()
{
    if (wifiSSID != "" && wifiPassword != "")
    {
        Serial.print("Connecting to WiFi with SSID: ");
        Serial.println(wifiSSID);
        Serial.print("Password: ");
        Serial.println(wifiPassword);

        WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
        unsigned long startAttemptTime = millis();

        // Only try to connect for 10 seconds
        while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000)
        {
            delay(1000);
            Serial.println("Connecting to WiFi...");
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.println("Connected to WiFi");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
        }
        else
        {
            Serial.println("Failed to connect to WiFi");
            // Don't enter WiFi credentials mode here, just return
            // This allows the device to keep operating without WiFi
        }
    }
    else
    {
        // Having no credentials is fine - don't trigger WiFi setup automatically
        Serial.println("No WiFi credentials found. Device operating in offline mode.");
        // Note: User can press the WiFi button on the display to configure WiFi if desired
    }
}

void enterWiFiCredentials()
{
    // Make sure we clear the screen completely before drawing the keyboard
    tft.fillScreen(COLOR_BACKGROUND);
    
    // Reset input text and set initial state to entering SSID
    inputText = "";
    isEnteringSSID = true;
    
    // Draw the keyboard with the screen freshly cleared
    drawKeyboard(isUpperCaseKeyboard);
    
    while (WiFi.status() != WL_CONNECTED)
    {
        uint16_t x, y;
        if (tft.getTouch(&x, &y))
        {
            handleKeyboardTouch(x, y, isUpperCaseKeyboard);
        }
        delay(100); // Reduced delay for more responsive touch handling
        
        // Periodically print status message
        static unsigned long lastStatusPrint = 0;
        unsigned long currentTime = millis();
        if (currentTime - lastStatusPrint > 5000) {
            Serial.println("Waiting for WiFi credentials...");
            lastStatusPrint = currentTime;
        }
    }
}

void drawKeyboard(bool isUpperCaseKeyboard)
{
    // Clear the entire screen first to prevent any overlapping elements
    tft.fillScreen(COLOR_BACKGROUND);
    
    // Draw header with better styling
    tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println(isEnteringSSID ? "Enter SSID:" : "Enter Password:");
    
    // Draw input text area with border
    tft.drawRect(5, 35, 310, 30, COLOR_TEXT);
    tft.fillRect(6, 36, 308, 28, COLOR_BACKGROUND);
    tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    tft.setTextSize(2);
    tft.setCursor(10, 42);
    tft.println(inputText);

    // Select keyboard layout
    const char* (*keys)[10] = isUpperCaseKeyboard ? KEYBOARD_UPPER : KEYBOARD_LOWER;

    // Draw keyboard with improved styling
    tft.setTextSize(1);
    for (int row = 0; row < 5; row++)
    {
        for (int col = 0; col < 10; col++)
        {
            int x = col * (KEY_WIDTH + KEY_SPACING) + KEYBOARD_X_OFFSET;
            int y = row * (KEY_HEIGHT + KEY_SPACING) + KEYBOARD_Y_OFFSET;
            
            // Choose key color based on function
            uint16_t keyColor = COLOR_SECONDARY;
            uint16_t textColor = TFT_BLACK;
            
            const char* keyLabel = keys[row][col];
            if (strcmp(keyLabel, "DEL") == 0 || strcmp(keyLabel, "CLR") == 0) {
                keyColor = COLOR_WARNING;
            } else if (strcmp(keyLabel, "OK") == 0) {
                keyColor = COLOR_SUCCESS;
            } else if (strcmp(keyLabel, "SHIFT") == 0) {
                keyColor = isUpperCaseKeyboard ? COLOR_PRIMARY : COLOR_ACCENT;
            } else if (strcmp(keyLabel, "SPC") == 0) {
                keyColor = COLOR_PRIMARY;
            }
            
            // Draw key background
            tft.fillRect(x, y, KEY_WIDTH, KEY_HEIGHT, keyColor);
            tft.drawRect(x, y, KEY_WIDTH, KEY_HEIGHT, COLOR_TEXT);
            
            // Draw key label - center the text
            tft.setTextColor(textColor);
            int textWidth = strlen(keyLabel) * 6; // Approximate width
            int textX = x + (KEY_WIDTH - textWidth) / 2;
            int textY = y + (KEY_HEIGHT - 8) / 2;
            tft.setCursor(textX, textY);
            
            // Special display for space key
            if (strcmp(keyLabel, "SPC") == 0) {
                tft.print("SPACE");
            } else {
                tft.print(keyLabel);
            }
        }
    }
}

void handleKeyPress(int row, int col)
{
    // Get the appropriate keyboard layout
    const char* (*keys)[10] = isUpperCaseKeyboard ? KEYBOARD_UPPER : KEYBOARD_LOWER;
    const char* keyLabel = keys[row][col];

    // Provide visual feedback for key press
    int x = col * (KEY_WIDTH + KEY_SPACING) + KEYBOARD_X_OFFSET;
    int y = row * (KEY_HEIGHT + KEY_SPACING) + KEYBOARD_Y_OFFSET;
    
    // Flash the key white briefly for feedback
    tft.fillRect(x, y, KEY_WIDTH, KEY_HEIGHT, TFT_WHITE);
    tft.drawRect(x, y, KEY_WIDTH, KEY_HEIGHT, COLOR_TEXT);
    tft.setTextColor(TFT_BLACK);
    int textWidth = strlen(keyLabel) * 6;
    int textX = x + (KEY_WIDTH - textWidth) / 2;
    int textY = y + (KEY_HEIGHT - 8) / 2;
    tft.setCursor(textX, textY);
    tft.setTextSize(1);
    if (strcmp(keyLabel, "SPC") == 0) {
        tft.print("SPACE");
    } else {
        tft.print(keyLabel);
    }
    delay(100); // Brief visual feedback

    // Handle key functionality
    if (strcmp(keyLabel, "DEL") == 0)
    {
        if (inputText.length() > 0)
        {
            inputText.remove(inputText.length() - 1);
        }
    }
    else if (strcmp(keyLabel, "SPC") == 0)
    {
        if (inputText.length() < 30) { // Limit input length
            inputText += " ";
        }
    }
    else if (strcmp(keyLabel, "CLR") == 0)
    {
        inputText = "";
    }
    else if (strcmp(keyLabel, "OK") == 0)
    {
        if (isEnteringSSID)
        {
            if (inputText.length() > 0) {
                wifiSSID = inputText;
                inputText = "";
                isEnteringSSID = false;
                drawKeyboard(isUpperCaseKeyboard);
                return; // Early return to avoid input update
            }
        }
        else
        {
            if (inputText.length() > 0) {
                wifiPassword = inputText;
                
                // Show connection attempt message
                tft.fillScreen(COLOR_BACKGROUND);
                tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
                tft.setTextSize(2);
                tft.setCursor(50, 100);
                tft.println("Connecting...");
                tft.setCursor(30, 130);
                tft.println("Please wait");
                
                saveWiFiSettings();
                WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
                unsigned long startAttemptTime = millis();

                // Try to connect for 10 seconds with progress indicator
                int dots = 0;
                while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000)
                {
                    delay(500);
                    tft.setCursor(30 + (dots * 12), 160);
                    tft.print(".");
                    dots = (dots + 1) % 20;
                    Serial.println("Connecting to WiFi...");
                }

                if (WiFi.status() == WL_CONNECTED)
                {
                    tft.fillScreen(COLOR_BACKGROUND);
                    tft.setCursor(50, 100);
                    tft.setTextColor(COLOR_SUCCESS, COLOR_BACKGROUND);
                    tft.println("Connected!");
                    tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
                    tft.setCursor(30, 130);
                    tft.println("Restarting...");
                    Serial.println("Connected to WiFi");
                    Serial.print("IP Address: ");
                    Serial.println(WiFi.localIP());
                    delay(2000);
                    ESP.restart();
                }
                else
                {
                    tft.fillScreen(COLOR_BACKGROUND);
                    tft.setCursor(30, 100);
                    tft.setTextColor(COLOR_WARNING, COLOR_BACKGROUND);
                    tft.println("Failed to connect");
                    tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
                    tft.setCursor(30, 130);
                    tft.println("Touch to retry");
                    Serial.println("Failed to connect to WiFi");
                    delay(3000);
                    // Reset and return to keyboard
                    inputText = "";
                    isEnteringSSID = true;
                    drawKeyboard(isUpperCaseKeyboard);
                    return;
                }
            }
        }
    }
    else if (strcmp(keyLabel, "SHIFT") == 0)
    {
        isUpperCaseKeyboard = !isUpperCaseKeyboard;
        drawKeyboard(isUpperCaseKeyboard);
        return; // Early return to avoid input update
    }
    else
    {
        // Add regular character if within length limit
        if (inputText.length() < 30) {
            inputText += keyLabel;
        }
    }

    // Update input display
    tft.fillRect(6, 36, 308, 28, COLOR_BACKGROUND);
    tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    tft.setTextSize(2);
    tft.setCursor(10, 42);
    tft.println(inputText);
    
    // Redraw the keyboard to restore normal key colors
    drawKeyboard(isUpperCaseKeyboard);
}

void drawButtons()
{
    // Draw the "+" button
    tft.fillRect(270, 200, 40, 40, COLOR_SUCCESS);
    tft.setCursor(285, 215);
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(2);
    tft.print("+");

    // Move the "-" button to the far bottom left corner
    tft.fillRect(0, 200, 40, 40, COLOR_WARNING);
    tft.setCursor(15, 215);
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(2);
    tft.print("-");

    // Draw the WiFi settings button between minus and mode buttons
    tft.fillRect(47, 200, 68, 40, COLOR_SECONDARY);
    tft.setCursor(50, 208);
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(2);
    tft.print("WiFi");
    tft.setCursor(55, 220);
    tft.print("Setup");

    // Draw the thermostat mode button
    tft.fillRect(125, 200, 60, 40, COLOR_PRIMARY);
    tft.setCursor(130, 208);
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(1);
    tft.print("Mode:");
    tft.setCursor(133, 220);
    tft.setTextSize(2);
    tft.print(thermostatMode);

    // Draw the fan mode button - make it wider to fit "cycle"
    tft.fillRect(195, 200, 65, 40, COLOR_ACCENT);
    
    tft.setCursor(205, 208);
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(1);
    tft.print("Fan:");
    
    // Adjust text position based on fan mode to center it
    int fanTextX = 210;
    if (fanMode == "on") {
        fanTextX = 215;
    } else if (fanMode == "auto") {
        fanTextX = 205;
    } else if (fanMode == "cycle") {
        fanTextX = 200;
    }
    
    tft.setCursor(fanTextX, 220);
    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(2);
    tft.print(fanMode);


}

void handleButtonPress(uint16_t x, uint16_t y)
{
    // Provide immediate audio feedback for touch
    buzzerBeep(50);
    
    unsigned long startTime = millis();
    
    // Add touch debounce mechanism to prevent accidental double touches
    static unsigned long lastButtonPressTime = 0;
    unsigned long currentTime = millis();
    
    // If less than 300ms has passed since the last button press, ignore this touch
    // This prevents rapid mode switching that can cause relay logic delays
    if (currentTime - lastButtonPressTime < 300) {
        return;
    }
    
    lastButtonPressTime = currentTime;
    
    // WiFi setup button - should work even in WiFi setup mode to allow cancellation
    if (x > 45 && x < 125 && y > 195 && y < 245) // WiFi button with slightly increased touch area
    {
        if (inWiFiSetupMode) {
            // Cancel WiFi setup and return to main screen
            inWiFiSetupMode = false;
            tft.fillScreen(COLOR_BACKGROUND);
            updateDisplay(currentTemp, currentHumidity);
            drawButtons();
            return;
        }
        
        // Handle WiFi setup button press - enter WiFi setup mode
        inWiFiSetupMode = true;
        tft.fillScreen(COLOR_BACKGROUND);
        tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
        tft.setTextSize(2);
        tft.setCursor(10, 10);
        tft.println("WiFi Setup");
        
        // Reset entering state to SSID first
        inputText = "";
        isEnteringSSID = true;
        
        // Draw keyboard for WiFi setup
        drawKeyboard(isUpperCaseKeyboard);
        return;
    }
    
    // If in WiFi setup mode, don't process other buttons
    if (inWiFiSetupMode) {
        return;
    }
    
    // Increase the touchable area slightly to make buttons easier to hit
    
    // "+" button
    if (x > 265 && x < 315 && y > 195 && y < 245)
    {
        if (thermostatMode == "heat")
        {
            setTempHeat += 0.5;
            if (setTempHeat > 95) setTempHeat = 95;
            if (setTempHeat < 50) setTempHeat = 50;
            if (thermostatMode == "auto" && setTempCool - setTempHeat < tempDifferential)
            {
                setTempCool = setTempHeat + tempDifferential;
                if (!handlingMQTTMessage) mqttClient.publish("thermostat/setTempCool", String(setTempCool).c_str(), true);
            }
            if (!handlingMQTTMessage) mqttClient.publish("thermostat/setTempHeat", String(setTempHeat).c_str(), true);
        }
        else if (thermostatMode == "cool")
        {
            setTempCool += 0.5;
            if (setTempCool > 95) setTempCool = 95;
            if (setTempCool < 50) setTempCool = 50;
            if (thermostatMode == "auto" && setTempCool - setTempHeat < tempDifferential)
            {
                setTempHeat = setTempCool - tempDifferential;
                if (!handlingMQTTMessage) mqttClient.publish("thermostat/setTempHeat", String(setTempHeat).c_str(), true);
            }
            if (!handlingMQTTMessage) mqttClient.publish("thermostat/setTempCool", String(setTempCool).c_str(), true);
        }
        else if (thermostatMode == "auto")
        {
            setTempAuto += 0.5;
            if (setTempAuto > 95) setTempAuto = 95;
            if (setTempAuto < 50) setTempAuto = 50;
            if (!handlingMQTTMessage) mqttClient.publish("thermostat/setTempAuto", String(setTempAuto).c_str(), true);
        }
        saveSettings();
        sendMQTTData();
        // Update display immediately for better responsiveness
        updateDisplay(currentTemp, currentHumidity);
    }
    else if (x > 0 && x < 45 && y > 195 && y < 245) // "-" button with slightly increased touch area
    {
        if (thermostatMode == "heat")
        {
            setTempHeat -= 0.5;
            if (setTempHeat > 95) setTempHeat = 95;
            if (setTempHeat < 50) setTempHeat = 50;
            if (thermostatMode == "auto" && setTempCool - setTempHeat < tempDifferential)
            {
                setTempCool = setTempHeat + tempDifferential;
                if (!handlingMQTTMessage) mqttClient.publish("thermostat/setTempCool", String(setTempCool).c_str(), true);
            }
            if (!handlingMQTTMessage) mqttClient.publish("thermostat/setTempHeat", String(setTempHeat).c_str(), true);
        }
        else if (thermostatMode == "cool")
        {
            setTempCool -= 0.5;
            if (setTempCool > 95) setTempCool = 95;
            if (setTempCool < 50) setTempCool = 50;
            if (thermostatMode == "auto" && setTempCool - setTempHeat < tempDifferential)
            {
                setTempHeat = setTempCool - tempDifferential;
                if (!handlingMQTTMessage) mqttClient.publish("thermostat/setTempHeat", String(setTempHeat).c_str(), true);
            }
            if (!handlingMQTTMessage) mqttClient.publish("thermostat/setTempCool", String(setTempCool).c_str(), true);
        }
        else if (thermostatMode == "auto")
        {
            setTempAuto -= 0.5;
            if (setTempAuto > 95) setTempAuto = 95;
            if (setTempAuto < 50) setTempAuto = 50;
            if (!handlingMQTTMessage) mqttClient.publish("thermostat/setTempAuto", String(setTempAuto).c_str(), true);
        }
        unsigned long beforeSave2 = millis();
        Serial.printf("[DEBUG] Before saveSettings (- button)\n");
        saveSettings();
        unsigned long afterSave2 = millis();
        Serial.printf("[DEBUG] After saveSettings (took %lu ms)\n", afterSave2 - beforeSave2);
        
        sendMQTTData();
        unsigned long afterMQTT2 = millis();
        Serial.printf("[DEBUG] After sendMQTTData (took %lu ms)\n", afterMQTT2 - afterSave2);
        
        // Update display immediately for better responsiveness
        unsigned long beforeDisplay2 = millis();
        Serial.printf("[DEBUG] Before updateDisplay (- button)\n");
        updateDisplay(currentTemp, currentHumidity);
        unsigned long afterDisplay2 = millis();
        Serial.printf("[DEBUG] After updateDisplay (took %lu ms)\n", afterDisplay2 - beforeDisplay2);
        Serial.printf("[DEBUG] Total - button time: %lu ms\n", afterDisplay2 - startTime);
    }
    else if (x > 125 && x < 195 && y > 195 && y < 245) // Mode button with slightly increased touch area
    {
        String oldMode = thermostatMode;
        // Change thermostat mode
        if (thermostatMode == "auto")
            thermostatMode = "heat";
        else if (thermostatMode == "heat")
            thermostatMode = "cool";
        else if (thermostatMode == "cool")
            thermostatMode = "off";
        else
            thermostatMode = "auto";
        
        Serial.printf("[DEBUG] Mode switched: %s -> %s\n", oldMode.c_str(), thermostatMode.c_str());

        saveSettings();
        sendMQTTData();
        // Immediately update relays to reflect mode change
        controlRelays(currentTemp);
        // Update display after relays for accurate indicators
        updateDisplay(currentTemp, currentHumidity);
        setDisplayUpdateFlag(); // Option C: Request display update
    }
    else if (x > 195 && x < 265 && y > 195 && y < 245) // Fan button with slightly increased touch area
    {
        // Change fan mode
        if (fanMode == "auto")
            fanMode = "on";
        else if (fanMode == "on")
            fanMode = "cycle";
        else
            fanMode = "auto";

        saveSettings();
        sendMQTTData();
        // Immediately update relays to reflect fan mode change
        controlRelays(currentTemp);
        // Update display after relays for accurate indicators
        updateDisplay(currentTemp, currentHumidity);
    }
}

void setupMQTT()
{
    mqttClient.setServer(mqttServer.c_str(), mqttPort);
    mqttClient.setBufferSize(1024); // Ensure buffer size is sufficient for large payloads
    mqttClient.setCallback(mqttCallback);
}

void reconnectMQTT()
{
    // Non-blocking approach - only try once per function call
    if (!mqttClient.connected())
    {
        Serial.print("Attempting MQTT connection to server: ");
        Serial.print(mqttServer);
        Serial.print(" port: ");
        Serial.print(mqttPort);
        Serial.print(" username: ");
        Serial.print(mqttUsername);
        
        if (mqttClient.connect(hostname.c_str(), mqttUsername.c_str(), mqttPassword.c_str())) {
            Serial.println(" - Connected successfully");

            // Subscribe to necessary topics
            String tempSetTopic = hostname + "/target_temperature/set";
            String modeSetTopic = hostname + "/mode/set";
            String fanModeSetTopic = hostname + "/fan_mode/set";
            mqttClient.subscribe(tempSetTopic.c_str());
            mqttClient.subscribe(modeSetTopic.c_str());
            mqttClient.subscribe(fanModeSetTopic.c_str());

            // Publish Home Assistant discovery messages
            publishHomeAssistantDiscovery();
        }
        else
        {
            int mqttState = mqttClient.state();
            Serial.print(" - Connection failed, rc=");
            Serial.print(mqttState);
            Serial.print(" (");
            
            // Provide human-readable error messages
            switch(mqttState) {
                case -4: Serial.print("MQTT_CONNECTION_TIMEOUT"); break;
                case -3: Serial.print("MQTT_CONNECTION_LOST"); break;
                case -2: Serial.print("MQTT_CONNECT_FAILED"); break;
                case -1: Serial.print("MQTT_DISCONNECTED"); break;
                case 1: Serial.print("MQTT_CONNECT_BAD_PROTOCOL"); break;
                case 2: Serial.print("MQTT_CONNECT_BAD_CLIENT_ID"); break;
                case 3: Serial.print("MQTT_CONNECT_UNAVAILABLE"); break;
                case 4: Serial.print("MQTT_CONNECT_BAD_CREDENTIALS"); break;
                case 5: Serial.print("MQTT_CONNECT_UNAUTHORIZED"); break;
                default: Serial.print("UNKNOWN ERROR"); break;
            }
            
            Serial.println(")");
            Serial.print("Server: ");
            Serial.print(mqttServer);
            Serial.print(", Port: ");
            Serial.println(mqttPort);
        }
    }
}

void publishHomeAssistantDiscovery()
{
    if (mqttEnabled)
    {
        char buffer[1024];
        StaticJsonDocument<1024> doc;

        // Publish discovery message for the thermostat device
        String configTopic = "homeassistant/climate/" + hostname + "/config";
        doc["name"] = "";
        doc["unique_id"] = hostname;
        doc["current_temperature_topic"] = hostname + "/current_temperature";
        doc["current_humidity_topic"] = hostname + "/current_humidity";
        doc["temperature_command_topic"] = hostname + "/target_temperature/set";
        doc["temperature_state_topic"] = hostname + "/target_temperature";
        doc["mode_command_topic"] = hostname + "/mode/set";
        doc["mode_state_topic"] = hostname + "/mode";
        doc["fan_mode_command_topic"] = hostname + "/fan_mode/set";
        doc["fan_mode_state_topic"] = hostname + "/fan_mode";
        doc["availability_topic"] = hostname + "/availability";
        doc["min_temp"] = 50; // Minimum temperature in Fahrenheit
        doc["max_temp"] = 90; // Maximum temperature in Fahrenheit
        doc["temp_step"] = 0.5; // Temperature step

        JsonArray modes = doc.createNestedArray("modes");
        modes.add("off");
        modes.add("heat");
        modes.add("cool");
        modes.add("auto");

        JsonArray fanModes = doc.createNestedArray("fan_modes");
        fanModes.add("auto");
        fanModes.add("on");
        fanModes.add("cycle");

        JsonObject device = doc.createNestedObject("device");
        device["identifiers"] = hostname;
        device["name"] = hostname;
        device["manufacturer"] = "TDC";
        device["model"] = "Simple Thermostat Alt Firmware";
        device["sw_version"] = sw_version;

        serializeJson(doc, buffer);
        mqttClient.publish(configTopic.c_str(), buffer, true);

        // Publish availability message
        String availabilityTopic = hostname + "/availability";
        mqttClient.publish(availabilityTopic.c_str(), "online", true);

        // Debug log for payload
        Serial.println("Published Home Assistant discovery payload:");
        Serial.println(buffer);

        // Publish motion sensor discovery if LD2410 is connected
        if (ld2410Connected) {
            StaticJsonDocument<512> motionDoc;
            String motionConfigTopic = "homeassistant/binary_sensor/" + hostname + "_motion/config";
            
            motionDoc["name"] = hostname + " Motion";
            motionDoc["device_class"] = "motion";
            motionDoc["state_topic"] = hostname + "/motion_detected";
            motionDoc["payload_on"] = "true";
            motionDoc["payload_off"] = "false";
            motionDoc["unique_id"] = hostname + "_motion";
            
            // Device information
            JsonObject device = motionDoc.createNestedObject("device");
            device["identifiers"][0] = hostname;
            device["name"] = hostname;
            device["model"] = "ESP32-S3 with LD2410";
            device["manufacturer"] = "Custom";
            
            char motionBuffer[512];
            serializeJson(motionDoc, motionBuffer);
            mqttClient.publish(motionConfigTopic.c_str(), motionBuffer, true);
            
            Serial.println("Published LD2410 motion sensor discovery to Home Assistant");
        }
        
        // Publish barometric pressure sensor discovery if BME280 is active
        if (activeSensor == SENSOR_BME280) {
            StaticJsonDocument<512> pressureDoc;
            String pressureConfigTopic = "homeassistant/sensor/" + hostname + "_pressure/config";
            
            pressureDoc["name"] = hostname + " Barometric Pressure";
            pressureDoc["device_class"] = "pressure";
            pressureDoc["state_topic"] = hostname + "/barometric_pressure";
            pressureDoc["unit_of_measurement"] = "hPa";
            pressureDoc["unique_id"] = hostname + "_pressure";
            pressureDoc["state_class"] = "measurement";
            
            // Link to same device as main thermostat
            JsonObject device = pressureDoc.createNestedObject("device");
            device["identifiers"][0] = hostname;
            device["name"] = hostname;
            device["model"] = "Simple Thermostat Alt Firmware";
            device["manufacturer"] = "TDC";
            device["sw_version"] = sw_version;
            
            char pressureBuffer[512];
            serializeJson(pressureDoc, pressureBuffer);
            mqttClient.publish(pressureConfigTopic.c_str(), pressureBuffer, true);
            
            Serial.println("Published BME280 pressure sensor discovery to Home Assistant");
        }
    }
    else
    {
        // Remove all entities by publishing empty payloads
        String configTopic = "homeassistant/climate/" + hostname + "/config";
        String availabilityTopic = hostname + "/availability";
        mqttClient.publish(configTopic.c_str(), "");
        mqttClient.publish(availabilityTopic.c_str(), "offline", true);
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
    String message;
    for (unsigned int i = 0; i < length; i++)
    {
        message += (char)payload[i];
    }

    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    Serial.println(message);

    // Set flag to indicate we're handling an MQTT message to prevent publish loops
    handlingMQTTMessage = true;
    bool settingsNeedSaving = false;

    // Declare all topic strings at the beginning
    String modeSetTopic = hostname + "/mode/set";
    String fanModeSetTopic = hostname + "/fan_mode/set";
    String tempSetTopic = hostname + "/target_temperature/set";

    if (String(topic) == modeSetTopic)
    {
        if (message != thermostatMode)
        {
            thermostatMode = message;
            Serial.print("Updated thermostat mode to: ");
            Serial.println(thermostatMode);
            settingsNeedSaving = true;
            controlRelays(currentTemp); // Apply changes to relays
            setDisplayUpdateFlag(); // Option C: Request display update
        }
    }
    else if (String(topic) == fanModeSetTopic)
    {
        if (message != fanMode)
        {
            fanMode = message;
            Serial.print("Updated fan mode to: ");
            Serial.println(fanMode);
            settingsNeedSaving = true;
            controlRelays(currentTemp); // Apply changes to relays
        }
    }
    else if (String(topic) == tempSetTopic)
    {
        float newTargetTemp = message.toFloat();
        if (thermostatMode == "heat" && newTargetTemp != setTempHeat)
        {
            setTempHeat = newTargetTemp;
            Serial.print("Updated heating target temperature to: ");
            Serial.println(setTempHeat);
            settingsNeedSaving = true;
        }
        else if (thermostatMode == "cool" && newTargetTemp != setTempCool)
        {
            setTempCool = newTargetTemp;
            Serial.print("Updated cooling target temperature to: ");
            Serial.println(setTempCool);
            settingsNeedSaving = true;
        }
        else if (thermostatMode == "auto" && newTargetTemp != setTempAuto)
        {
            setTempAuto = newTargetTemp;
            Serial.print("Updated auto target temperature to: ");
            Serial.println(setTempAuto);
            settingsNeedSaving = true;
        }
        controlRelays(currentTemp); // Apply changes to relays
    }

    // Save settings to flash if they were changed
    if (settingsNeedSaving) {
        Serial.println("Saving settings changed via MQTT");
        saveSettings();
        // Update display immediately when settings change via MQTT
        updateDisplay(currentTemp, currentHumidity);
        
        // Set flag for immediate MQTT feedback to Home Assistant
        mqttFeedbackNeeded = true;
    }

    // Clear the handling flag
    handlingMQTTMessage = false;
}

void sendMQTTData()
{
    static float lastTemp = 0.0;
    static float lastHumidity = 0.0;
    static float lastSetTempHeat = 0.0;
    static float lastSetTempCool = 0.0;
    static float lastSetTempAuto = 0.0; // Track last auto temp
    static String lastThermostatMode = "";
    static String lastFanMode = "";

    if (mqttClient.connected())
    {
        // Publish current temperature
        if (!isnan(currentTemp) && currentTemp != lastTemp)
        {
            String currentTempTopic = hostname + "/current_temperature";
            mqttClient.publish(currentTempTopic.c_str(), String(currentTemp, 1).c_str(), true);
            lastTemp = currentTemp;
        }

        // Publish current humidity
        if (!isnan(currentHumidity) && currentHumidity != lastHumidity)
        {
            String currentHumidityTopic = hostname + "/current_humidity";
            mqttClient.publish(currentHumidityTopic.c_str(), String(currentHumidity, 1).c_str(), true);
            lastHumidity = currentHumidity;
        }
        
        // Publish barometric pressure if BME280 sensor is active
        if (activeSensor == SENSOR_BME280 && !isnan(currentPressure))
        {
            static float lastPressure = 0.0;
            if (currentPressure != lastPressure)
            {
                String pressureTopic = hostname + "/barometric_pressure";
                mqttClient.publish(pressureTopic.c_str(), String(currentPressure, 1).c_str(), true);
                lastPressure = currentPressure;
            }
        }

        // Publish target temperature (set temperature for heating, cooling, or auto)
        if (thermostatMode == "heat" && setTempHeat != lastSetTempHeat)
        {
            String targetTempTopic = hostname + "/target_temperature";
            mqttClient.publish(targetTempTopic.c_str(), String(setTempHeat, 1).c_str(), true);
            lastSetTempHeat = setTempHeat;
        }
        else if (thermostatMode == "cool" && setTempCool != lastSetTempCool)
        {
            String targetTempTopic = hostname + "/target_temperature";
            mqttClient.publish(targetTempTopic.c_str(), String(setTempCool, 1).c_str(), true);
            lastSetTempCool = setTempCool;
        }
        else if (thermostatMode == "auto" && setTempAuto != lastSetTempAuto)
        {
            String targetTempTopic = hostname + "/target_temperature";
            mqttClient.publish(targetTempTopic.c_str(), String(setTempAuto, 1).c_str(), true);
            lastSetTempAuto = setTempAuto;
        }

        // Publish thermostat mode
        if (thermostatMode != lastThermostatMode)
        {
            String modeTopic = hostname + "/mode";
            mqttClient.publish(modeTopic.c_str(), thermostatMode.c_str(), true);
            lastThermostatMode = thermostatMode;
        }

        // Publish fan mode
        if (fanMode != lastFanMode)
        {
            String fanModeTopic = hostname + "/fan_mode";
            mqttClient.publish(fanModeTopic.c_str(), fanMode.c_str(), true);
            lastFanMode = fanMode;
        }

        // Publish hydronic temperature if hydronic heating is enabled
        if (hydronicHeatingEnabled)
        {
            String hydronicTempTopic = hostname + "/hydronic_temperature";
            mqttClient.publish(hydronicTempTopic.c_str(), String(hydronicTemp, 1).c_str(), true);
        }

        // Monitor hydronic boiler water temperature and send alerts
        Serial.printf("[DEBUG] Hydronic Alert Check: enabled=%s, temp=%.1f, tempValid=%s\n",
                     hydronicHeatingEnabled ? "YES" : "NO", 
                     hydronicTemp,
                     !isnan(hydronicTemp) ? "YES" : "NO");
                     
        if (hydronicHeatingEnabled && !isnan(hydronicTemp))
        {
            Serial.printf("[DEBUG] Hydronic Logic: temp=%.1f < threshold=%.1f? %s, alertSent=%s\n",
                         hydronicTemp, hydronicTempLow,
                         (hydronicTemp < hydronicTempLow) ? "YES" : "NO",
                         hydronicLowTempAlertSent ? "YES" : "NO");
            
            // Check if temperature dropped below setpoint and we haven't sent alert yet
            if (hydronicTemp < hydronicTempLow && !hydronicLowTempAlertSent)
            {
                // Send alert to Home Assistant
                String alertTopic = hostname + "/hydronic_alert";
                String alertMessage = "ALERT: Boiler water temperature (" + String(hydronicTemp, 1) + "°F) is below setpoint (" + String(hydronicTempLow, 1) + "°F)";
                mqttClient.publish(alertTopic.c_str(), alertMessage.c_str(), false);
                
                // Also send to Home Assistant notification service
                String haTopic = "homeassistant/notify/thermostat_alerts";
                String haMessage = "{\"title\":\"Boiler Alert\",\"message\":\"" + alertMessage + "\"}";
                mqttClient.publish(haTopic.c_str(), haMessage.c_str(), false);
                
                // Set flag to prevent duplicate alerts
                hydronicLowTempAlertSent = true;
                preferences.putBool("hydAlertSent", hydronicLowTempAlertSent);
                Serial.println("MQTT: Hydronic low temperature alert sent");
            }
            // Reset alert flag only when temperature recovers above HIGH threshold (hysteresis)
            else if (hydronicTemp >= hydronicTempHigh && hydronicLowTempAlertSent)
            {
                hydronicLowTempAlertSent = false;
                preferences.putBool("hydAlertSent", hydronicLowTempAlertSent);
                Serial.printf("MQTT: Hydronic temperature recovered to %.1f°F (above %.1f°F) - alert reset\n", 
                             hydronicTemp, hydronicTempHigh);
            }
        }

        // Publish motion sensor status if connected
        if (ld2410Connected) {
            static bool lastMotionDetected = false;
            if (motionDetected != lastMotionDetected) {
                String motionTopic = hostname + "/motion_detected";
                mqttClient.publish(motionTopic.c_str(), motionDetected ? "true" : "false", false);
                lastMotionDetected = motionDetected;
            }
        }

        // Publish schedule status
        String scheduleEnabledTopic = hostname + "/schedule_enabled";
        mqttClient.publish(scheduleEnabledTopic.c_str(), scheduleEnabled ? "on" : "off", true);
        
        String activePeriodTopic = hostname + "/active_period";
        mqttClient.publish(activePeriodTopic.c_str(), activePeriod.c_str(), false);
        
        if (scheduleOverride) {
            String overrideTopic = hostname + "/schedule_override";
            mqttClient.publish(overrideTopic.c_str(), "active", false);
        }

        // Publish availability
        String availabilityTopic = hostname + "/availability";
        mqttClient.publish(availabilityTopic.c_str(), "online", true);
    }
}

void controlRelays(float currentTemp)
{
    // Take mutex to prevent concurrent access from multiple cores
    if (xSemaphoreTake(controlRelaysMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        Serial.println("[WARNING] controlRelays: Failed to acquire mutex, skipping this call");
        return;
    }
    
    // Debug entry
    Serial.printf("[DEBUG] controlRelays ENTRY: mode=%s, temp=%.1f, heatingOn=%d, coolingOn=%d\n", 
                 thermostatMode.c_str(), currentTemp, heatingOn, coolingOn);
    
    // Track previous states to only print debug info on changes
    static bool prevHeatingOn = false;
    static bool prevCoolingOn = false;
    static bool prevFanOn = false;
    static String prevThermostatMode = "";
    static float prevTemp = 0.0;
    
    // Check if temperature reading is valid
    if (isnan(currentTemp)) {
        Serial.println("WARNING: Invalid temperature reading, skipping relay control");
        return;
    }
    
    // Store current states before any changes
    bool currentHeatingOn = heatingOn;
    bool currentCoolingOn = coolingOn;
    bool currentFanOn = fanOn;

    if (thermostatMode == "off")
    {
        Serial.println("[DEBUG] In OFF mode - turning off heating and cooling relays");
        // Turn off heating and cooling relays, but don't turn off fan
        // This allows the fan to operate in "on" or "cycle" mode even when thermostat is off
        digitalWrite(HEAT_RELAY_1_PIN, LOW);
        digitalWrite(HEAT_RELAY_2_PIN, LOW);
        digitalWrite(COOL_RELAY_1_PIN, LOW);
        digitalWrite(COOL_RELAY_2_PIN, LOW);
        heatingOn = false;
        coolingOn = false;
        stage1Active = false;
        stage2Active = false;
        
        // Handle fan separately based on fanMode
        if (fanMode == "on") {
            if (!fanOn) {
                digitalWrite(FAN_RELAY_PIN, HIGH);
                fanOn = true;
                Serial.println("Fan on while thermostat is off");
            }
        }
        else if (fanMode == "auto") {
            digitalWrite(FAN_RELAY_PIN, LOW);
            fanOn = false;
        }
        // Note: "cycle" fan mode is handled by controlFanSchedule()
        updateStatusLEDs(); // Update LED status
        
        xSemaphoreGive(controlRelaysMutex);
        return;
    }

    // Rest of the thermostat logic for heat, cool, and auto modes
    if (thermostatMode == "heat")
    {
        Serial.printf("[DEBUG] In HEAT mode: temp=%.1f, setpoint=%.1f, swing=%.1f\n", 
                     currentTemp, setTempHeat, tempSwing);
        
        // Turn off cooling relays when entering heat mode
        if (coolingOn) {
            Serial.println("[DEBUG] Turning off cooling relays in heat mode");
            digitalWrite(COOL_RELAY_1_PIN, LOW);
            digitalWrite(COOL_RELAY_2_PIN, LOW);
            coolingOn = false;
            // Reset staging flags to allow heating to start fresh
            stage1Active = false;
            stage2Active = false;
        }
        
        // Only activate heating if below setpoint - swing
        Serial.printf("[DEBUG] Heat check: %.1f < %.1f? %s\n", 
                     currentTemp, (setTempHeat - tempSwing), 
                     (currentTemp < (setTempHeat - tempSwing)) ? "YES" : "NO");
        if (currentTemp < (setTempHeat - tempSwing))
        {
            // Only call activateHeating if not already heating
            if (!heatingOn) {
                Serial.printf("Activating heating: current %.1f < setpoint-swing %.1f\n", 
                             currentTemp, (setTempHeat - tempSwing));
                activateHeating();
            }
        }
        // Only turn off if above setpoint (hysteresis)
        else if (currentTemp >= setTempHeat)
        {
            if (heatingOn || coolingOn || fanOn) {
                Serial.printf("Deactivating heating: current %.1f >= setpoint %.1f\n", 
                             currentTemp, setTempHeat);
            }
            turnOffAllRelays();
        }
        // Otherwise maintain current state (hysteresis band)
    }
    else if (thermostatMode == "cool")
    {
        Serial.printf("[DEBUG] In COOL mode: temp=%.1f, setpoint=%.1f, swing=%.1f\n", 
                     currentTemp, setTempCool, tempSwing);
        
        // Turn off heating relays when entering cool mode
        if (heatingOn) {
            Serial.println("[DEBUG] Turning off heating relays in cool mode");
            digitalWrite(HEAT_RELAY_1_PIN, LOW);
            digitalWrite(HEAT_RELAY_2_PIN, LOW);
            heatingOn = false;
            // Reset staging flags to allow cooling to start fresh
            stage1Active = false;
            stage2Active = false;
        }
        
        // Only activate cooling if above setpoint + swing
        Serial.printf("[DEBUG] Cool check: %.1f > %.1f? %s\n", 
                     currentTemp, (setTempCool + tempSwing), 
                     (currentTemp > (setTempCool + tempSwing)) ? "YES" : "NO");
        if (currentTemp > (setTempCool + tempSwing))
        {
            // Only call activateCooling if not already cooling
            if (!coolingOn) {
                Serial.printf("Activating cooling: current %.1f > setpoint+swing %.1f\n", 
                             currentTemp, (setTempCool + tempSwing));
                activateCooling();
            }
        }
        // Only turn off if below setpoint (hysteresis)
        else if (currentTemp < setTempCool)
        {
            if (heatingOn || coolingOn || fanOn) {
                Serial.printf("Deactivating cooling: current %.1f < setpoint %.1f\n", 
                             currentTemp, setTempCool);
            }
            turnOffAllRelays();
        }
        // Otherwise maintain current state (hysteresis band between setpoint and setpoint+swing)
    }
    else if (thermostatMode == "auto")
    {
        Serial.printf("[DEBUG] In AUTO mode: temp=%.1f, setpoint=%.1f, autoSwing=%.1f\n", 
                     currentTemp, setTempAuto, autoTempSwing);
        
        if (currentTemp < (setTempAuto - autoTempSwing))
        {
            Serial.printf("[DEBUG] Auto heating check: %.1f < %.1f? YES\n", 
                         currentTemp, (setTempAuto - autoTempSwing));
            if (!heatingOn) {
                Serial.printf("Auto mode activating heating: current %.1f < auto_setpoint-swing %.1f\n", 
                             currentTemp, (setTempAuto - autoTempSwing));
            }
            activateHeating();
        }
        else if (currentTemp > (setTempAuto + autoTempSwing))
        {
            Serial.printf("[DEBUG] Auto cooling check: %.1f > %.1f? YES\n", 
                         currentTemp, (setTempAuto + autoTempSwing));
            if (!coolingOn) {
                Serial.printf("Auto mode activating cooling: current %.1f > auto_setpoint+swing %.1f\n", 
                             currentTemp, (setTempAuto + autoTempSwing));
            }
            activateCooling();
        }
        else
        {
            Serial.printf("[DEBUG] Auto deadband check: %.1f between %.1f and %.1f\n", 
                         currentTemp, (setTempAuto - autoTempSwing), (setTempAuto + autoTempSwing));
            if (heatingOn || coolingOn) {
                Serial.printf("Auto mode temperature in deadband, turning off: %.1f is between %.1f and %.1f\n", 
                             currentTemp, (setTempAuto - autoTempSwing), (setTempAuto + autoTempSwing));
            }
            turnOffAllRelays();
        }
    }

    // Make sure fan control is applied
    handleFanControl();
    
    // Detect any state or mode changes to trigger display/LED updates
    bool stateChanged = (heatingOn != prevHeatingOn || coolingOn != prevCoolingOn || fanOn != prevFanOn);
    bool modeChanged = (thermostatMode != prevThermostatMode);
    
    // Only print debug info when there are changes
    if (stateChanged || modeChanged || abs(currentTemp - prevTemp) > 0.5) {
        Serial.printf("controlRelays: mode=%s, temp=%.1f, setHeat=%.1f, setCool=%.1f, setAuto=%.1f, swing=%.1f\n", 
                     thermostatMode.c_str(), currentTemp, setTempHeat, setTempCool, setTempAuto, tempSwing);
        Serial.printf("Relay states: heating=%d, cooling=%d, fan=%d\n", heatingOn, coolingOn, fanOn);
        
        // CONSOLIDATED UPDATE: Update LEDs and display when relay state or mode changes
        updateStatusLEDs();
        setDisplayUpdateFlag();
        
        // Update previous states
        prevHeatingOn = heatingOn;
        prevCoolingOn = coolingOn;
        prevFanOn = fanOn;
        prevThermostatMode = thermostatMode;
        prevTemp = currentTemp;
    }
    
    // Debug: Verify actual relay pin states
    bool actualHeat1 = digitalRead(HEAT_RELAY_1_PIN) == HIGH;
    bool actualHeat2 = digitalRead(HEAT_RELAY_2_PIN) == HIGH;
    bool actualCool1 = digitalRead(COOL_RELAY_1_PIN) == HIGH;
    bool actualCool2 = digitalRead(COOL_RELAY_2_PIN) == HIGH;
    bool actualFan = digitalRead(FAN_RELAY_PIN) == HIGH;
    
    Serial.printf("[DEBUG] controlRelays EXIT: RelayPins H1=%d H2=%d C1=%d C2=%d F=%d | Flags heat=%d cool=%d fan=%d stage1=%d stage2=%d\n", 
                 actualHeat1, actualHeat2, actualCool1, actualCool2, actualFan, 
                 heatingOn, coolingOn, fanOn, stage1Active, stage2Active);
    
    xSemaphoreGive(controlRelaysMutex);
}

void turnOffAllRelays()
{
    Serial.println("[DEBUG] turnOffAllRelays() - Turning off heating/cooling relays");
    digitalWrite(HEAT_RELAY_1_PIN, LOW);
    digitalWrite(HEAT_RELAY_2_PIN, LOW);
    digitalWrite(COOL_RELAY_1_PIN, LOW);
    digitalWrite(COOL_RELAY_2_PIN, LOW);
    heatingOn = false;
    coolingOn = false;
    stage1Active = false; // Reset stage 1 active flag
    stage2Active = false; // Reset stage 2 active flag
    
    // Handle fan based on fanMode setting
    if (fanMode == "on") {
        // Keep fan running in "on" mode
        if (!fanOn) {
            digitalWrite(FAN_RELAY_PIN, HIGH);
            fanOn = true;
            Serial.println("[DEBUG] turnOffAllRelays() - Keeping fan ON (fanMode=on)");
        }
    } else if (fanMode == "auto") {
        // Turn off fan in auto mode when heating/cooling stops
        if (fanRelayNeeded) {
            // Only control fan if fanRelayNeeded is true
            digitalWrite(FAN_RELAY_PIN, LOW);
            fanOn = false;
            Serial.println("[DEBUG] turnOffAllRelays() - Turning fan OFF (fanMode=auto)");
        }
    }
    // Note: "cycle" mode is handled by controlFanSchedule(), don't interfere
    
    Serial.printf("[DEBUG] turnOffAllRelays() COMPLETE: heatingOn=%d, coolingOn=%d, fanOn=%d, fanMode=%s\n", 
                 heatingOn, coolingOn, fanOn, fanMode.c_str());
    updateStatusLEDs(); // Update LED status
    setDisplayUpdateFlag(); // Option C: Request display update
}

void activateHeating() {
    Serial.printf("[DEBUG] activateHeating() ENTRY: stage1Active=%d, stage2Active=%d\n", stage1Active, stage2Active);
    
    // Hydronic boiler safety interlock - prevent heating if boiler water is too cold
    if (hydronicHeatingEnabled && !isnan(hydronicTemp)) {
        Serial.printf("[DEBUG] Hydronic Safety Check: temp=%.1f, low=%.1f, high=%.1f, lockout=%d\n", 
                     hydronicTemp, hydronicTempLow, hydronicTempHigh, hydronicLockout);
        
        // Manage lockout state with hysteresis
        // Lockout activates at low threshold, clears at high threshold
        if (hydronicTemp < hydronicTempLow && !hydronicLockout) {
            hydronicLockout = true;
            Serial.printf("[LOCKOUT] Hydronic lockout ACTIVATED - temp %.1f°F below %.1f°F\n", 
                         hydronicTemp, hydronicTempLow);
        } else if (hydronicTemp >= hydronicTempHigh && hydronicLockout) {
            hydronicLockout = false;
            Serial.printf("[LOCKOUT] Hydronic lockout CLEARED - temp %.1f°F reached %.1f°F\n", 
                         hydronicTemp, hydronicTempHigh);
        }
        
        // If in lockout state, prevent heating
        if (hydronicLockout) {
            Serial.printf("[LOCKOUT] Hydronic lockout active - waiting for temp to reach %.1f°F (currently %.1f°F)\n", 
                         hydronicTempHigh, hydronicTemp);
            
            // Turn off heating relays
            digitalWrite(HEAT_RELAY_1_PIN, LOW);
            digitalWrite(HEAT_RELAY_2_PIN, LOW);
            heatingOn = false;
            stage1Active = false;
            stage2Active = false;
            
            // Keep fan running if needed (to circulate existing warm air)
            if (fanMode == "on" || fanMode == "cycle") {
                if (!fanOn) {
                    Serial.println("[LOCKOUT] Keeping fan on for air circulation");
                    digitalWrite(FAN_RELAY_PIN, HIGH);
                    fanOn = true;
                }
            }
            
            updateStatusLEDs();
            setDisplayUpdateFlag();
            return; // Exit - no heating allowed
        }
        
        Serial.printf("[LOCKOUT] Hydronic water temp %.1f°F OK - heating allowed\n", hydronicTemp);
    }

    // Default heating behavior with hybrid staging
    heatingOn = true;
    coolingOn = false;
    
    // Turn off cooling relays when activating heating
    digitalWrite(COOL_RELAY_1_PIN, LOW);
    digitalWrite(COOL_RELAY_2_PIN, LOW);
    
    // Check if stage 1 is not active yet
    if (!stage1Active) {
        Serial.println("[DEBUG] Activating heating stage 1 relay");
        digitalWrite(HEAT_RELAY_1_PIN, HIGH); // Activate stage 1
        stage1Active = true;
        stage1StartTime = millis(); // Record the start time
        stage2Active = false; // Ensure stage 2 is off initially
        Serial.printf("[DEBUG] Stage 1 heating activated - relay pin %d set HIGH\n", HEAT_RELAY_1_PIN);
    } 
    // Check if it's time to activate stage 2 based on hybrid approach
    else if (!stage2Active && 
             ((millis() - stage1StartTime) / 1000 >= stage1MinRuntime) && // Minimum run time condition
             (currentTemp < setTempHeat - tempSwing - stage2TempDelta) && // Temperature delta condition
             stage2HeatingEnabled) { // Check if stage 2 heating is enabled
        digitalWrite(HEAT_RELAY_2_PIN, HIGH); // Activate stage 2
        stage2Active = true;
        Serial.println("Stage 2 heating activated");
    }
    
    // Control fan based on fanRelayNeeded setting
    // BUT: Never override manual "on" mode - user takes priority
    if (fanMode == "on") {
        // User has manually set fan to always on - respect that
        if (!fanOn) {
            digitalWrite(FAN_RELAY_PIN, HIGH);
            fanOn = true;
            Serial.println("Fan activated with heat (manual 'on' mode)");
        }
    } else if (fanRelayNeeded) {
        if (!fanOn) {
            digitalWrite(FAN_RELAY_PIN, HIGH);
            fanOn = true;
            Serial.println("Fan activated with heat");
        }
    } else {
        // HVAC controls its own fan, turn ours off
        if (fanOn) {
            digitalWrite(FAN_RELAY_PIN, LOW);
            fanOn = false;
            Serial.println("Fan turned off during heat - HVAC controls fan");
        }
    }
    updateStatusLEDs(); // Update LED status
    setDisplayUpdateFlag(); // Option C: Request display update
}

void activateCooling()
{
    Serial.printf("[DEBUG] activateCooling() ENTRY: stage1Active=%d, stage2Active=%d\n", stage1Active, stage2Active);
    
    // Default cooling behavior with hybrid staging
    coolingOn = true;
    heatingOn = false;
    
    // Turn off heating relays when activating cooling
    digitalWrite(HEAT_RELAY_1_PIN, LOW);
    digitalWrite(HEAT_RELAY_2_PIN, LOW);
    
    // Check if stage 1 is not active yet
    if (!stage1Active) {
        Serial.println("[DEBUG] Activating cooling stage 1 relay");
        digitalWrite(COOL_RELAY_1_PIN, HIGH); // Activate stage 1
        stage1Active = true;
        stage1StartTime = millis(); // Record the start time
        stage2Active = false; // Ensure stage 2 is off initially
        Serial.printf("[DEBUG] Stage 1 cooling activated - relay pin %d set HIGH\n", COOL_RELAY_1_PIN);
    } else {
        Serial.printf("[DEBUG] Cooling stage 1 already active (stage1Active=%d)\n", stage1Active);
    }
    // Check if it's time to activate stage 2 based on hybrid approach
    if (!stage2Active && 
            ((millis() - stage1StartTime) / 1000 >= stage1MinRuntime) && // Minimum run time condition
            (currentTemp > setTempCool + tempSwing + stage2TempDelta) && // Temperature delta condition
            stage2CoolingEnabled) { // Check if stage 2 cooling is enabled
        digitalWrite(COOL_RELAY_2_PIN, HIGH); // Activate stage 2
        stage2Active = true;
        Serial.println("Stage 2 cooling activated");
    }
    
    // Control fan based on fanRelayNeeded setting
    // BUT: Never override manual "on" mode - user takes priority
    if (fanMode == "on") {
        // User has manually set fan to always on - respect that
        if (!fanOn) {
            digitalWrite(FAN_RELAY_PIN, HIGH);
            fanOn = true;
            Serial.println("Fan activated with cooling (manual 'on' mode)");
        }
    } else if (fanRelayNeeded) {
        if (!fanOn) {
            digitalWrite(FAN_RELAY_PIN, HIGH);
            fanOn = true;
            Serial.println("Fan activated with cooling");
        }
    } else {
        // HVAC controls its own fan, turn ours off
        if (fanOn) {
            digitalWrite(FAN_RELAY_PIN, LOW);
            fanOn = false;
            Serial.println("Fan turned off during cool - HVAC controls fan");
        }
    }
    updateStatusLEDs(); // Update LED status
    setDisplayUpdateFlag(); // Option C: Request display update
}

void handleFanControl()
{
    bool newFanState = fanOn;  // Default: keep current state
    
    if (fanMode == "on")
    {
        newFanState = true;  // Always on
    }
    else if (fanMode == "auto")
    {
        // Auto mode: fan only runs with heating/cooling if fanRelayNeeded is true
        // If fanRelayNeeded is false, HVAC controls the fan
        if (fanRelayNeeded) {
            newFanState = (heatingOn || coolingOn);
        } else {
            newFanState = false;  // Don't control fan - HVAC system controls it
        }
    }
    else if (fanMode == "cycle")
    {
        // Cycle mode: handled by controlFanSchedule(), don't override here
        return;
    }
    
    // Only write GPIO if state actually changed (state guard)
    if (newFanState != fanOn) {
        digitalWrite(FAN_RELAY_PIN, newFanState ? HIGH : LOW);
        fanOn = newFanState;
        Serial.printf("[FAN] Fan state changed via handleFanControl: %s\n", fanOn ? "ON" : "OFF");
    }
    
    setDisplayUpdateFlag(); // Option C: Request display update
}

void controlFanSchedule()
{
    if (firstHourAfterBoot)
    {
        unsigned long currentTime = millis();
        if (currentTime >= SECONDS_PER_HOUR * 1000) // Check if an hour has passed
        {
            firstHourAfterBoot = false; // Reset the flag after the first hour
        }
        return; // Skip fan schedule during the first hour
    }

    if (fanMode == "cycle")
    {
        // Don't run cycle schedule if heating or cooling is active
        if (heatingOn || coolingOn) {
            if (!fanRelayNeeded && fanOn) {
                digitalWrite(FAN_RELAY_PIN, LOW);
                fanOn = false;
                Serial.println("[FAN SCHEDULE] Stopping fan - heating/cooling active, fanRelayNeeded=false");
            }
            return;
        }

        unsigned long currentTime = millis();
        unsigned long elapsedTime = (currentTime - lastFanRunTime) / 1000; // Convert to seconds
        unsigned long hourElapsed = elapsedTime % SECONDS_PER_HOUR;

        // If an hour has passed, reset the cycle
        if (elapsedTime >= SECONDS_PER_HOUR)
        {
            lastFanRunTime = currentTime;
            hourElapsed = 0;
        }

        // Calculate which 5-minute increment we're in (0-11)
        // fanMinutesPerHour determines how many 5-min increments to run
        // Example: 15 minutes = 3 increments of 5 minutes
        unsigned long totalIncrements = (fanMinutesPerHour / 5);
        unsigned long currentIncrement = (hourElapsed / 300);  // 300 seconds = 5 minutes
        
        // Validate totalIncrements (max 12 for 60 minutes)
        if (totalIncrements > 12) {
            totalIncrements = 12;  // Cap at 60 minutes
        }
        if (totalIncrements == 0) {
            totalIncrements = 1;  // Minimum 1 increment (5 minutes)
        }

        // Fan should run during first N increments, then off for remainder
        bool shouldRun = (currentIncrement < totalIncrements);
        
        // Only write GPIO if state actually changed
        if (shouldRun != fanOn) {
            digitalWrite(FAN_RELAY_PIN, shouldRun ? HIGH : LOW);
            fanOn = shouldRun;
            Serial.printf("[FAN SCHEDULE] Cycle mode: increment %lu/%lu, fan %s\n", 
                         currentIncrement, totalIncrements, fanOn ? "ON" : "OFF");
        }
        
        updateStatusLEDs(); // Update LED status
    }
    // Retain auto mode for backward compatibility
    else if (fanMode == "auto")
    {
        // No scheduled fan running in auto mode - handled by handleFanControl()
    }
}

void handleWebRequests()
{
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        String html = generateStatusPage(currentTemp, currentHumidity, hydronicTemp, 
                                       thermostatMode, fanMode, version_info, hostname, 
                                       useFahrenheit, hydronicHeatingEnabled,
                                       HEAT_RELAY_1_PIN, HEAT_RELAY_2_PIN, COOL_RELAY_1_PIN, 
                                       COOL_RELAY_2_PIN, FAN_RELAY_PIN,
                                       setTempHeat, setTempCool, setTempAuto,
                                       tempSwing, autoTempSwing,
                                       fanRelayNeeded, stage1MinRuntime, 
                                       stage2TempDelta, fanMinutesPerHour,
                                       stage2HeatingEnabled, stage2CoolingEnabled,
                                       hydronicTempLow, hydronicTempHigh,
                                       wifiSSID, wifiPassword, timeZone,
                                       use24HourClock, mqttEnabled, mqttServer,
                                       mqttPort, mqttUsername, mqttPassword,
                                       tempOffset, humidityOffset, currentBrightness,
                                       displaySleepEnabled, displaySleepTimeout,
                                       weekSchedule, scheduleEnabled, activePeriod,
                                       scheduleOverride,
                                       weatherSource, owmApiKey, owmCity, owmState, owmCountry,
                                       haUrl, haToken, haEntityId, weatherUpdateInterval);
        request->send(200, "text/html", html);
    });

    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        String html = generateSettingsPage(thermostatMode, fanMode, setTempHeat, setTempCool, setTempAuto, 
                                          tempSwing, autoTempSwing, fanRelayNeeded, useFahrenheit, mqttEnabled, 
                                          stage1MinRuntime, stage2TempDelta, stage2HeatingEnabled, stage2CoolingEnabled,
                                          hydronicHeatingEnabled, hydronicTempLow, hydronicTempHigh, fanMinutesPerHour,
                                          mqttServer, mqttPort, mqttUsername, mqttPassword, wifiSSID, wifiPassword,
                                          hostname, use24HourClock, timeZone, tempOffset, humidityOffset, displaySleepEnabled,
                                          displaySleepTimeout);
        request->send(200, "text/html", html);
    });

    server.on("/confirm_restore", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String html = generateFactoryResetPage();
        request->send(200, "text/html", html);
    });

    server.on("/restore_defaults", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        restoreDefaultSettings();
        request->send(200, "text/plain", "Default settings restored! Please go back to the previous page."); });

    server.on("/set", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("setTempHeat", true)) {
            setTempHeat = request->getParam("setTempHeat", true)->value().toFloat();
            if (setTempHeat < 50) setTempHeat = 50;
            if (setTempHeat > 95) setTempHeat = 95;
        }
        if (request->hasParam("setTempCool", true)) {
            setTempCool = request->getParam("setTempCool", true)->value().toFloat();
            if (setTempCool < 50) setTempCool = 50;
            if (setTempCool > 95) setTempCool = 95;
        }
        if (request->hasParam("setTempAuto", true)) {
            setTempAuto = request->getParam("setTempAuto", true)->value().toFloat();
            if (setTempAuto < 50) setTempAuto = 50;
            if (setTempAuto > 95) setTempAuto = 95;
        }
        if (request->hasParam("tempSwing", true)) {
            tempSwing = request->getParam("tempSwing", true)->value().toFloat();
        }
        if (request->hasParam("autoTempSwing", true)) {
            autoTempSwing = request->getParam("autoTempSwing", true)->value().toFloat();
        }
        if (request->hasParam("fanRelayNeeded", true)) {
            fanRelayNeeded = request->getParam("fanRelayNeeded", true)->value() == "on";
        } else {
            fanRelayNeeded = false; // Ensure fanRelayNeeded is set to false if not present in the form
        }
        if (request->hasParam("useFahrenheit", true)) {
            useFahrenheit = request->getParam("useFahrenheit", true)->value() == "on";
        }
        if (request->hasParam("mqttEnabled", true)) {
            mqttEnabled = request->getParam("mqttEnabled", true)->value() == "on";
        } else {
            mqttEnabled = false; // Ensure mqttEnabled is set to false if not present in the form
        }
        if (request->hasParam("hydronicHeatingEnabled", true)) {
            hydronicHeatingEnabled = request->getParam("hydronicHeatingEnabled", true)->value() == "on"; // Handle hydronic heating option
        } else {
            hydronicHeatingEnabled = false; // Ensure hydronicHeatingEnabled is set to false if not present in the form
        }
        if (request->hasParam("hydronicTempLow", true)) {
            hydronicTempLow = request->getParam("hydronicTempLow", true)->value().toFloat(); // Handle hydronic temp low
        }
        if (request->hasParam("hydronicTempHigh", true)) {
            hydronicTempHigh = request->getParam("hydronicTempHigh", true)->value().toFloat(); // Handle hydronic temp high
        }
        if (request->hasParam("fanMinutesPerHour", true)) {
            fanMinutesPerHour = request->getParam("fanMinutesPerHour", true)->value().toInt();
        }
        if (request->hasParam("mqttServer", true)) {
            mqttServer = request->getParam("mqttServer", true)->value(); // Ensure mqttServer is updated correctly
        }
        if (request->hasParam("mqttPort", true)) {
            mqttPort = request->getParam("mqttPort", true)->value().toInt(); // Ensure mqttPort is updated correctly
        }
        if (request->hasParam("mqttUsername", true)) {
            mqttUsername = request->getParam("mqttUsername", true)->value(); // Ensure mqttUsername is updated correctly
        }
        if (request->hasParam("mqttPassword", true)) {
            mqttPassword = request->getParam("mqttPassword", true)->value(); // Ensure mqttPassword is updated correctly
        }
        if (request->hasParam("wifiSSID", true)) {
            wifiSSID = request->getParam("wifiSSID", true)->value(); // Ensure wifiSSID is updated correctly
        }
        if (request->hasParam("wifiPassword", true)) {
            String newWifiPassword = request->getParam("wifiPassword", true)->value();
            if (!newWifiPassword.isEmpty()) {
                wifiPassword = newWifiPassword; // Update wifiPassword only if a new one is provided
            }
        }
        if (request->hasParam("hostname", true)) {
            hostname = request->getParam("hostname", true)->value(); // Ensure hostname is updated correctly
        }
        if (request->hasParam("clockFormat", true)) {
            use24HourClock = request->getParam("clockFormat", true)->value() == "24";
        }
        if (request->hasParam("timeZone", true)) {
            timeZone = request->getParam("timeZone", true)->value();
            setenv("TZ", timeZone.c_str(), 1);
            tzset();
        }
        if (request->hasParam("thermostatMode", true)) {
            thermostatMode = request->getParam("thermostatMode", true)->value();
        }
        if (request->hasParam("fanMode", true)) {
            fanMode = request->getParam("fanMode", true)->value();
        }
        if (request->hasParam("stage1MinRuntime", true)) {
            stage1MinRuntime = request->getParam("stage1MinRuntime", true)->value().toInt();
        }
        if (request->hasParam("stage2TempDelta", true)) {
            stage2TempDelta = request->getParam("stage2TempDelta", true)->value().toFloat();
        }
        if (request->hasParam("stage2HeatingEnabled", true)) {
            stage2HeatingEnabled = request->getParam("stage2HeatingEnabled", true)->value() == "on";
        } else {
            stage2HeatingEnabled = false; // Ensure stage2HeatingEnabled is set to false if not present in the form
        }
        if (request->hasParam("stage2CoolingEnabled", true)) {
            stage2CoolingEnabled = request->getParam("stage2CoolingEnabled", true)->value() == "on";
        } else {
            stage2CoolingEnabled = false; // Ensure stage2CoolingEnabled is set to false if not present in the form
        }
        if (request->hasParam("tempOffset", true)) {
            tempOffset = request->getParam("tempOffset", true)->value().toFloat();
            // Constrain to reasonable range (-10°C to +10°C)
            tempOffset = constrain(tempOffset, -10.0, 10.0);
        }
        if (request->hasParam("humidityOffset", true)) {
            humidityOffset = request->getParam("humidityOffset", true)->value().toFloat();
            // Constrain to reasonable range (-50% to +50%)
            humidityOffset = constrain(humidityOffset, -50.0, 50.0);
        }
        if (request->hasParam("displaySleepEnabled", true)) {
            displaySleepEnabled = request->getParam("displaySleepEnabled", true)->value() == "on";
        } else {
            displaySleepEnabled = false;
        }
        if (request->hasParam("displaySleepTimeout", true)) {
            unsigned long timeoutMinutes = request->getParam("displaySleepTimeout", true)->value().toInt();
            // Constrain to reasonable range (1 minute to 60 minutes)
            timeoutMinutes = constrain(timeoutMinutes, 1UL, 60UL);
            displaySleepTimeout = timeoutMinutes * 60000; // Convert to milliseconds
        }
        if (request->hasParam("currentBrightness", true)) {
            currentBrightness = request->getParam("currentBrightness", true)->value().toInt();
            // Constrain to reasonable range (30 to 255)
            currentBrightness = constrain(currentBrightness, 30, 255);
            setBrightness(currentBrightness); // Apply brightness immediately
        }
        if (request->hasParam("use24HourClock", true)) {
            use24HourClock = request->getParam("use24HourClock", true)->value() == "on";
        } else {
            use24HourClock = false;
        }
        
        // Weather settings
        if (request->hasParam("weatherSource", true)) {
            weatherSource = request->getParam("weatherSource", true)->value().toInt();
        }
        if (request->hasParam("owmApiKey", true)) {
            owmApiKey = request->getParam("owmApiKey", true)->value();
        }
        if (request->hasParam("owmCity", true)) {
            owmCity = request->getParam("owmCity", true)->value();
        }
        if (request->hasParam("owmState", true)) {
            owmState = request->getParam("owmState", true)->value();
        }
        if (request->hasParam("owmCountry", true)) {
            owmCountry = request->getParam("owmCountry", true)->value();
        }
        if (request->hasParam("haUrl", true)) {
            haUrl = request->getParam("haUrl", true)->value();
        }
        if (request->hasParam("haToken", true)) {
            haToken = request->getParam("haToken", true)->value();
        }
        if (request->hasParam("haEntityId", true)) {
            haEntityId = request->getParam("haEntityId", true)->value();
        }
        if (request->hasParam("weatherUpdateInterval", true)) {
            weatherUpdateInterval = request->getParam("weatherUpdateInterval", true)->value().toInt();
            if (weatherUpdateInterval < 5) weatherUpdateInterval = 5;
            if (weatherUpdateInterval > 60) weatherUpdateInterval = 60;
        }

        saveSettings();
        
        // Reconfigure weather module if weather settings were provided
        if (request->hasParam("weatherSource", true)) {
            Serial.println("WEATHER CONFIG: Reconfiguring weather module from web interface");
            Serial.printf("  Source: %d\n", weatherSource);
            Serial.printf("  Update Interval: %d minutes\n", weatherUpdateInterval);
            
            weather.setUseFahrenheit(useFahrenheit);
            weather.setSource((WeatherSource)weatherSource);
            weather.setOpenWeatherMapConfig(owmApiKey, owmCity, owmState, owmCountry);
            weather.setHomeAssistantConfig(haUrl, haToken, haEntityId);
            weather.setUpdateInterval(weatherUpdateInterval * 60000);
            
            bool success = weather.update(); // Force immediate update
            Serial.printf("WEATHER CONFIG: Immediate update %s\n", success ? "SUCCESS" : "FAILED");
            if (!success) {
                Serial.printf("WEATHER CONFIG: Error: %s\n", weather.getLastError().c_str());
            }
        }
        
        sendMQTTData();
        publishHomeAssistantDiscovery(); // Publish discovery messages after saving settings
        request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"Settings saved successfully!\"}"); });

    server.on("/set_heating", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("heating", true)) {
            String heatingState = request->getParam("heating", true)->value();
            heatingOn = heatingState == "on";
            digitalWrite(HEAT_RELAY_1_PIN, heatingOn ? HIGH : LOW);
            digitalWrite(HEAT_RELAY_2_PIN, heatingOn ? HIGH : LOW);
            request->send(200, "application/json", "{\"heating\": \"" + heatingState + "\"}");
        } else {
            request->send(400, "application/json", "{\"error\": \"Invalid request\"}");
        } });

    server.on("/set_cooling", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("cooling", true)) {
            String coolingState = request->getParam("cooling", true)->value();
            coolingOn = coolingState == "on";
            digitalWrite(COOL_RELAY_1_PIN, coolingOn ? HIGH : LOW);
            digitalWrite(COOL_RELAY_2_PIN, coolingOn ? HIGH : LOW);
            request->send(200, "application/json", "{\"cooling\": \"" + coolingState + "\"}");
        } else {
            request->send(400, "application/json", "{\"error\": \"Invalid request\"}");
        } });

    server.on("/set_fan", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("fan", true)) {
            String fanState = request->getParam("fan", true)->value();
            fanOn = fanState == "on";
            digitalWrite(FAN_RELAY_PIN, fanOn ? HIGH : LOW);
            request->send(200, "application/json", "{\"fan\": \"" + fanState + "\"}");
        } else {
            request->send(400, "application/json", "{\"error\": \"Invalid request\"}");
        } });

    server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // Use filtered temperature from sensor task instead of raw reading
        String response = "{\"temperature\": \"" + String(currentTemp) + "\"}";
        request->send(200, "application/json", response); });

    server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request)
              { 
        sensors_event_t humidity, temp;
        aht.getEvent(&humidity, &temp);
        float currentHumidity = getCalibratedHumidity(humidity.relative_humidity);
        String response = "{\"humidity\": \"" + String(currentHumidity) + "\"}";
        request->send(200, "application/json", response); });

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String response = "{\"currentTemp\": \"" + String(currentTemp) + "\",";
        response += "\"currentHumidity\": \"" + String(currentHumidity) + "\",";
        response += "\"setTempHeat\": \"" + String(setTempHeat) + "\",";
        response += "\"setTempCool\": \"" + String(setTempCool) + "\",";
        response += "\"setTempAuto\": \"" + String(setTempAuto) + "\",";
        response += "\"tempSwing\": \"" + String(tempSwing) + "\",";
        response += "\"thermostatMode\": \"" + thermostatMode + "\",";
        response += "\"fanMode\": \"" + fanMode + "\"}";
        request->send(200, "application/json", response); });

    server.on("/version", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String response = "{\"version\": \"" + sw_version + "\",";
        response += "\"build_date\": \"" + build_date + "\",";
        response += "\"build_time\": \"" + build_time + "\",";
        response += "\"full_version\": \"" + version_info + "\",";
        response += "\"hostname\": \"" + hostname + "\"}";
        request->send(200, "application/json", response); });

    server.on("/control", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (request->hasParam("setTempHeat", true)) {
            setTempHeat = request->getParam("setTempHeat", true)->value().toFloat();
            if (setTempHeat < 50) setTempHeat = 50;
            if (setTempHeat > 95) setTempHeat = 95;
        }
        if (request->hasParam("setTempCool", true)) {
            setTempCool = request->getParam("setTempCool", true)->value().toFloat();
            if (setTempCool < 50) setTempCool = 50;
            if (setTempCool > 95) setTempCool = 95;
        }
        if (request->hasParam("setTempAuto", true)) {
            setTempAuto = request->getParam("setTempAuto", true)->value().toFloat();
            if (setTempAuto < 50) setTempAuto = 50;
            if (setTempAuto > 95) setTempAuto = 95;
        }
        if (request->hasParam("tempSwing", true)) {
            tempSwing = request->getParam("tempSwing", true)->value().toFloat();
        }
        if (request->hasParam("thermostatMode", true)) {
            thermostatMode = request->getParam("thermostatMode", true)->value();
        }
        if (request->hasParam("fanMode", true)) {
            fanMode = request->getParam("fanMode", true)->value();
        }

        saveSettings();
        sendMQTTData();
        request->send(200, "application/json", "{\"status\": \"success\"}");
    });

    server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        request->send(200, "text/html", "<html><head><meta http-equiv='refresh' content='10;url=/'>" 
                      "<title>Rebooting...</title></head><body>" 
                      "<h1>Rebooting Device...</h1>" 
                      "<p>Please wait while the device restarts. You will be redirected to the main page in 10 seconds.</p>" 
                      "<p><a href='/'>Return to Main Page</a></p>" 
                      "</body></html>");
        delay(1000);
        ESP.restart();
    });
    
    server.on("/reboot", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        request->send(200, "text/plain", "Rebooting...");
        delay(1000);
        ESP.restart();
    });

    // OTA status JSON for client-side fallback progress polling
    server.on("/update_status", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = "{";
        json += "\"bytes\":" + String(otaBytesWritten) + ",";
        json += "\"total\":" + String(otaTotalSize) + ",";
        if (otaTotalSize > 0) {
            int pct = (otaBytesWritten * 100) / otaTotalSize;
            json += "\"percent\":" + String(pct) + ",";
        }
        json += "\"state\":\"" + String(otaRebooting ? "rebooting" : (otaInProgress ? "writing" : "idle")) + "\"";
        if (otaInProgress && otaStartTime) {
            unsigned long elapsed = millis() - otaStartTime;
            json += ",\"elapsed_ms\":" + String(elapsed);
        }
        json += "}";
        request->send(200, "application/json", json);
    });

    server.on("/update", HTTP_POST, 
        [](AsyncWebServerRequest *request) {
            bool updateSuccess = !Update.hasError();
            otaInProgress = false;
            if (updateSuccess) {
                otaRebooting = true;
                Serial.println("[OTA] Update SUCCESS - sending response and scheduling reboot...");
                // Send response immediately so client gets it before connection drops
                AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Update successful! Rebooting...");
                response->addHeader("Connection", "close");
                request->send(response);
                // Longer delay to ensure response is fully transmitted before reboot
                delay(1500);
                Serial.println("[OTA] Rebooting now...");
                ESP.restart();
            } else {
                otaRebooting = false;
                String error = "Update failed: ";
                if (Update.getError() == UPDATE_ERROR_SIZE) error += "File too large";
                else if (Update.getError() == UPDATE_ERROR_SPACE) error += "Not enough space";
                else if (Update.getError() == UPDATE_ERROR_MD5) error += "MD5 check failed";
                else if (Update.getError() == UPDATE_ERROR_MAGIC_BYTE) error += "Invalid firmware file";
                else error += "Error code " + String(Update.getError());
                Serial.printf("[OTA] Update FAILED: %s\n", error.c_str());
                request->send(500, "text/plain", error);
            }
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            if (!index) {
                Serial.println("\n[OTA] Starting firmware update...");
                Serial.printf("[OTA] Filename: %s\n", filename.c_str());
                Serial.printf("[OTA] Free space: %u bytes\n", ESP.getFreeSketchSpace());
                otaBytesWritten = 0;
                otaTotalSize = request->contentLength(); // Capture total upload size
                Serial.printf("[OTA] Total size: %u bytes\n", (unsigned)otaTotalSize);
                otaInProgress = true;
                otaRebooting = false;
                otaStartTime = millis();
                otaLastUpdateLog = otaStartTime;
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Serial.printf("[OTA] Update.begin() failed: %s\n", Update.errorString());
                    otaInProgress = false;
                    return;
                }
                Update.setMD5("");
            }
            if (len) {
                size_t written = Update.write(data, len);
                if (written != len) {
                    Serial.printf("[OTA] Write error: expected %u bytes, wrote %u bytes\n", len, written);
                    otaInProgress = false;
                    return;
                }
                otaBytesWritten += written;
                unsigned long now = millis();
                if (now - otaLastUpdateLog > 1000) { // Update every second for smoother progress
                    int pct = otaTotalSize > 0 ? (otaBytesWritten * 100) / otaTotalSize : 0;
                    Serial.printf("[OTA] Flash write: %u / %u bytes (%d%%)\n", 
                                  (unsigned)otaBytesWritten, (unsigned)otaTotalSize, pct);
                    otaLastUpdateLog = now;
                }
            }
            if (final) {
                if (Update.end(true)) {
                    Serial.printf("[OTA] Update complete! Total bytes: %u\n", (unsigned)(index + len));
                } else {
                    Serial.printf("[OTA] Update.end() failed: %s\n", Update.errorString());
                }
            }
        }
    );

    // Schedule management route (schedule interface is now embedded in main page)

    server.on("/schedule_set", HTTP_POST, [](AsyncWebServerRequest *request)
    {
        bool settingsChanged = false;
        
        // Master enable/disable
        if (request->hasParam("scheduleEnabled", true)) {
            bool newEnabled = request->getParam("scheduleEnabled", true)->value() == "on";
            if (newEnabled != scheduleEnabled) {
                scheduleEnabled = newEnabled;
                settingsChanged = true;
                if (!scheduleEnabled) {
                    activePeriod = "manual";
                    scheduleOverride = false;
                    overrideEndTime = 0;
                }
            }
        } else {
            if (scheduleEnabled) {
                scheduleEnabled = false;
                activePeriod = "manual";
                scheduleOverride = false;
                overrideEndTime = 0;
                settingsChanged = true;
            }
        }
        
        // Schedule override control
        if (request->hasParam("scheduleOverride", true)) {
            String action = request->getParam("scheduleOverride", true)->value();
            if (action == "temporary") {
                scheduleOverride = true;
                overrideEndTime = millis() + (2 * 60 * 60 * 1000); // 2 hours
                settingsChanged = true;
            } else if (action == "permanent") {
                scheduleOverride = true;
                overrideEndTime = 0; // Permanent until manually disabled
                settingsChanged = true;
            } else if (action == "resume") {
                scheduleOverride = false;
                overrideEndTime = 0;
                settingsChanged = true;
            }
        }
        
        // Process schedule updates for each day
        for (int day = 0; day < 7; day++) {
            String dayPrefix = "day" + String(day) + "_";
            
            // Day enabled
            if (request->hasParam((dayPrefix + "enabled").c_str(), true)) {
                weekSchedule[day].enabled = request->getParam((dayPrefix + "enabled").c_str(), true)->value() == "on";
                settingsChanged = true;
            } else if (weekSchedule[day].enabled) {
                weekSchedule[day].enabled = false;
                settingsChanged = true;
            }
            
            // Day period - parse time from time input
            if (request->hasParam((dayPrefix + "day_time").c_str(), true)) {
                String timeStr = request->getParam((dayPrefix + "day_time").c_str(), true)->value();
                int colonPos = timeStr.indexOf(':');
                if (colonPos > 0) {
                    weekSchedule[day].day.hour = timeStr.substring(0, colonPos).toInt();
                    weekSchedule[day].day.minute = timeStr.substring(colonPos + 1).toInt();
                    settingsChanged = true;
                }
            }
            if (request->hasParam((dayPrefix + "day_heat").c_str(), true)) {
                weekSchedule[day].day.heatTemp = request->getParam((dayPrefix + "day_heat").c_str(), true)->value().toFloat();
                settingsChanged = true;
            }
            if (request->hasParam((dayPrefix + "day_cool").c_str(), true)) {
                weekSchedule[day].day.coolTemp = request->getParam((dayPrefix + "day_cool").c_str(), true)->value().toFloat();
                settingsChanged = true;
            }
            if (request->hasParam((dayPrefix + "day_auto").c_str(), true)) {
                weekSchedule[day].day.autoTemp = request->getParam((dayPrefix + "day_auto").c_str(), true)->value().toFloat();
                settingsChanged = true;
            }
            
            // Night period - parse time from time input
            if (request->hasParam((dayPrefix + "night_time").c_str(), true)) {
                String timeStr = request->getParam((dayPrefix + "night_time").c_str(), true)->value();
                int colonPos = timeStr.indexOf(':');
                if (colonPos > 0) {
                    weekSchedule[day].night.hour = timeStr.substring(0, colonPos).toInt();
                    weekSchedule[day].night.minute = timeStr.substring(colonPos + 1).toInt();
                    settingsChanged = true;
                }
            }
            if (request->hasParam((dayPrefix + "night_heat").c_str(), true)) {
                weekSchedule[day].night.heatTemp = request->getParam((dayPrefix + "night_heat").c_str(), true)->value().toFloat();
                settingsChanged = true;
            }
            if (request->hasParam((dayPrefix + "night_cool").c_str(), true)) {
                weekSchedule[day].night.coolTemp = request->getParam((dayPrefix + "night_cool").c_str(), true)->value().toFloat();
                settingsChanged = true;
            }
            if (request->hasParam((dayPrefix + "night_auto").c_str(), true)) {
                weekSchedule[day].night.autoTemp = request->getParam((dayPrefix + "night_auto").c_str(), true)->value().toFloat();
                settingsChanged = true;
            }
        }
        
        if (settingsChanged) {
            scheduleUpdatedFlag = true;
            saveScheduleSettings();
            Serial.println("SCHEDULE: Settings updated via web interface");
        }
        
        request->send(200, "text/plain", "Schedule settings updated!");
    });

    // Weather refresh endpoint for manual force update
    server.on("/weather_refresh", HTTP_POST, [](AsyncWebServerRequest *request) {
        weather.forceUpdate();
        request->send(200, "text/plain", "Weather update forced");
    });
}

void updateDisplay(float currentTemp, float currentHumidity)
{
    // Skip display updates when asleep to prevent flickering
    if (displayIsAsleep) {
        return;
    }
    
    unsigned long displayStart = millis();
    Serial.printf("[DEBUG] updateDisplay start at %lu\n", displayStart);
    
    // Get current time - skip if no WiFi to avoid 5-second delay
    unsigned long beforeTime = millis();
    Serial.printf("[DEBUG] About to call getLocalTime\n");
    struct tm timeinfo;
    if (WiFi.status() == WL_CONNECTED && getLocalTime(&timeinfo))
    {
        unsigned long afterTime = millis();
        Serial.printf("[DEBUG] getLocalTime took %lu ms\n", afterTime - beforeTime);
        // Build formatted time string: "10:40 Monday Dec 1 2025"
        char timePart[8];
        if (use24HourClock) {
            strftime(timePart, sizeof(timePart), "%H:%M", &timeinfo);
        } else {
            strftime(timePart, sizeof(timePart), "%I:%M", &timeinfo);
            // Remove leading zero for 12-hour format
            if (timePart[0] == '0') {
                for (int i = 0; i < (int)strlen(timePart); ++i) timePart[i] = timePart[i+1];
            }
        }
        char dayName[16];
        strftime(dayName, sizeof(dayName), "%A", &timeinfo);
        char monthName[8];
        strftime(monthName, sizeof(monthName), "%b", &timeinfo);
        int dayNum = timeinfo.tm_mday;
        int yearNum = timeinfo.tm_year + 1900;

        char headerLine[64];
        snprintf(headerLine, sizeof(headerLine), "%s %s %s %d %d", timePart, dayName, monthName, dayNum, yearNum);

        // Display current time/date header only when it changes to reduce flicker
        static String lastHeaderLine = "";
        String headerNow(headerLine);
        if (headerNow != lastHeaderLine) {
            tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
            tft.setTextSize(2);
            tft.setCursor(0, 0);
            tft.print(headerLine);
            // If new line is shorter, overwrite the remaining area with spaces
            int diff = lastHeaderLine.length() - headerNow.length();
            if (diff > 0) {
                for (int i = 0; i < diff; i++) tft.print(' ');
            }
            lastHeaderLine = headerNow;
        }
        
        unsigned long afterTimeOps = millis();
        Serial.printf("[DEBUG] Time operations took %lu ms\n", afterTimeOps - beforeTime);
    } else {
        unsigned long afterFailedTime = millis();
        Serial.printf("[DEBUG] getLocalTime failed, took %lu ms\n", afterFailedTime - beforeTime);
    }
    
    // Display weather if enabled and data is valid
    static bool lastWeatherDisplayState = false;
    if (weatherSource != 0 && weather.isDataValid()) {
        if (!lastWeatherDisplayState) {
            Serial.println("WEATHER DISPLAY: Showing weather on TFT");
            WeatherData data = weather.getData();
            Serial.printf("  Temp: %.1f, Condition: %s\n", data.temperature, data.condition.c_str());
            lastWeatherDisplayState = true;
        }
        weather.displayOnTFT(tft, 5, 25, useFahrenheit);
    } else if (weatherSource != 0) {
        if (lastWeatherDisplayState) {
            Serial.printf("WEATHER DISPLAY: Clearing (source=%d, valid=%d)\n", 
                         weatherSource, weather.isDataValid());
            lastWeatherDisplayState = false;
        }
        // Clear weather area if weather is enabled but data invalid
        tft.fillRect(5, 25, 110, 40, COLOR_BACKGROUND);
    } else if (lastWeatherDisplayState) {
        Serial.println("WEATHER DISPLAY: Weather disabled, clearing display");
        tft.fillRect(5, 25, 110, 40, COLOR_BACKGROUND);
        lastWeatherDisplayState = false;
    }

    // Update temperature and humidity only if they have changed
    if (currentTemp != previousTemp || currentHumidity != previousHumidity)
    {
        // Display temperature and humidity on the right side vertically
        // Background color in setTextColor will clear as it writes
        tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
        tft.setTextSize(2); // Adjust text size to fit the display
        tft.setRotation(1); // Set rotation for vertical display
        tft.setCursor(240, 30); // Move temperature down a bit
        char tempStr[6];
        dtostrf(currentTemp, 4, 1, tempStr); // Convert temperature to string with 1 decimal place
        tft.print(tempStr);
        tft.println(useFahrenheit ? "F" : "C");

        tft.setCursor(240, 70); // Move humidity down a bit
        char humidityStr[6];
        dtostrf(currentHumidity, 4, 1, humidityStr); // Convert humidity to string with 1 decimal place
        tft.print(humidityStr);
        tft.println("%");
        
        // Display pressure if BME280 sensor is active
        if (activeSensor == SENSOR_BME280 && !isnan(currentPressure)) {
            tft.setCursor(240, 110); // Position below humidity
            char pressureStr[7];
            dtostrf(currentPressure, 5, 1, pressureStr); // Format as "XXX.X"
            tft.print(pressureStr);
            tft.println(" hPa");
        } else {
            // Clear pressure area if not BME280 or invalid
            tft.fillRect(240, 110, 80, 20, COLOR_BACKGROUND);
        }

        // Update previous values
        previousTemp = currentTemp;
        previousHumidity = currentHumidity;
    }

    // Display hydronic temperature if enabled and sensor is present
    static bool prevHydronicDisplayState = false;
    
    // Only update hydronic temp display if it's changed or display state has changed
    // if (hydronicHeatingEnabled && ds18b20SensorPresent) {
    if (hydronicHeatingEnabled) {
        if (hydronicTemp != previousHydronicTemp || !prevHydronicDisplayState) {
            // Display hydronic temperature on right side (shift down if BME280 active)
            int yPos = (activeSensor == SENSOR_BME280 && !isnan(currentPressure)) ? 150 : 110;
            tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
            tft.setTextSize(2);
            tft.setCursor(240, yPos);
            char hydronicTempStr[6];
            dtostrf(hydronicTemp, 4, 1, hydronicTempStr);
            tft.print(hydronicTempStr);
            tft.println(useFahrenheit ? "F" : "C");
            
            // Clear old position if it moved
            if (yPos == 150) {
                tft.fillRect(240, 110, 80, 40, COLOR_BACKGROUND);
            }
            
            previousHydronicTemp = hydronicTemp;
            prevHydronicDisplayState = true;
        }
    } 
    else if (prevHydronicDisplayState) {
        // If hydronic heating is disabled or sensor not present, clear both possible areas
        tft.fillRect(240, 110, 80, 40, COLOR_BACKGROUND);
        tft.fillRect(240, 150, 80, 40, COLOR_BACKGROUND);
        prevHydronicDisplayState = false;
    }

    // Display hydronic lockout warning if active
    static bool prevHydronicLockoutDisplay = false;
    if (hydronicHeatingEnabled && hydronicLockout) {
        if (!prevHydronicLockoutDisplay) {
            // Show lockout warning
            tft.fillRect(10, 20, 200, 30, COLOR_WARNING);
            tft.setTextColor(TFT_BLACK, COLOR_WARNING);
            tft.setTextSize(2);
            tft.setCursor(15, 30);
            tft.print("BOILER LOCKOUT");
            prevHydronicLockoutDisplay = true;
        }
    } else if (prevHydronicLockoutDisplay) {
        // Clear lockout warning
        tft.fillRect(10, 20, 200, 30, COLOR_BACKGROUND);
        prevHydronicLockoutDisplay = false;
    }

    // Update set temperature only if it has changed and mode is not "off"
    if (thermostatMode != "off")
    {
        float currentSetTemp = (thermostatMode == "heat") ? setTempHeat : (thermostatMode == "cool") ? setTempCool : setTempAuto;
        if (currentSetTemp != previousSetTemp)
        {
            // Display set temperature in the center of the display
            tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
            tft.setTextSize(4);
            tft.setCursor(60, 100);
            char tempStr[6];
            dtostrf(currentSetTemp, 4, 1, tempStr); // Convert set temperature to string with 1 decimal place
            tft.print(tempStr);
            tft.println(useFahrenheit ? " F" : " C");

            // Update previous value
            previousSetTemp = currentSetTemp;
        }
    }
    else
    {
        // Clear the set temperature area if mode is "off"
        tft.fillRect(60, 100, 200, 40, COLOR_BACKGROUND);
    }

    // Add status indicators for heating, cooling, and fan
    // Define an area for status indicators (above the buttons)
    static bool prevHeatingStatus = false;
    static bool prevCoolingStatus = false;
    static bool prevFanStatus = false;
    
    // Use software state flags instead of GPIO reads for display indicators
    bool heatActive = heatingOn;
    bool coolActive = coolingOn;
    bool fanActive = fanOn;
    
    // Check if status has changed and update only when necessary
    if (heatActive != prevHeatingStatus || coolActive != prevCoolingStatus || fanActive != prevFanStatus)
    {
        // Clear the status indicator area - make sure to clear the full area where indicators are drawn
        tft.fillRect(0, 145, 320, 35, COLOR_BACKGROUND);
        
        // Draw heat indicator if heating is on
        if (heatActive)
        {
            tft.fillRoundRect(10, 145, 90, 30, 5, COLOR_WARNING);
            tft.setTextColor(TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(15, 152);
            tft.print("HEATING");
        }
        
        // Draw cool indicator if cooling is on
        if (coolActive)
        {
            tft.fillRoundRect(115, 145, 90, 30, 5, COLOR_PRIMARY);
            tft.setTextColor(TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(125, 152);
            tft.print("COOLING");
        }
        
        // Draw fan indicator if fan is on
        if (fanActive)
        {
            tft.fillRoundRect(220, 145, 90, 30, 5, COLOR_ACCENT);
            tft.setTextColor(TFT_BLACK);
            tft.setTextSize(2);
            tft.setCursor(240, 152);
            tft.print("FAN");
        }
        
        // Update previous status values
        prevHeatingStatus = heatActive;
        prevCoolingStatus = coolActive;
        prevFanStatus = fanActive;
    }

    // Draw buttons at the bottom
    drawButtons();
}

void saveSettings()
{
    Serial.println("Saving settings:");
    Serial.print("setTempHeat: "); Serial.println(setTempHeat);
    Serial.print("setTempCool: "); Serial.println(setTempCool);
    Serial.print("setTempAuto: "); Serial.println(setTempAuto);
    Serial.print("tempSwing: "); Serial.println(tempSwing);
    Serial.print("autoTempSwing: "); Serial.println(autoTempSwing);
    Serial.print("fanRelayNeeded: "); Serial.println(fanRelayNeeded);
    Serial.print("useFahrenheit: "); Serial.println(useFahrenheit);
    Serial.print("mqttEnabled: "); Serial.println(mqttEnabled);
    Serial.print("fanMinutesPerHour: "); Serial.println(fanMinutesPerHour);
    Serial.print("mqttServer: "); Serial.println(mqttServer);
    Serial.print("mqttPort: "); Serial.println(mqttPort);
    Serial.print("mqttUsername: "); Serial.println(mqttUsername);
    Serial.print("mqttPassword: "); Serial.println(mqttPassword);
    Serial.print("wifiSSID: "); Serial.println(wifiSSID);
    Serial.print("wifiPassword: "); Serial.println(wifiPassword);
    Serial.print("thermostatMode: "); Serial.println(thermostatMode);
    Serial.print("fanMode: "); Serial.println(fanMode);
    Serial.print("timeZone: "); Serial.println(timeZone);
    Serial.print("use24HourClock: "); Serial.println(use24HourClock);
    Serial.print("hydronicHeatingEnabled: "); Serial.println(hydronicHeatingEnabled);
    Serial.print("hydronicTempLow: "); Serial.println(hydronicTempLow);
    Serial.print("hydronicTempHigh: "); Serial.println(hydronicTempHigh);
    Serial.print("hostname: "); Serial.println(hostname);
    Serial.print("stage1MinRuntime: "); Serial.println(stage1MinRuntime);
    Serial.print("stage2TempDelta: "); Serial.println(stage2TempDelta);
    Serial.print("stage2HeatingEnabled: "); Serial.println(stage2HeatingEnabled);
    Serial.print("stage2CoolingEnabled: "); Serial.println(stage2CoolingEnabled);
    Serial.print("weatherSource: "); Serial.println(weatherSource);
    Serial.print("owmApiKey: "); Serial.println(owmApiKey.length() > 0 ? "[SET]" : "[NOT SET]");
    Serial.print("owmCity: "); Serial.println(owmCity);
    Serial.print("owmCountry: "); Serial.println(owmCountry);
    Serial.print("haUrl: "); Serial.println(haUrl);
    Serial.print("haToken: "); Serial.println(haToken.length() > 0 ? "[SET]" : "[NOT SET]");
    Serial.print("haEntityId: "); Serial.println(haEntityId);
    Serial.print("weatherUpdateInterval: "); Serial.println(weatherUpdateInterval);

    preferences.putFloat("setHeat", setTempHeat);
    preferences.putFloat("setCool", setTempCool);
    preferences.putFloat("setAuto", setTempAuto);
    preferences.putFloat("swing", tempSwing);
    preferences.putFloat("autoSwing", autoTempSwing);
    preferences.putBool("fanRelay", fanRelayNeeded);
    preferences.putBool("useF", useFahrenheit);
    preferences.putBool("mqttEn", mqttEnabled);
    preferences.putInt("fanMinHr", fanMinutesPerHour);
    preferences.putString("mqttSrv", mqttServer);
    preferences.putInt("mqttPrt", mqttPort);
    preferences.putString("mqttUsr", mqttUsername);
    preferences.putString("mqttPwd", mqttPassword);
    preferences.putString("wifiSSID", wifiSSID);
    preferences.putString("wifiPassword", wifiPassword);
    preferences.putString("thermoMd", thermostatMode);
    preferences.putString("fanMd", fanMode);
    preferences.putString("tz", timeZone);
    preferences.putBool("use24Clk", use24HourClock);
    preferences.putBool("hydHeat", hydronicHeatingEnabled);
    preferences.putFloat("hydLow", hydronicTempLow);
    preferences.putFloat("hydHigh", hydronicTempHigh);
    preferences.putBool("hydAlertSent", hydronicLowTempAlertSent);
    preferences.putString("host", hostname);
    preferences.putUInt("stg1MnRun", stage1MinRuntime);
    preferences.putFloat("stg2Delta", stage2TempDelta);
    preferences.putBool("stg2HeatEn", stage2HeatingEnabled);
    preferences.putBool("stg2CoolEn", stage2CoolingEnabled);
    preferences.putFloat("tempOffset", tempOffset);
    preferences.putFloat("humOffset", humidityOffset);
    preferences.putBool("dispSleepEn", displaySleepEnabled);
    preferences.putULong("dispTimeout", displaySleepTimeout);
    
    // Save weather settings
    preferences.putInt("weatherSrc", weatherSource);
    preferences.putString("owmApiKey", owmApiKey);
    preferences.putString("owmCity", owmCity);
    preferences.putString("owmState", owmState);
    preferences.putString("owmCountry", owmCountry);
    preferences.putString("haUrl", haUrl);
    preferences.putString("haToken", haToken);
    preferences.putString("haEntityId", haEntityId);
    preferences.putInt("weatherInt", weatherUpdateInterval);
    
    // Save schedule settings if flag is set
    if (scheduleUpdatedFlag) {
        saveScheduleSettings();
        scheduleUpdatedFlag = false;
    }

    // Debug print to confirm settings are saved
    Serial.print("Settings saved.");
}

void loadSettings()
{
    setTempHeat = preferences.getFloat("setHeat", 72.0);
    setTempCool = preferences.getFloat("setCool", 76.0);
    setTempAuto = preferences.getFloat("setAuto", 74.0);
    tempSwing = preferences.getFloat("swing", 1.0);
    autoTempSwing = preferences.getFloat("autoSwing", 1.5);
    fanRelayNeeded = preferences.getBool("fanRelay", false);
    useFahrenheit = preferences.getBool("useF", true);
    mqttEnabled = preferences.getBool("mqttEn", false);
    fanMinutesPerHour = preferences.getInt("fanMinHr", 15);
    mqttServer = preferences.getString("mqttSrv", "0.0.0.0");
    mqttPort = preferences.getInt("mqttPrt", 1883);
    mqttUsername = preferences.getString("mqttUsr", "mqtt");
    mqttPassword = preferences.getString("mqttPwd", "password");
    wifiSSID = preferences.getString("wifiSSID", "");
    wifiPassword = preferences.getString("wifiPassword", "");
    thermostatMode = preferences.getString("thermoMd", "off");
    fanMode = preferences.getString("fanMd", "auto");
    timeZone = preferences.getString("tz", "CST6CDT,M3.2.0,M11.1.0");
    use24HourClock = preferences.getBool("use24Clk", true);
    hydronicHeatingEnabled = preferences.getBool("hydHeat", false);
    hydronicTempLow = preferences.getFloat("hydLow", 110.0);
    hydronicTempHigh = preferences.getFloat("hydHigh", 130.0);
    hydronicLowTempAlertSent = preferences.getBool("hydAlertSent", false);
    hostname = preferences.getString("host", "ESP32-S3-Simple-Thermostat");
    stage1MinRuntime = preferences.getUInt("stg1MnRun", 300);
    stage2TempDelta = preferences.getFloat("stg2Delta", 2.0);
    stage2HeatingEnabled = preferences.getBool("stg2HeatEn", false);
    stage2CoolingEnabled = preferences.getBool("stg2CoolEn", false);
    tempOffset = preferences.getFloat("tempOffset", -4.0);
    humidityOffset = preferences.getFloat("humOffset", 0.0);
    displaySleepEnabled = preferences.getBool("dispSleepEn", true);
    displaySleepTimeout = preferences.getULong("dispTimeout", 300000); // Default 5 minutes
    
    // Load weather settings
    weatherSource = preferences.getInt("weatherSrc", 0);
    owmApiKey = preferences.getString("owmApiKey", "");
    owmCity = preferences.getString("owmCity", "");
    owmState = preferences.getString("owmState", "");
    owmCountry = preferences.getString("owmCountry", "");
    haUrl = preferences.getString("haUrl", "");
    haToken = preferences.getString("haToken", "");
    haEntityId = preferences.getString("haEntityId", "");
    weatherUpdateInterval = preferences.getInt("weatherInt", 10);
    
    // Debug print to confirm settings are loaded
    Serial.println("Loading settings:");
    Serial.print("setTempHeat: "); Serial.println(setTempHeat);
    Serial.print("setTempCool: "); Serial.println(setTempCool);
    Serial.print("setTempAuto: "); Serial.println(setTempAuto);
    Serial.print("tempSwing: "); Serial.println(tempSwing);
    Serial.print("autoTempSwing: "); Serial.println(autoTempSwing);
    Serial.print("fanRelayNeeded: "); Serial.println(fanRelayNeeded);
    Serial.print("useFahrenheit: "); Serial.println(useFahrenheit);
    Serial.print("mqttEnabled: "); Serial.println(mqttEnabled);
    Serial.print("fanMinutesPerHour: "); Serial.println(fanMinutesPerHour);
    Serial.print("mqttServer: "); Serial.println(mqttServer);
    Serial.print("mqttPort: "); Serial.println(mqttPort);
    Serial.print("mqttUsername: "); Serial.println(mqttUsername);
    Serial.print("mqttPassword: "); Serial.println(mqttPassword);
    Serial.print("wifiSSID: "); Serial.println(wifiSSID);
    Serial.print("wifiPassword: "); Serial.println(wifiPassword);
    Serial.print("thermostatMode: "); Serial.println(thermostatMode);
    Serial.print("fanMode: "); Serial.println(fanMode);
    Serial.print("timeZone: "); Serial.println(timeZone);
    Serial.print("use24HourClock: "); Serial.println(use24HourClock);
    Serial.print("hydronicHeatingEnabled: "); Serial.println(hydronicHeatingEnabled);
    Serial.print("hydronicTempLow: "); Serial.println(hydronicTempLow);
    Serial.print("hydronicTempHigh: "); Serial.println(hydronicTempHigh);
    Serial.print("hydronicLowTempAlertSent: "); Serial.println(hydronicLowTempAlertSent);
    Serial.print("hostname: "); Serial.println(hostname);
    Serial.print("stage1MinRuntime: "); Serial.println(stage1MinRuntime);
    Serial.print("stage2TempDelta: "); Serial.println(stage2TempDelta);
    Serial.print("stage2HeatingEnabled: "); Serial.println(stage2HeatingEnabled);
    Serial.print("stage2CoolingEnabled: "); Serial.println(stage2CoolingEnabled);
    Serial.print("tempOffset: "); Serial.println(tempOffset);
    Serial.print("humidityOffset: "); Serial.println(humidityOffset);
    Serial.print("displaySleepEnabled: "); Serial.println(displaySleepEnabled);
    Serial.print("displaySleepTimeout: "); Serial.println(displaySleepTimeout);
    Serial.print("weatherSource: "); Serial.println(weatherSource);
    Serial.print("owmApiKey: "); Serial.println(owmApiKey.length() > 0 ? "[SET]" : "[NOT SET]");
    Serial.print("owmCity: "); Serial.println(owmCity);
    Serial.print("owmState: "); Serial.println(owmState);
    Serial.print("owmCountry: "); Serial.println(owmCountry);
    Serial.print("haUrl: "); Serial.println(haUrl);
    Serial.print("haToken: "); Serial.println(haToken.length() > 0 ? "[SET]" : "[NOT SET]");
    Serial.print("haEntityId: "); Serial.println(haEntityId);
    Serial.print("weatherUpdateInterval: "); Serial.println(weatherUpdateInterval);

    // Debug print to confirm settings are loaded
    Serial.print("Settings loaded.");
}

float convertCtoF(float celsius)
{
    return celsius * 9.0 / 5.0 + 32.0;
}

void saveWiFiSettings()
{
    preferences.putString("wifiSSID", wifiSSID);
    preferences.putString("wifiPassword", wifiPassword);
}

void calibrateTouchScreen()
{
    uint16_t calData[5];
    uint8_t calDataOK = 0;

    // Check if calibration data is stored in Preferences
    if (preferences.getBytesLength("calData") == sizeof(calData))
    {
        preferences.getBytes("calData", calData, sizeof(calData));
        tft.setTouch(calData);
        calDataOK = 1;
    }

    if (calDataOK && tft.getTouchRaw(&calData[0], &calData[1]))
    {
        Serial.println("Touch screen calibration data loaded from Preferences");
    }
    else
    {
        Serial.println("Calibrating touch screen...");
        tft.fillScreen(COLOR_BACKGROUND);
        tft.setCursor(20, 0);
        tft.setTextFont(2);
        tft.setTextSize(1);
        tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);

        tft.println("Touch corners as indicated");
        tft.setTextFont(1);
        tft.println();

        tft.calibrateTouch(calData, TFT_WHITE, TFT_RED, 15);

        // Store calibration data in Preferences
        preferences.putBytes("calData", calData, sizeof(calData));
    }
}

void handleKeyboardTouch(uint16_t x, uint16_t y, bool isUpperCaseKeyboard)
{
    // Add touch debounce mechanism to prevent accidental double touches
    static unsigned long lastTouchTime = 0;
    unsigned long currentTime = millis();
    
    // If less than 300ms has passed since the last touch, ignore this touch
    if (currentTime - lastTouchTime < 300) {
        return;
    }
    
    // Only process keyboard touches if we're in WiFi setup mode
    if (!inWiFiSetupMode) {
        return;
    }
    
    // Check if touch is within the keyboard area
    if (y < 60) {  // Touch is above the keyboard, ignore
        return;
    }
    
    // Process touch within keyboard area
    for (int row = 0; row < 5; row++)
    {
        for (int col = 0; col < 10; col++)
        {
            // Use the same constants as the visual keyboard
            int keyX = col * (KEY_WIDTH + KEY_SPACING) + KEYBOARD_X_OFFSET;
            int keyY = row * (KEY_HEIGHT + KEY_SPACING) + KEYBOARD_Y_OFFSET;
            
            // Expand touch area by 15% to account for touch inaccuracy
            int touchMargin = 4; // Extra pixels around each key for easier touching
            int expandedX = keyX - touchMargin;
            int expandedY = keyY - touchMargin;
            int expandedWidth = KEY_WIDTH + (touchMargin * 2);
            int expandedHeight = KEY_HEIGHT + (touchMargin * 2);

            if (x >= expandedX && x <= expandedX + expandedWidth &&
                y >= expandedY && y <= expandedY + expandedHeight)
            {
                Serial.print("Touch at (");
                Serial.print(x);
                Serial.print(",");
                Serial.print(y);
                Serial.print(") -> Key[");
                Serial.print(row);
                Serial.print(",");
                Serial.print(col);
                Serial.print("] KeyArea(");
                Serial.print(keyX);
                Serial.print(",");
                Serial.print(keyY);
                Serial.print(" ");
                Serial.print(KEY_WIDTH);
                Serial.print("x");
                Serial.print(KEY_HEIGHT);
                Serial.println(")");
                
                // Process the key press
                handleKeyPress(row, col);
                
                // Update last touch time for debounce
                lastTouchTime = currentTime;
                
                // Exit after processing a key press to prevent multiple key detection
                return;
            }
        }
    }
}

void restoreDefaultSettings()
{
    setTempHeat = 72.0;
    setTempCool = 76.0;
    setTempAuto = 74.0;
    tempSwing = 1.0;
    autoTempSwing = 1.5;
    fanRelayNeeded = false;
    useFahrenheit = true;
    mqttEnabled = false;
    wifiSSID = "";
    wifiPassword = "";
    fanMinutesPerHour = 15;
    mqttServer = "0.0.0.0";
    mqttUsername = "mqtt";
    mqttPassword = "password";
    thermostatMode = "off";
    fanMode = "auto";
    timeZone = "CST6CDT,M3.2.0,M11.1.0"; // Reset time zone to default
    use24HourClock = true; // Reset clock format to default
    hydronicHeatingEnabled = false; // Reset hydronic heating setting to default
    hostname = "ESP32-S3-Simple-Thermostat"; // Reset hostname to default

    mqttPort = 1883; // Reset MQTT port default
    hydronicTempLow = 110.0; // Reset hydronic low temp
    hydronicTempHigh = 130.0; // Reset hydronic high temp
    hydronicLowTempAlertSent = false; // Reset hydronic alert state
    stage2HeatingEnabled = false; // Reset stage 2 heating enabled to default
    stage2CoolingEnabled = false; // Reset stage 2 cooling enabled to default
    tempOffset = -4.0; // Reset temperature calibration offset to default
    humidityOffset = 0.0; // Reset humidity calibration offset to default
    displaySleepEnabled = true; // Reset display sleep enabled to default
    displaySleepTimeout = 300000; // Reset display sleep timeout to default (5 minutes)
    
    // Reset weather settings to defaults
    weatherSource = 0; // Disabled
    owmApiKey = "";
    owmCity = "";
    owmState = "";
    owmCountry = "";
    haUrl = "";
    haToken = "";
    haEntityId = "";
    weatherUpdateInterval = 10; // 10 minutes

    saveSettings();

    // Reset the ESP32
    ESP.restart();
}

// Light sensor and display brightness control functions
void readLightSensor() {
    lastLightReading = analogRead(LIGHT_SENSOR_PIN);
}

void setBrightness(int brightness) {
    // Constrain brightness to valid range
    brightness = constrain(brightness, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
    currentBrightness = brightness;
    
    // Set PWM duty cycle (0-255)
    ledcWrite(PWM_CHANNEL, brightness);
}

void updateDisplayBrightness() {
    // Don't adjust brightness when display is asleep
    if (displayIsAsleep) {
        return;
    }
    
    unsigned long currentTime = millis();
    
    // Only update brightness every BRIGHTNESS_UPDATE_INTERVAL
    if (currentTime - lastBrightnessUpdate < BRIGHTNESS_UPDATE_INTERVAL) {
        return;
    }
    
    lastBrightnessUpdate = currentTime;
    
    // Read the light sensor
    readLightSensor();
    
    // Map light sensor reading to brightness level
    // Lower light readings = dimmer display, higher readings = brighter display
    int targetBrightness = map(lastLightReading, LIGHT_SENSOR_MIN, LIGHT_SENSOR_MAX, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
    
    // Constrain to valid range
    targetBrightness = constrain(targetBrightness, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
    
    // Only update if brightness changed significantly (reduce flickering)
    if (abs(targetBrightness - currentBrightness) > 5) {
        setBrightness(targetBrightness);
    }
}

// AHT20 sensor calibration functions
float getCalibratedTemperature(float rawTemp) {
    return rawTemp + tempOffset;
}

float getCalibratedHumidity(float rawHumidity) {
    float calibrated = rawHumidity + humidityOffset;
    // Constrain humidity to valid range (0-100%)
    return constrain(calibrated, 0.0, 100.0);
}

// Display sleep mode functions
void checkDisplaySleep() {
    // Debug: Always print status every 30 seconds regardless of enabled state
    static unsigned long lastDebugTime = 0;
    unsigned long currentTime = millis();
    
    // Track sustained motion at function scope so it persists correctly
    static unsigned long firstMotionTime = 0;
    static unsigned long lastFilterLog = 0;
    
    if (currentTime - lastDebugTime > 30000) {
        Serial.printf("[SLEEP_DEBUG] Function called - Enabled: %s, Time since touch: %lu ms / timeout: %lu ms, Display asleep: %d\n",
                      displaySleepEnabled ? "YES" : "NO",
                      currentTime - lastInteractionTime, displaySleepTimeout, displayIsAsleep);
        lastDebugTime = currentTime;
    }
    
    if (!displaySleepEnabled) {
        return; // Sleep mode disabled
    }
    
    // Check for continuous presence while display is asleep (motion wake feature)
    // Require SUSTAINED motion at close range with moderate signal to filter false positives
    if (displayIsAsleep && motionWakeEnabled && ld2410Connected) {
        // Apply cooldown: don't wake immediately after sleeping
        unsigned long timeSinceSleep = currentTime - lastSleepTime;
        if (timeSinceSleep < MOTION_WAKE_COOLDOWN) {
            return; // Still in cooldown
        }
        
        // Use sensor data already updated by readMotionSensor() - NO radar.check() call here
        // But ONLY use data that's fresh (recently updated)
        unsigned long dataAge = currentTime - radarDataTimestamp;
        if (dataAge > RADAR_DATA_MAX_AGE) {
            // Data is stale, skip this check
            if (firstMotionTime > 0) {
                Serial.printf("[MOTION_WAKE] Data too old (%lums), resetting tracker\n", dataAge);
                firstMotionTime = 0;
            }
            return;
        }
        
        // Get latest sensor readings with mutex protection
        if (radarSensorMutex == NULL || xSemaphoreTake(radarSensorMutex, pdMS_TO_TICKS(10)) != pdTRUE) {
            return; // Can't get mutex, skip this check
        }
        
        // Check for valid moving target using cached data
        bool validMotion = false;
        if (radar.movingTargetDetected()) {
            unsigned long distance = radar.movingTargetDistance();
            int signal = radar.movingTargetSignal();
            
            xSemaphoreGive(radarSensorMutex); // Release mutex immediately after reading
            
            // Validate distance and signal
            if (distance > 0 && distance < MOTION_WAKE_MAX_DISTANCE && 
                signal >= MOTION_WAKE_MIN_SIGNAL && signal <= MOTION_WAKE_MAX_SIGNAL) {
                validMotion = true;
                
                // Start or continue tracking
                if (firstMotionTime == 0) {
                    firstMotionTime = currentTime;
                    Serial.printf("[MOTION_WAKE] Started tracking: %lucm, signal %d\n", distance, signal);
                } else {
                    // Check if sustained long enough
                    unsigned long duration = currentTime - firstMotionTime;
                    if (duration >= MOTION_WAKE_DEBOUNCE) {
                        Serial.printf("[MOTION_WAKE] Sustained %lums: %lucm, signal %d - WAKING\n", 
                                      duration, distance, signal);
                        firstMotionTime = 0;
                        wakeDisplay();
                        return;
                    }
                }
            } else {
                // Log why motion was filtered
                if (currentTime - lastFilterLog > 2000) {
                    Serial.printf("[MOTION_WAKE] Filtered: %lucm (max %d), signal %d (range %d-%d)\n",
                                  distance, MOTION_WAKE_MAX_DISTANCE, signal, 
                                  MOTION_WAKE_MIN_SIGNAL, MOTION_WAKE_MAX_SIGNAL);
                    lastFilterLog = currentTime;
                }
            }
        } else {
            xSemaphoreGive(radarSensorMutex); // Release mutex if no moving target
        }
        
        // Reset if motion stopped or invalid
        if (!validMotion && firstMotionTime > 0) {
            Serial.println("[MOTION_WAKE] Motion lost - resetting tracker");
            firstMotionTime = 0;
        }
    } else {
        // Display is awake or motion wake disabled - reset tracker to prevent stale state
        if (firstMotionTime != 0) {
            firstMotionTime = 0;
        }
    }
    
    unsigned long timeSinceInteraction = currentTime - lastInteractionTime;
    
    // Check if display should go to sleep
    if (!displayIsAsleep && (timeSinceInteraction > displaySleepTimeout)) {
        Serial.printf("[SLEEP] Display going to sleep after %lu ms\n", timeSinceInteraction);
        sleepDisplay();
    }
}

void wakeDisplay() {
    if (displayIsAsleep) {
        displayIsAsleep = false;
        lastWakeTime = millis();
        lastInteractionTime = millis();
        
        // Just restore the backlight
        updateDisplayBrightness();
    }
}

void sleepDisplay() {
    if (!displayIsAsleep) {
        displayIsAsleep = true;
        lastSleepTime = millis(); // Record sleep time for motion wake cooldown
        // Turn off backlight completely (bypass MIN_BRIGHTNESS constraint)
        currentBrightness = 0;
        ledcWrite(PWM_CHANNEL, 0);
    }
}

// Motion sensor functions

// Helper function to wait for UART response
bool waitForLD2410Response(unsigned long timeoutMs) {
    unsigned long start = millis();
    while (millis() - start < timeoutMs) {
        if (Serial2.available() > 0) {
            return true;
        }
        delay(10);
    }
    return false;
}

// Configure LD2410 using raw UART commands (bypasses library initialization)
bool configureLD2410ViaRawUART() {
    Serial.println("LD2410: Configuring via raw UART commands...");
    
    // Clear buffer
    while (Serial2.available()) Serial2.read();
    delay(100);
    
    // Enter config mode
    Serial.println("  Entering config mode...");
    uint8_t enableConfig[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 
                               0xFF, 0x00, 0x01, 0x00, 
                               0x04, 0x03, 0x02, 0x01};
    Serial2.write(enableConfig, sizeof(enableConfig));
    delay(200);
    
    if (waitForLD2410Response(200)) {
        Serial.println("    ✓ Config mode enabled");
        // Clear the response
        while (Serial2.available()) Serial2.read();
    } else {
        Serial.println("    ✗ No config mode response");
        return false;
    }
    
    // Set max distance: 4 gates = 3 meters, 5 second timeout
    Serial.println("  Setting max distance (4 gates = 3m, 5s timeout)...");
    uint8_t setMaxDist[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x14, 0x00, 
                             0x60, 0x00, 0x00, 0x00, 
                             0x04, 0x00, 0x00, 0x00, // Max motion gate
                             0x04, 0x00, 0x00, 0x00, // Max static gate
                             0x05, 0x00,             // 5 second timeout
                             0x04, 0x03, 0x02, 0x01};
    Serial2.write(setMaxDist, sizeof(setMaxDist));
    delay(200);
    
    if (waitForLD2410Response(200)) {
        Serial.println("    ✓ Max distance set");
        while (Serial2.available()) Serial2.read();
    } else {
        Serial.println("    ✗ No max distance response");
    }
    
    // Set sensitivity for each gate (reduce false positives)
    Serial.println("  Setting sensitivity per gate (Motion=30, Static=20)...");
    for (uint8_t gate = 0; gate <= 4; gate++) {
        uint8_t setSensitivity[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x14, 0x00,
                                     0x64, 0x00, 0x00, 0x00,
                                     gate, 0x00, 0x00, 0x00,  // Gate number
                                     0x1E, 0x00, 0x00, 0x00,  // Motion: 30
                                     0x14, 0x00, 0x00, 0x00,  // Static: 20
                                     0x04, 0x03, 0x02, 0x01};
        Serial2.write(setSensitivity, sizeof(setSensitivity));
        delay(100);
        
        if (waitForLD2410Response(100)) {
            Serial.printf("    ✓ Gate %d configured\n", gate);
            while (Serial2.available()) Serial2.read();
        }
    }
    
    // Exit config mode
    Serial.println("  Exiting config mode...");
    uint8_t endConfig[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 
                            0xFE, 0x00, 
                            0x04, 0x03, 0x02, 0x01};
    Serial2.write(endConfig, sizeof(endConfig));
    delay(500);
    
    Serial.println("LD2410: Raw UART configuration complete");
    return true;
}

bool configureLD2410Sensitivity() {
    Serial.println("LD2410: Configuring sensor sensitivity...");
    
    // Enter configuration mode
    if (!radar.configMode()) {
        Serial.println("  ✗ Failed to enter config mode");
        return false;
    }
    
    // Read current configuration
    radar.requestParameters();
    Serial.println("  Current configuration:");
    Serial.printf("    Max range: %lu cm\n", radar.getRange_cm());
    Serial.printf("    No-one window: %d seconds\n", radar.getNoOneWindow());
    
    // Set conservative parameters to reduce false positives:
    // - Max gate 4 (~3 meters)
    // - Moving threshold: 30 (reduced from default ~50)
    // - Stationary threshold: 20 (reduced from default ~50)
    // - No-one window: 5 seconds
    
    MyLD2410::ValuesArray movingThresholds, stationaryThresholds;
    movingThresholds.N = 8;  // 0-8 is 9 gates, so N=8
    stationaryThresholds.N = 8;
    
    // Set reduced sensitivity for all gates
    for (int i = 0; i <= 8; i++) {
        movingThresholds.values[i] = (i <= 4) ? 30 : 15;      // Gates 0-4: 30, 5-8: 15
        stationaryThresholds.values[i] = (i <= 4) ? 20 : 10;  // Gates 0-4: 20, 5-8: 10
    }
    
    Serial.println("  Setting gate parameters...");
    if (!radar.setGateParameters(movingThresholds, stationaryThresholds, 5)) {
        Serial.println("  ✗ Failed to set gate parameters");
        radar.configMode(false);
        return false;
    }
    Serial.println("    ✓ Gate parameters set");
    
    // Exit configuration mode
    radar.configMode(false);
    
    Serial.println("LD2410: Configuration complete");
    return true;
}

bool testLD2410Connection() {
    Serial.println("LD2410: Testing motion sensor with MyLD2410 library...");
    Serial.println("LD2410: UART Debug Info:");
    Serial.printf("  RX Pin: %d, TX Pin: %d, Baud: 256000\n", LD2410_RX_PIN, LD2410_TX_PIN);
    Serial.printf("  Serial2 available: %d bytes\n", Serial2.available());
    
    // MyLD2410 library handles continuous stream naturally via check() method
    Serial.println("  Initializing with MyLD2410 library...");
    
    if (radar.begin()) {
        Serial.println("LD2410: ✓ Library initialized!");
        
        // Request configuration mode to read firmware
        radar.configMode();
        Serial.printf("  Firmware: %s\n", radar.getFirmware().c_str());
        Serial.printf("  Protocol version: %lu\n", radar.getVersion());
        radar.configMode(false);
        
        // Configure sensor to reduce false positives
        if (configureLD2410Sensitivity()) {
            Serial.println("LD2410: ✓ Sensor configured successfully");
        } else {
            Serial.println("LD2410: ✗ Warning - configuration may have failed");
        }
        
        return true;
    } else {
        Serial.println("LD2410: ✗ Library initialization failed");
        
        // Check digital pin as fallback
        Serial.println("  Checking digital OUT pin as fallback...");
        pinMode(LD2410_MOTION_PIN, INPUT_PULLDOWN);
        delay(100);
        
        bool readings[5];
        for(int i = 0; i < 5; i++) {
            readings[i] = digitalRead(LD2410_MOTION_PIN);
            delay(10);
        }
        
        Serial.printf("  Digital pin readings: %d %d %d %d %d\n", 
                      readings[0], readings[1], readings[2], readings[3], readings[4]);
        
        Serial.println("  WARNING: Using digital OUT pin only");
        return false;
    }
}

void readMotionSensor() {
    if (!ld2410Connected) return;
    
    // Acquire mutex for thread-safe radar access
    if (radarSensorMutex == NULL || xSemaphoreTake(radarSensorMutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return; // Can't get mutex, skip this read
    }
    
    // Guard against calling into library with an empty/partial UART buffer
    // Some LD2410 frames can be larger; ensure at least a minimal header is present
    int uartAvail = Serial2.available();
    if (uartAvail <= 0) {
        xSemaphoreGive(radarSensorMutex);
        return;
    }
    // If very few bytes are available, skip processing to avoid misaligned reads
    if (uartAvail < 8) {
        xSemaphoreGive(radarSensorMutex);
        return;
    }
    // CRITICAL: MyLD2410 library has buffer overflow bug in readFrame()
    // inBuf is only 64 bytes (LD2410_BUFFER_SIZE=0x40) but readFrame() doesn't check bounds
    // If Serial2 buffer has >64 bytes, readFrame() will write past inBuf causing heap corruption
    // Drain buffer to safe level before calling radar.check()
    if (uartAvail > 60) {
        // Leave only ~30 bytes (typical frame size) to prevent overflow
        int toDrain = uartAvail - 30;
        while (toDrain-- > 0 && Serial2.available() > 0) {
            (void)Serial2.read();
        }
        // Re-check minimal availability after draining
        if (Serial2.available() < 8) {
            xSemaphoreGive(radarSensorMutex);
            return;
        }
    }

    // Check for new data using MyLD2410 library
    // check() returns DATA when a data frame is received
    radar.check();
    
    // Update timestamp to indicate fresh data
    radarDataTimestamp = millis();
    
    // Check for presence using library's built-in detection
    bool currentPresence = radar.presenceDetected();
    
    // Debug presence state changes with detailed information
    static bool lastPresenceState = false;
    static unsigned long lastPresenceChangeTime = 0;
    
    if (currentPresence != lastPresenceState) {
        unsigned long now = millis();
        Serial.printf("LD2410: Presence %s after %lu ms\n", 
                      currentPresence ? "DETECTED" : "CLEARED",
                      now - lastPresenceChangeTime);
        
        // Show what type of target was detected
        if (currentPresence) {
            if (radar.movingTargetDetected()) {
                Serial.printf("  Moving target at %lu cm (signal: %d)\n",
                              radar.movingTargetDistance(),
                              radar.movingTargetSignal());
            }
            if (radar.stationaryTargetDetected()) {
                Serial.printf("  Stationary target at %lu cm (signal: %d)\n",
                              radar.stationaryTargetDistance(),
                              radar.stationaryTargetSignal());
            }
            
            // Wake display on NEW presence detection (state change from NO to YES)
            // Only wake if MOVING target detected with valid distance/signal to filter false positives
            // Only if motion wake is enabled and display is asleep
            if (motionWakeEnabled && displayIsAsleep && radar.movingTargetDetected()) {
                unsigned long distance = radar.movingTargetDistance();
                int signal = radar.movingTargetSignal();
                
                // Apply same filters as sustained motion wake
                if (distance > 0 && distance < MOTION_WAKE_MAX_DISTANCE && 
                    signal >= MOTION_WAKE_MIN_SIGNAL && signal <= MOTION_WAKE_MAX_SIGNAL) {
                    Serial.printf("LD2410: Waking display - NEW moving target: %lucm, signal %d\n", distance, signal);
                    wakeDisplay();
                } else {
                    Serial.printf("LD2410: Filtered NEW moving target: %lucm (max %d), signal %d (range %d-%d)\n",
                                  distance, MOTION_WAKE_MAX_DISTANCE, signal, 
                                  MOTION_WAKE_MIN_SIGNAL, MOTION_WAKE_MAX_SIGNAL);
                }
            }
        }
        
        lastPresenceState = currentPresence;
        lastPresenceChangeTime = now;
    }
    
    if (currentPresence) {
        if (!motionDetected) {
            Serial.println("LD2410: Presence activated - starting presence timer");
        }
        motionDetected = true;
        lastMotionTime = millis();
    } else {
        // Presence cleared by sensor's internal timeout (configured in no-one window parameter)
        if (motionDetected) {
            Serial.println("LD2410: Presence timeout - clearing motion flag");
            motionDetected = false;
        }
    }
    
    // Periodic status (every 10 seconds)
    static unsigned long lastDebugTime = 0;
    if (millis() - lastDebugTime > 10000) {
        lastDebugTime = millis();
        Serial.printf("LD2410: Presence=%s, Motion Flag=%s, Age=%lu ms\n",
                      currentPresence ? "YES" : "NO",
                      motionDetected ? "ACTIVE" : "INACTIVE",
                      millis() - lastMotionTime);
        
        // Show target details if present
        if (currentPresence) {
            if (radar.movingTargetDetected()) {
                Serial.printf("  Moving: %lucm @ signal %d\n",
                              radar.movingTargetDistance(),
                              radar.movingTargetSignal());
            }
            if (radar.stationaryTargetDetected()) {
                Serial.printf("  Stationary: %lucm @ signal %d\n",
                              radar.stationaryTargetDistance(),
                              radar.stationaryTargetSignal());
            }
        }
    }
    
    xSemaphoreGive(radarSensorMutex); // Release mutex
}

// LED control functions
void setHeatLED(bool state) {
    ledcWrite(PWM_CHANNEL_HEAT, state ? 128 : 0); // 50% brightness (128/255)
}

void setCoolLED(bool state) {
    ledcWrite(PWM_CHANNEL_COOL, state ? 128 : 0); // 50% brightness (128/255)
}

void setFanLED(bool state) {
    ledcWrite(PWM_CHANNEL_FAN, state ? 128 : 0); // 50% brightness (128/255)
}

void updateStatusLEDs() {
    // Update LEDs based on STATE FLAGS, not actual relay pin states
    // This prevents flickering and provides stable visual feedback
    // LEDs reflect the intended state (heatingOn/coolingOn/fanOn), not the physical relay pins
    setHeatLED(heatingOn);
    setCoolLED(coolingOn);
    setFanLED(fanOn);
}

// Buzzer control functions using PWM channel for tone generation
void buzzerBeep(int duration) {
    ledcWriteTone(PWM_CHANNEL_BUZZER, 4000); // 4kHz tone
    delay(duration);
    ledcWrite(PWM_CHANNEL_BUZZER, 0); // Turn off
}

void buzzerStartupTone() {
    // Single tone to indicate setup complete
    buzzerBeep(125); // 125ms beep at 4kHz
}
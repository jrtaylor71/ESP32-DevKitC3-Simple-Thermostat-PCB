# Smart Thermostat Alt Firmware - Development Guide

This guide helps developers understand, modify, and extend the Smart Thermostat Alt Firmware project.

## 🎯 Project Architecture Overview

### Current Implementation Status
- **Version**: 1.4.001 (January 2026)
- **Platform**: ESP32-S3-WROOM-1-N16 (16MB Flash, No PSRAM)
- **Display**: ILI9341 320x240 TFT with XPT2046 touch controller
- **Sensors**: AHT20 (I2C temp/humidity), DS18B20 (OneWire hydronic temp), LD2410 (24GHz mmWave motion)
- **Weather**: Dual-source (OpenWeatherMap/Home Assistant) with color-coded standard icons
- **Architecture**: Dual-core FreeRTOS with Option C centralized display management
- **Memory Usage**: ~3.2MB flash (18.5% utilization = 1,210,272 bytes with default_16mb.csv partition)
- **Key Features**: Bidirectional MQTT schedule sync, weather integration, motion wake on presence, I2C mutex protection, anti-flicker display, multi-thermostat support

## 🎯 Development Environment Setup

### Prerequisites
- **VS Code** with PlatformIO extension
- **Git** for version control
- **MQTT Client** for testing (MQTT.fx, MQTT Explorer, etc.)
- **ESP32-S3 Development Board** or custom PCB
- **Serial Monitor** for debugging (115200 baud)

### Initial Setup
```bash
# Clone the repository
git clone <repository-url>
cd Smart-Thermostat-Alt-Firmware

# Open in VS Code
code .

# PlatformIO will automatically:
# - Install ESP32 platform
# - Download required libraries
# - Configure build environment
```

### Hardware Setup for Development
1. **ESP32-S3-WROOM-1-N16**: Main microcontroller with 16MB flash
2. **ILI9341 TFT Display with XPT2046**: 320x240 SPI display with resistive touch
3. **AHT20 Sensor**: I2C temperature/humidity sensor
4. **DS18B20 Sensor**: OneWire water temperature sensor (optional)
5. **LD2410 Motion Sensor**: 24GHz mmWave radar for occupancy detection (pins 15, 16, 18)
6. **5x Relay Module**: For HVAC control simulation
7. **Custom PCB**: Professional PCB design by Stefan Meisner (recommended)
8. **Development Breadboard**: For prototyping connections

## 🏗️ Code Architecture

### Dual-Core FreeRTOS Architecture

The system uses ESP32-S3's dual-core architecture for optimal performance:
- **Core 0**: Main loop, display updates, web server, MQTT
- **Core 1**: Sensor reading task, HVAC control logic

```cpp
// Core 1: Sensor task for responsive control
void sensorTaskFunction(void *parameter) {
    for (;;) {
        // Read sensors every 5 seconds
        sensors_event_t humidity, temp;
        aht.getEvent(&humidity, &temp);
        // Control HVAC relays
        controlRelays(newTemp);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

// Core 0: Centralized display update task (Option C)
void displayUpdateTaskFunction(void* parameter) {
    for (;;) {
        if (displayUpdateRequired) {
            updateDisplayIndicators();
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}
```

### Main Components

#### Core State Management
```cpp
// Global state variables - central to system operation
bool heatingOn = false;
bool coolingOn = false; 
bool fanOn = false;
String thermostatMode = "off";    // "off", "heat", "cool", "auto"
String fanMode = "auto";          // "auto", "on", "cycle"

// Multi-stage control flags
bool stage1Active = false;
bool stage2Active = false;
bool stage2HeatingEnabled = false;
bool stage2CoolingEnabled = false;

// Hydronic heating support
float hydronicTemp = 0.0;
bool hydronicHeatingEnabled = false;
float hydronicTempLow = 110.0;
float hydronicTempHigh = 130.0;
```

#### Settings Persistence Pattern
```cpp
// Save settings to flash memory
void saveSettings() {
    preferences.putFloat("setHeat", setTempHeat);
    preferences.putString("thermoMd", thermostatMode);
    // ... save all configurable parameters
}

// Load settings from flash memory  
void loadSettings() {
    setTempHeat = preferences.getFloat("setHeat", 72.0);  // default: 72.0
    thermostatMode = preferences.getString("thermoMd", "off");  // default: "off"
    // ... load all parameters with defaults
}
```

#### Non-blocking Task Scheduling
```cpp
void loop() {
    static unsigned long lastDisplayUpdate = 0;
    static unsigned long lastSensorRead = 0;
    
    unsigned long currentTime = millis();
    
    // Update display every 1000ms
    if (currentTime - lastDisplayUpdate > 1000) {
        updateDisplay(currentTemp, currentHumidity);
        lastDisplayUpdate = currentTime;
    }
    
    // Read sensors every 2000ms
    if (currentTime - lastSensorRead > 2000) {
        // Read sensors and control relays
        lastSensorRead = currentTime;
    }
}
```

### Key Function Categories

#### 1. Display Functions
```cpp
void updateDisplay(float temp, float humidity);     // Main display update
void drawButtons();                                 // Render touch buttons
void handleButtonPress(uint16_t x, uint16_t y);   // Process touch input
void drawKeyboard(bool uppercase);                 // WiFi setup keyboard
```

#### 2. HVAC Control Functions  
```cpp
void controlRelays(float currentTemp);             // Main control logic
void activateHeating();                           // Multi-stage heating
void activateCooling();                           // Multi-stage cooling
void handleFanControl();                          // Fan operation
void controlFanSchedule();                        // Scheduled fan cycling
```

#### 3. Communication Functions
```cpp
void setupMQTT();                                 // MQTT initialization
void mqttCallback(char* topic, byte* payload, unsigned int length);
void sendMQTTData();                              // Publish sensor data
void handleWebRequests();                        // Web server setup
```

## 🔧 Common Development Tasks

### Adding New Configuration Settings

1. **Add Global Variable**
```cpp
// Add near other global variables
bool newFeatureEnabled = false;
float newParameterValue = 10.0;
```

2. **Update Settings Functions**
```cpp
void saveSettings() {
    // Add to existing function
    preferences.putBool("newFeat", newFeatureEnabled);
    preferences.putFloat("newParam", newParameterValue);
}

void loadSettings() {
    // Add to existing function  
    newFeatureEnabled = preferences.getBool("newFeat", false);  // default: false
    newParameterValue = preferences.getFloat("newParam", 10.0); // default: 10.0
}

void restoreDefaultSettings() {
    // Add to existing function
    newFeatureEnabled = false;
    newParameterValue = 10.0;
}
```

3. **Add Web Interface Support**
```cpp
// In handleWebRequests() function, add to settings page HTML:
html += "New Feature: <input type='checkbox' name='newFeature' " + 
        String(newFeatureEnabled ? "checked" : "") + "><br>";
html += "New Parameter: <input type='text' name='newParam' value='" + 
        String(newParameterValue) + "'><br>";

// In /set POST handler, add parameter processing:
if (request->hasParam("newFeature", true)) {
    newFeatureEnabled = request->getParam("newFeature", true)->value() == "on";
} else {
    newFeatureEnabled = false;
}
if (request->hasParam("newParam", true)) {
    newParameterValue = request->getParam("newParam", true)->value().toFloat();
}
```

### Extending MQTT Functionality

1. **Add New Status Topics**
```cpp
void sendMQTTData() {
    static bool lastFeatureState = false;
    
    // Add to existing function
    if (newFeatureEnabled != lastFeatureState) {
        mqttClient.publish("esp32_thermostat/new_feature", 
                          String(newFeatureEnabled).c_str(), true);
        lastFeatureState = newFeatureEnabled;
    }
}
```

2. **Add Command Topics**
```cpp
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    // Add to existing function
    if (String(topic) == "esp32_thermostat/new_feature/set") {
        newFeatureEnabled = (message == "true" || message == "1");
        saveSettings();
    }
}

void reconnectMQTT() {
    if (mqttClient.connect(hostname.c_str(), mqttUsername.c_str(), mqttPassword.c_str())) {
        // Add to existing subscriptions
        mqttClient.subscribe("esp32_thermostat/new_feature/set");
    }
}
```

3. **Add Home Assistant Discovery**
```cpp
void publishHomeAssistantDiscovery() {
    // Add new sensor discovery
    String configTopic = "homeassistant/binary_sensor/esp32_thermostat_new_feature/config";
    StaticJsonDocument<512> doc;
    doc["name"] = "Thermostat New Feature";
    doc["unique_id"] = "esp32_thermostat_new_feature";
    doc["state_topic"] = "esp32_thermostat/new_feature";
    
    JsonObject device = doc.createNestedObject("device");
    device["identifiers"] = "esp32_thermostat";
    device["name"] = hostname;
    
    char buffer[512];
    serializeJson(doc, buffer);
    mqttClient.publish(configTopic.c_str(), buffer, true);
}
```

### MQTT 7-Day Schedule Architecture 🔄

The thermostat implements a complete bidirectional MQTT schedule sync system with Home Assistant.

#### Firmware-Side Implementation

**Schedule Data Structure** (in Main-Thermostat.cpp):
```cpp
struct SchedulePeriod {
    int hour;        // 0-23
    int minute;      // 0-59
    float heatTemp;  // Target heating temperature
    float coolTemp;  // Target cooling temperature
    float autoTemp;  // Target auto mode temperature
    bool active;     // Whether this period is enabled
};

struct DaySchedule {
    SchedulePeriod day;    // Day period (default 6:00 AM)
    SchedulePeriod night;  // Night period (default 10:00 PM)
    bool enabled;          // Whether scheduling is enabled for this day
};

// Array indexed: 0=Sunday, 1=Monday, ..., 6=Saturday
DaySchedule weekSchedule[7];
```

**Inbound MQTT Handler** (schedule set topic):
```cpp
// Topic: {hostname}/schedule/set
// Format: {"day": 0-6, "period": "day"|"night", "hour": 0-23, "minute": 0-59, 
//          "heat": ##, "cool": ##, "auto": ##, "active": true|false}
// Note: MQTT uses 0=Monday through 6=Sunday, converted to array index (0=Sunday)

void handleScheduleSetMessage() {
    int mqttDay = doc["day"];  // 0=Monday in MQTT protocol
    int day = (mqttDay + 1) % 7;  // Convert to array index (0=Sunday)
    String period = doc["period"];  // "day" or "night"
    
    SchedulePeriod* target = (period == "day") ? 
        &weekSchedule[day].day : &weekSchedule[day].night;
    
    // Update fields that are present in payload
    if (doc.containsKey("hour")) target->hour = doc["hour"];
    if (doc.containsKey("minute")) target->minute = doc["minute"];
    if (doc.containsKey("heat")) target->heatTemp = doc["heat"];
    if (doc.containsKey("cool")) target->coolTemp = doc["cool"];
    if (doc.containsKey("auto")) target->autoTemp = doc["auto"];
    if (doc.containsKey("active")) target->active = doc["active"];
    if (doc.containsKey("enabled")) weekSchedule[day].enabled = doc["enabled"];
    
    scheduleNeedsSaving = true;
    if (changed) sendMQTTData();  // Republish updated state
}
```

**Outbound MQTT Publisher** (schedule state topics):
```cpp
// Topics: {hostname}/schedule/sunday through /schedule/saturday
// Publishes complete schedule for each day

void publishScheduleState() {
    const char* dayNames[7] = {"Sunday", "Monday", "Tuesday", ...};
    
    for (int day = 0; day < 7; day++) {
        StaticJsonDocument<512> schedDoc;
        schedDoc["day_index"] = (day - 1 + 7) % 7;  // Convert to MQTT format (0=Monday)
        schedDoc["day_name"] = dayNames[day];
        schedDoc["day_enabled"] = weekSchedule[day].enabled;
        
        JsonObject dayPeriod = schedDoc.createNestedObject("day_period");
        dayPeriod["time"] = formatTime(weekSchedule[day].day);
        dayPeriod["heat"] = weekSchedule[day].day.heatTemp;
        dayPeriod["cool"] = weekSchedule[day].day.coolTemp;
        dayPeriod["auto"] = weekSchedule[day].day.autoTemp;
        dayPeriod["active"] = weekSchedule[day].day.active;
        
        JsonObject nightPeriod = schedDoc.createNestedObject("night_period");
        // ... similar for night period ...
        
        mqttClient.publish(topic.c_str(), buffer, false);
    }
}
```

**Key Implementation Details**:
- Day index conversion: MQTT (0=Monday) ↔ Array (0=Sunday)
- Inbound: Convert `(mqttDay + 1) % 7` when receiving
- Outbound: Convert `(arrayDay - 1 + 7) % 7` when publishing
- All temps sent as floats (72.0, 74.5, etc.)
- Boolean values rendered as lowercase JSON: `"active": true` (not `True`)
- Time format: "HH:MM" string (e.g., "6:00", "22:30")

#### Home Assistant Side

**Multi-Thermostat Centralized Automation** (`multi_thermostat_schedule_sync.yaml`):
```yaml
# Handles inbound schedule updates from ALL thermostats
trigger:
  platform: mqtt
  topic: "+/schedule/+"  # Wildcard captures all devices and days

variables:
  host_key: "{{ trigger.topic.split('/')[0] | lower | replace('-', '_') }}"
  day_slug: "{{ trigger.topic.split('/')[2] }}"
  payload: "{{ trigger.payload_json }}"

condition:
  - condition: template
    value_template: |
      {%- set valid_days = ['monday','tuesday','wednesday','thursday','friday','saturday','sunday'] -%}
      {%- set valid_hosts = ['shop_thermostat', 'studio_thermostat', 'house_thermostat'] -%}
      {{ day_slug | lower in valid_days and host_key in valid_hosts }}

action:
  # Updates 11 helpers per day (2 times, 6 temps, 3 enabled flags)
  - service: input_datetime.set_datetime
    target:
      entity_id: "input_datetime.{{ host_key }}_{{ day_slug }}_day_time"
    data:
      time: "{{ '%02d:%02d:00' | format(payload.day_period.time.split(':')[0] | int, ...) }}"
  # ... 10 more services for temps and enabled status ...
```

**Per-Thermostat Outbound Automations** (generated via script):
```yaml
# Triggered when any schedule helper changes
# Total: 77 automations per thermostat (14 times × 5 params + 7 enabled flags)

- alias: "Thermostat - Friday Morning Time Changed"
  trigger:
    platform: state
    entity_id: input_datetime.shop_thermostat_friday_day_time
  action:
    service: mqtt.publish
    data:
      topic: "Shop-Thermostat/schedule/set"
      payload: |
        {
          "day": 4,
          "period": "day",
          "hour": {{ states('input_datetime.shop_thermostat_friday_day_time').split(':')[0] | int }},
          "minute": {{ states('input_datetime.shop_thermostat_friday_day_time').split(':')[1] | int }}
        }
```

**Generation Script** (`generate_schedule_package.sh`):
```bash
# Generates all 77 outbound automations and 77 helpers for a device
./generate_schedule_package.sh shop_thermostat

# Creates:
# - shop_thermostat_schedule.yaml (77 automations + 77 helpers)
# - shop_thermostat_schedule_card.yaml (dashboard card UI)

# Hostname conversion: shop_thermostat → Shop-Thermostat for MQTT topics
# Helper IDs: lowercase_with_underscores
# MQTT Topics: CamelCase-With-Hyphens
```

**Total System Overhead**:
- 2 thermostats: 154 helpers, 155 automations
- 3 thermostats: 231 helpers, 232 automations (1 central + 77×3)
- Scales linearly with device count
- All generated automatically - zero manual configuration

### Adding Display Elements

1. **Update Main Display**
```cpp
void updateDisplay(float currentTemp, float currentHumidity) {
    // Add after existing display updates
    static bool prevNewFeatureState = false;
    
    if (newFeatureEnabled != prevNewFeatureState) {
        // Clear area and update display
        tft.fillRect(10, 140, 100, 20, TFT_BLACK);
        
        if (newFeatureEnabled) {
            tft.fillRoundRect(10, 140, 100, 20, 5, TFT_GREEN);
            tft.setTextColor(TFT_WHITE);
            tft.setCursor(15, 145);
            tft.print("NEW FEATURE");
        }
        
        prevNewFeatureState = newFeatureEnabled;
    }
}
```

2. **Add Touch Button**
```cpp
void drawButtons() {
    // Add after existing buttons
    tft.fillRect(270, 100, 40, 30, TFT_PURPLE);
    tft.setCursor(275, 110);
    tft.setTextColor(TFT_WHITE);
    tft.print("NEW");
}

void handleButtonPress(uint16_t x, uint16_t y) {
    // Add button detection
    if (x > 270 && x < 310 && y > 100 && y < 130) {
        newFeatureEnabled = !newFeatureEnabled;
        saveSettings();
        sendMQTTData();
        updateDisplay(currentTemp, currentHumidity);
    }
}
```

## 🐛 Debugging Techniques

### Serial Debug Output
```cpp
// Add debug messages throughout code
Serial.println("Debug: Function started");
Serial.printf("Debug: Variable value = %.2f\n", someVariable);
Serial.print("Debug: String value = "); Serial.println(someString);

// Monitor at 115200 baud rate
// PlatformIO: Ctrl+Alt+S or use Serial Monitor
```

### Web Interface Debugging
```cpp
// Add debug endpoint to web server
server.on("/debug", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<html><body><h1>Debug Info</h1>";
    html += "<p>Free Heap: " + String(ESP.getFreeHeap()) + "</p>";
    html += "<p>Uptime: " + String(millis() / 1000) + " seconds</p>";
    html += "<p>WiFi RSSI: " + String(WiFi.RSSI()) + " dBm</p>";
    html += "<p>Current Mode: " + thermostatMode + "</p>";
    html += "</body></html>";
    request->send(200, "text/html", html);
});
```

### MQTT Debug Messages
```cpp
void sendDebugMQTT(String message) {
    if (mqttClient.connected()) {
        mqttClient.publish("esp32_thermostat/debug", message.c_str());
    }
}

// Usage:
sendDebugMQTT("Heating activated: temp=" + String(currentTemp));
```

## 🧪 Testing Strategies

### Unit Testing Approach
```cpp
// Create test functions for key logic
void testTemperatureLogic() {
    Serial.println("Testing temperature control logic...");
    
    // Test heating activation
    thermostatMode = "heat";
    setTempHeat = 72.0;
    controlRelays(70.0);  // Should activate heating
    
    if (heatingOn) {
        Serial.println("✓ Heating activation test passed");
    } else {
        Serial.println("✗ Heating activation test failed");
    }
}

// Call from setup() during development
void setup() {
    // ... normal setup code ...
    
    #ifdef DEBUG_MODE
    testTemperatureLogic();
    #endif
}
```

### Integration Testing
1. **MQTT Testing**: Use MQTT client to send commands and verify responses
2. **Web Interface Testing**: Test all web endpoints with various parameters
3. **Hardware Testing**: Verify relay outputs with multimeter or LED indicators
4. **Touch Testing**: Verify all buttons respond correctly

### Load Testing
```cpp
// Test system under continuous operation
void stressTest() {
    static unsigned long testStartTime = millis();
    static int testCycles = 0;
    
    // Simulate rapid temperature changes
    if (millis() - testStartTime > 1000) {
        testCycles++;
        float testTemp = 70.0 + (testCycles % 10);
        controlRelays(testTemp);
        testStartTime = millis();
        
        if (testCycles > 100) {
            Serial.println("Stress test completed successfully");
            testCycles = 0;
        }
    }
}
```

## 🔐 Best Practices

### Memory Management
```cpp
// Use static buffers to avoid fragmentation
void handleLargeData() {
    static char buffer[1024];  // Reuse buffer
    // ... process data ...
}

// Check heap regularly
void checkMemory() {
    if (ESP.getFreeHeap() < 10000) {
        Serial.println("Warning: Low memory!");
    }
}
```

### Error Handling
```cpp
// Always validate sensor readings
float readTemperature() {
    float temp = dht.readTemperature(useFahrenheit);
    if (isnan(temp)) {
        Serial.println("Error: Invalid temperature reading");
        return previousTemp;  // Use last known good value
    }
    return temp;
}

// Handle WiFi disconnections gracefully
void checkWiFiStatus() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected, entering offline mode");
        // Continue operation without network features
    }
}
```

### Code Organization
```cpp
// Group related constants
namespace ThermostatConstants {
    const float MIN_TEMP = 50.0;
    const float MAX_TEMP = 95.0;
    const unsigned long DISPLAY_UPDATE_INTERVAL = 1000;
    const unsigned long SENSOR_READ_INTERVAL = 2000;
}

// Use descriptive function names
void activateFirstStageHeating();
void checkForSecondStageRequirement();
void updateTemperatureDisplay();
```

## 🚀 Performance Optimization

### Display Updates
```cpp
// Only update changed elements
void updateDisplay(float temp, float humidity) {
    // Check if values actually changed before updating
    if (abs(temp - previousTemp) > 0.1) {
        updateTemperatureDisplay(temp);
        previousTemp = temp;
    }
}
```

### MQTT Optimization
```cpp
// Batch MQTT publishes to reduce network traffic
void sendMQTTBatch() {
    static unsigned long lastBatch = 0;
    
    if (millis() - lastBatch > 5000) {  // Every 5 seconds
        // Send all status updates at once
        sendMQTTData();
        lastBatch = millis();
    }
}
```

### Memory Efficiency
```cpp
// Use PROGMEM for large string constants
const char webPageTemplate[] PROGMEM = R"(
<html>
<head><title>Thermostat</title></head>
<body>%s</body>
</html>
)";
```

## 📦 Packaging and Distribution

### Firmware Preparation
```bash
# Build release version
pio run -e esp32dev

# Create firmware package
mkdir release
cp .pio/build/esp32dev/firmware.bin release/
cp README.md DOCUMENTATION.md release/
```

### PCB Manufacturing Package
The `jlcpcb/` folder contains complete manufacturing files:
- **Gerber files**: Ready for PCB fabrication
- **BOM**: Component sourcing list
- **CPL**: Pick-and-place assembly file

### Version Management
```cpp
// Update version in code
String sw_version = "1.2.2";  // Increment for releases

// Update in documentation
// Update in README.md
// Tag git repository: git tag v1.2.2
```

## 🔍 Recent Development Sessions

### Session: November 29, 2025 - Motion Wake & I2C Crash Fix (v1.2.2)

**Problem**: System crashes after 5-10 minutes of operation, motion wake not functioning properly.

**Root Causes Identified**:
1. **LD2410 radar concurrent access**: Both `checkDisplaySleep()` and `readMotionSensor()` calling `radar.check()` caused library internal buffer corruption
2. **I2C bus contention**: AHT20 sensor task and display operations accessing I2C bus simultaneously without synchronization
3. **Motion wake logic issues**: 
   - State-change wake triggering on any moving target without filtering
   - Variable scope bugs causing broken motion tracking
   - False positives from environmental noise (furniture, walls)

**Solutions Implemented**:

1. **Radar Access Protection**:
   - Removed `radar.check()` from `checkDisplaySleep()` - only called in `readMotionSensor()` now
   - Added `radarDataTimestamp` to track data freshness (500ms max age)
   - `checkDisplaySleep()` now reads cached sensor data with brief mutex locks

2. **I2C Mutex Protection**:
   ```cpp
   SemaphoreHandle_t i2cMutex = NULL;  // Protects I2C bus access
   
   // In sensorTaskFunction():
   if (xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
       if (aht.getEvent(&humidity, &temp)) {
           // Process sensor data
       }
       xSemaphoreGive(i2cMutex);
   }
   ```
   
3. **AHT20 Error Handling**:
   - Wrapped sensor reads with error detection
   - Added automatic reinitialization on failure (30s cooldown)
   - Continues operation if sensor read fails

4. **Motion Wake Improvements**:
   - **Sustained motion tracking**: 2-second debounce filters brief environmental blips
   - **Distance filtering**: Only wake on motion within 100cm (close range)
   - **Signal strength filtering**: Range 50-100 to reject weak noise and stationary targets
   - **Data freshness validation**: Skip processing stale radar data
   - **Dual-path filtering**: Applied filters to both state-change wake AND sustained motion wake
   - **Variable scope fix**: Moved `firstMotionTime` and `lastFilterLog` to function scope

5. **Motion Wake Constants** (lines ~115-127):
   ```cpp
   const unsigned long MOTION_WAKE_COOLDOWN = 5000;     // 5s after sleep
   const unsigned long MOTION_WAKE_DEBOUNCE = 2000;     // 2s sustained motion
   const int MOTION_WAKE_MAX_DISTANCE = 100;            // 100cm max range
   const unsigned long RADAR_DATA_MAX_AGE = 500;        // 500ms data freshness
   const int MOTION_WAKE_MIN_SIGNAL = 50;               // Min signal strength
   const int MOTION_WAKE_MAX_SIGNAL = 100;              // Max signal strength
   ```

**Testing Results**:
- Motion wake working correctly with hand wave
- False positives eliminated (stationary targets filtered)
- Display sleep/wake cycle functioning properly
- System stable with I2C mutex protection

**Files Modified**:
- `src/Main-Thermostat.cpp`: Added i2cMutex, improved sensor error handling, motion wake filtering

**Lessons Learned**:
1. **Thread safety critical**: Concurrent hardware access requires mutex protection even with FreeRTOS tasks
2. **Library limitations**: LD2410 library cannot handle concurrent `check()` calls - must be called from single location
3. **Data freshness matters**: Cached sensor data needs timestamp validation
4. **Variable scope awareness**: Static variables in nested blocks create separate instances
5. **Filter all paths**: Motion detection with multiple trigger paths needs consistent filtering
6. **Sustained vs instant**: Environmental noise requires debounce filtering for reliable motion detection

**Known Issues**:
- None currently - system stable with all fixes applied

### Session: November 30, 2025 - Enhanced OTA Update System (v1.3.0)

**Objective**: Improve OTA update user experience with real-time progress tracking and proper status messaging.

**Challenges**:
1. No visual feedback during firmware upload
2. Page timeout before device finished rebooting
3. Connection error messages despite successful updates
4. No visibility into flash write progress

**Solutions Implemented**:

1. **Real-Time Upload Progress**:
   - XHR-based upload with `onprogress` event tracking
   - Progress bar showing bytes transferred and ETA
   - Transfer speed calculation and display

2. **Flash Write Progress Tracking**:
   - Server-side tracking: `otaBytesWritten`, `otaTotalSize` global variables
   - Update progress during `Update.write()` operations
   - `/update_status` JSON endpoint for client polling (800ms intervals)
   - 1-second logging intervals showing flash write percentage

3. **Response Timing Optimization**:
   - Send HTTP 200 response immediately with `Connection: close` header
   - 1500ms delay before `ESP.restart()` to ensure response transmission
   - Client-side 3-second wait after flash complete before polling
   - 70-second polling window for device reboot and startup

4. **Status Message Flow**:
   - "Uploading firmware: X remaining"
   - "Upload complete, writing to flash..."
   - "Flash complete. Device rebooting..."
   - "Waiting for reboot and startup (up to 15s)..."
   - "✓ Update successful! Version X.X.X"

5. **Code Cleanup**:
   - Removed redundant standalone `/update` GET endpoint (~70 lines)
   - Integrated all OTA functionality into System tab
   - Reduced code duplication and maintenance burden

**Technical Details**:
```cpp
// Global OTA tracking variables
volatile size_t otaBytesWritten = 0;
volatile size_t otaTotalSize = 0;
volatile bool otaInProgress = false;
volatile bool otaRebooting = false;

// Upload handler updates
if (index == 0) {
    otaTotalSize = request->contentLength();
    otaInProgress = true;
}

// Track flash write progress
otaBytesWritten = index + len;
if (currentTime - otaLastUpdateLog > 1000) {
    Serial.printf("Flash write progress: %.1f%%\n", 
                  (otaBytesWritten * 100.0) / otaTotalSize);
}

// Response timing
AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
response->addHeader("Connection", "close");
request->send(response);
delay(1500);  // Allow response transmission
ESP.restart();
```

**Files Modified**:
- `src/Main-Thermostat.cpp`: Added OTA tracking, improved upload handler, removed standalone page
- `include/WebPages.h`: Enhanced System tab with progress tracking UI

**Testing Results**:
- Upload progress bar shows accurate real-time progress
- Flash write progress updates every second
- No connection error messages - clean reboot handling
- Version verification confirms successful update
- Reduced flash usage by ~2KB with code cleanup

**Lessons Learned**:
1. **Browser XHR behavior**: Connection drops on reboot cause errors even after successful response - must send response before restart
2. **Timing is critical**: Device needs adequate time (3-5s) for reboot and initialization before polling
3. **Progress visibility**: Users need feedback during both upload and flash write phases
4. **Code organization**: Integrated features in tabs provide better UX than standalone pages

This development guide provides the foundation for extending and maintaining the Smart Thermostat Alt Firmware project. Follow these patterns and practices to ensure code quality and system reliability.
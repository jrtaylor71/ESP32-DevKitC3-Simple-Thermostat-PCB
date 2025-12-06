# Firmware - Complete Documentation

## Project Overview

The Firmware is a comprehensive, feature-rich smart thermostat system built on the ESP32-S3 platform with professional PCB design.

**Current Version**: 1.3.5 (December 2025)
**Hardware**: ESP32-S3-WROOM-1-N16 (16MB Flash)
**Architecture**: Dual-core FreeRTOS with Option C display management
**Status**: Production-ready with comprehensive testing, scheduling, weather integration, and enhanced OTA updates

### Key Features

- **Professional Dual-Core Architecture**: ESP32-S3 FreeRTOS with Core 0 for UI/networking, Core 1 for control
- **Weather Integration**: Dual-source weather (OpenWeatherMap/Home Assistant) with color-coded standard icons
- **7-Day Scheduling System**: Complete inline scheduling with day/night periods, editable Heat/Cool/Auto temperatures, and MQTT integration
- **Modern Tabbed Web Interface**: All features embedded in main page - no separate pages, always-visible options
- **Local Touch Control**: ILI9341 TFT LCD with resistive touch and adaptive brightness
- **Smart Home Integration**: Full MQTT support with Home Assistant auto-discovery, precision temperature control, and schedule status publishing
- **Advanced Sensor Suite**: AHT20 I2C sensor for ambient conditions, DS18B20 OneWire for hydronic systems
- **Intelligent Multi-Stage HVAC**: 2-stage heating/cooling with hybrid time/temperature staging logic
- **Enhanced Hydronic Safety**: Water temperature monitoring with [LOCKOUT] safety interlocks and improved alerts
- **Flexible Operation Modes**: Heat, Cool, Auto, and Off with configurable temperature swings
- **Advanced Fan Control**: Auto, On, and Cycle modes with scheduled operation (minutes per hour)
- **Anti-Flicker Display**: Cached redraw optimization eliminates time and weather flicker
- **Comprehensive Web Interface**: Real-time monitoring and complete configuration in tabbed layout
- **Robust Offline Operation**: Full functionality without WiFi connection
- **Custom PCB Design**: Professional PCB by Stefan Meisner for clean installation
- **Modern UI**: Material Design color scheme with responsive touch interface
- **Factory Reset**: 10-second boot button press for complete settings reset
- **OTA Updates**: Over-the-air firmware updates with real-time upload and flash write progress tracking

## Hardware Components

### Main Components
- **ESP32-S3-WROOM-1-N16**: Main microcontroller (16MB Flash, No PSRAM)
- **ILI9341 TFT LCD with XPT2046**: 320x240 pixel display with resistive touch controller
- **AHT20 Sensor**: I2C temperature and humidity measurement  
- **DS18B20 Sensor**: OneWire water/hydronic temperature measurement (optional)
- **LD2410 Motion Sensor**: 24GHz mmWave radar for occupancy detection and display wake
- **5x Relay Outputs**: Control for heating, cooling, and fan systems

### Pin Configuration (ESP32-S3)

#### TFT Display Connections (ILI9341 + XPT2046)
```cpp
// Current platformio.ini build_flags configuration:
#define TFT_MISO 21    // SPI MISO (shared with touch)
#define TFT_MOSI 12    // SPI MOSI (shared with touch)
#define TFT_SCLK 13    // SPI Clock (shared with touch)  
#define TFT_CS    9    // TFT Chip Select
#define TFT_DC   11    // Data/Command
#define TFT_RST  10    // Reset
#define TFT_BL   14    // Backlight (PWM controlled)
#define TOUCH_CS 47    // Touch Chip Select
#define TOUCH_IRQ 48   // Touch Interrupt
```

#### Sensor Connections
```cpp
// AHT20 I2C sensor (ESP32-S3 pins)
#define AHT20_SDA 36        // I2C SDA for AHT20
#define AHT20_SCL 35        // I2C SCL for AHT20
// DS18B20 OneWire sensor
#define ONE_WIRE_BUS 34     // DS18B20 data pin
// Light sensor for adaptive brightness
#define LIGHT_SENSOR_PIN 8  // Photocell/light sensor
```

#### Relay Outputs (ESP32-S3 GPIO)
```cpp
// GPIO pins verified for ESP32-S3 schematic
const int heatRelay1Pin = 5;    // Heat Stage 1
const int heatRelay2Pin = 7;    // Heat Stage 2  
const int coolRelay1Pin = 6;    // Cool Stage 1
const int coolRelay2Pin = 39;   // Cool Stage 2
const int fanRelayPin = 4;      // Fan Control
```

#### Status LEDs and Buzzer
```cpp
// PWM-controlled status indicators
const int ledFanPin = 37;       // Fan status LED
const int ledHeatPin = 38;      // Heat status LED
const int ledCoolPin = 2;       // Cool status LED
const int buzzerPin = 17;       // Buzzer (5V through 2N7002 MOSFET)
```

#### Control Input
```cpp
#define BOOT_BUTTON 0       // Factory reset button
```

#### Motion Sensor (LD2410) - Optional
```cpp
#define LD2410_RX_PIN 15    // ESP32 RX (connect to LD2410 TX)
#define LD2410_TX_PIN 16    // ESP32 TX (connect to LD2410 RX) 
#define LD2410_MOTION_PIN 18 // Digital motion output pin
```

## Software Architecture

### Core Libraries
- **WiFi**: Network connectivity
- **ESPAsyncWebServer**: Web interface
- **PubSubClient**: MQTT communication
- **TFT_eSPI**: Display control
- **DHT sensor library**: Temperature/humidity reading
- **DallasTemperature**: DS18B20 sensor support
- **ArduinoJson**: JSON parsing for MQTT and weather APIs
- **HTTPClient**: Weather API requests (OpenWeatherMap)
- **Preferences**: Non-volatile settings storage
- **Weather Module**: Custom weather.h/weather.cpp for dual-source weather integration

### Main Functions

#### Display Management
- `updateDisplay()`: Updates LCD with current status
- `drawButtons()`: Renders touch interface buttons
- `handleButtonPress()`: Processes touch input
- `drawKeyboard()`: WiFi setup keyboard interface
- `wakeDisplay()`: Wake display from sleep on touch/motion
- `checkDisplaySleep()`: Automatic display sleep management

#### Motion Detection (LD2410)
- `testLD2410Connection()`: Verify motion sensor connectivity with robust detection
- `readMotionSensor()`: Read motion status, wake display, and publish MQTT status
- **Auto-Wake Display**: Display automatically wakes on motion detection
- **Robust Connection Logic**: Handles sensors that don't respond to UART commands
- **MQTT Integration**: Motion status published to Home Assistant with auto-discovery
- **Display Sleep Prevention**: Motion detection prevents display sleep timeout

#### HVAC Control
- `controlRelays()`: Main thermostat logic controller
- `activateHeating()`: Multi-stage heating control
- `activateCooling()`: Multi-stage cooling control
- `handleFanControl()`: Fan operation management
- `controlFanSchedule()`: Scheduled fan cycling

#### Communication
- `setupMQTT()`: MQTT client initialization
- `mqttCallback()`: Handles incoming MQTT commands
- `sendMQTTData()`: Publishes sensor data and status
- `publishHomeAssistantDiscovery()`: Auto-discovery setup
- `handleWebRequests()`: Web interface handlers

#### Configuration
- `saveSettings()`: Persistent storage of configuration
- `loadSettings()`: Startup configuration loading
- `restoreDefaultSettings()`: Factory reset functionality

## Operating Modes

### Thermostat Modes
1. **Off**: All HVAC equipment disabled (fan can still operate)
2. **Heat**: Heating-only operation with configurable setpoint
3. **Cool**: Cooling-only operation with configurable setpoint  
4. **Auto**: Automatic changeover between heating and cooling

### Fan Modes
1. **Auto**: Fan runs only when heating/cooling is active
2. **On**: Fan runs continuously
3. **Cycle**: Fan runs for specified minutes per hour

### Advanced Features

#### Multi-Stage Operation
- **Stage 1 Priority**: Always starts with single-stage operation
- **Time-Based Staging**: Stage 2 activates after minimum runtime
- **Temperature-Based Staging**: Stage 2 activates based on temperature differential
- **Configurable Parameters**: Minimum runtime and temperature delta settings

#### Hydronic Heating Support
- **Water Temperature Monitoring**: DS18B20 sensor integration
- **Safety Interlocks**: Prevents operation when water temperature is too low
- **Configurable Thresholds**: High/low temperature setpoints

#### Temperature Control Logic
- **Hysteresis Control**: Prevents short cycling
- **Configurable Swing**: Adjustable temperature deadband
- **Auto Mode Logic**: Separate swing settings for auto changeover

## Web Interface

### Tabbed Interface Design
Modern single-page application with embedded tabs:

#### Status Tab
- Real-time temperature and humidity display
- Current relay states and system status
- Thermostat and fan mode indicators
- Hydronic temperature (if enabled)
- Auto-refresh every 10 seconds

#### Settings Tab
Comprehensive configuration including:
- Temperature setpoints for all modes
- Temperature swing settings
- Multi-stage configuration
- Hydronic heating parameters
- MQTT settings
- WiFi credentials
- Time zone and clock format
- Fan scheduling parameters

#### Weather Tab (New in v1.3.5)
Comprehensive weather configuration:
- **Weather Source Selection**: Choose between OpenWeatherMap, Home Assistant, or Disabled
- **OpenWeatherMap Settings**: API key, city, state (for US disambiguation), country code
- **Home Assistant Settings**: Server URL, bearer token, weather entity ID
- **Update Interval**: Configurable 5-60 minute refresh intervals
- **AJAX Form Submission**: Settings saved without page redirect
- **Conditional Sections**: Only show relevant settings for selected weather source

#### Schedule Tab (New in v1.1.0)
Complete 7-day scheduling system:
- **Weekly Schedule Table**: 7 days with day/night periods
- **Editable Temperatures**: Independent Heat, Cool, and Auto temperatures for each period
- **Time Controls**: Set day and night period start times
- **Per-Day Enable**: Individual day enable/disable toggles
- **Master Controls**: Schedule enable/disable and override options
- **Real-Time Status**: Current schedule state and active period display
- **Always Visible**: All options permanently displayed - no hidden menus
- **MQTT Integration**: Schedule status published to Home Assistant
- **Persistent Storage**: All schedule settings saved to preferences

#### System Tab
- Device information and system status
- Firmware version and hardware details
- OTA firmware update interface with real-time progress tracking
  - Upload progress bar with transfer speed and ETA
  - Flash write progress with percentage indication
  - Automatic reboot handling with status messages
  - Version verification after successful update
- System restart controls

### API Endpoints
- `/`: Main interface with embedded tabs
- `/schedule_set`: Schedule configuration processing (POST)
- `/status`: JSON API for current status
- `/control`: JSON API for remote control
- `/update`: OTA firmware update endpoint (POST)
- `/update_status`: Real-time OTA progress JSON endpoint
- `/version`: Firmware version JSON endpoint
- `/reboot`: System restart endpoint

## Weather Integration (v1.3.5)

### Weather Module Architecture

The weather system is implemented as a separate modular component:
- **include/Weather.h**: Interface definition with WeatherSource enum and WeatherData struct
- **src/Weather.cpp**: Implementation with dual API support and display rendering

### Weather Data Structure
```cpp
enum WeatherSource {
    WEATHER_DISABLED = 0,
    WEATHER_OPENWEATHERMAP = 1,
    WEATHER_HOMEASSISTANT = 2
};

struct WeatherData {
    float temperature;        // Current temperature
    float tempHigh;          // Daily high
    float tempLow;           // Daily low
    String condition;        // Short condition (e.g., "Clear")
    String description;      // Full description
    int humidity;            // Humidity percentage
    float windSpeed;         // Wind speed
    String iconCode;         // OWM standard icon code (01d-50n)
    bool valid;              // Data validity flag
    unsigned long lastUpdate; // Update timestamp
};
```

### Supported Weather Sources

#### OpenWeatherMap (Free Tier)
- **API Endpoint**: Current Weather Data API
- **Configuration**: API key, city, state (optional for US), country code
- **Features**: Standard OWM icon codes, temperature, conditions, humidity, wind speed
- **Update Mechanism**: HTTP GET with JSON parsing, URL-encoded city names
- **Icon Mapping**: 01=clear, 02=few clouds, 03/04=clouds, 09=shower, 10=rain, 11=storm, 13=snow, 50=mist

#### Home Assistant
- **API Endpoint**: REST API with entity state queries
- **Configuration**: Server URL, bearer token, weather entity ID
- **Features**: Direct integration with HA weather entities
- **Authentication**: Bearer token in Authorization header

### Display Integration

#### Weather Display Layout
- **Position**: Top-left at (5, 25) below time header
- **Temperature**: Large display at (x+10, y+10)
- **Weather Icon**: 36x36 pixels at (x+110, y) with color-coded rendering
- **High/Low**: Daily range at (x+10, y+30)

#### Color-Coded Weather Icons
Standard OpenWeatherMap icon codes with visual color mapping:
- **01 (Clear)**: Orange sun (0xFD20)
- **02 (Few Clouds)**: Gray clouds with sun (0xCE79)
- **03/04 (Clouds)**: Gray clouds (0xCE79)
- **09 (Shower)**: Blue rain drops (0x3D5F)
- **10 (Rain)**: Blue rain with cloud (0x3D5F)
- **11 (Thunderstorm)**: Yellow lightning (0xFC00)
- **13 (Snow)**: White snowflakes (0xFFFF)
- **50 (Mist)**: Gray fog lines (0xAD55)

#### Anti-Flicker Optimization
Weather display uses cached redraw to eliminate flicker:
- **Cache Variables**: `prevLastUpdate`, `prevUnitsF`, `prevX`, `prevY`
- **Redraw Trigger**: Only updates when weather data timestamp changes
- **Benefits**: Smooth display, reduced TFT bus traffic, better responsiveness

### Weather Update Logic

#### Forced First Update
- `_forceNextUpdate` flag set to `true` in constructor
- Bypasses interval check on first update attempt
- Ensures weather displays immediately after configuration

#### Interval-Based Updates
- Configurable 5-60 minute intervals
- Respects WiFi connectivity status
- Automatic retry on API failures with error logging

### Web Interface

#### Weather Tab Features
- **Weather Source Dropdown**: DISABLED / OpenWeatherMap / Home Assistant
- **Dynamic Sections**: JavaScript toggles visibility based on source selection
- **OpenWeatherMap Section**: 3-column grid layout (city 2fr, state 1fr, country 1fr)
- **AJAX Submission**: Form posts to `/set` endpoint without page redirect
- **Settings Persistence**: All weather settings saved to ESP32 preferences

### Settings Storage

Weather settings are stored in ESP32 NVS preferences:
- `weatherSrc`: Weather source (0=disabled, 1=OWM, 2=HA)
- `owmApiKey`: OpenWeatherMap API key
- `owmCity`: City name (URL-encoded for API calls)
- `owmState`: State/province (optional, for US city disambiguation)
- `owmCountry`: ISO country code (e.g., "US", "CA")
- `haUrl`: Home Assistant server URL
- `haToken`: HA long-lived access token
- `haEntityId`: HA weather entity ID
- `weatherInt`: Update interval in minutes (5-60)

### Factory Reset
Factory reset includes weather settings restoration:
- Weather source set to DISABLED
- All API keys, URLs, and tokens cleared
- Update interval reset to 30 minutes
- State field cleared

## 7-Day Scheduling System

### Schedule Structure
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
```

### Schedule Features
- **7-Day Configuration**: Individual schedules for each day of the week
- **Day/Night Periods**: Two periods per day with independent temperature settings
- **Three Temperature Modes**: Separate Heat, Cool, and Auto temperatures for each period
- **Flexible Timing**: User-configurable start times for day and night periods
- **Override System**: Temporary (2-hour) or permanent override capabilities
- **MQTT Integration**: Schedule status and changes published to Home Assistant
- **Persistent Storage**: All settings saved to ESP32 preferences
- **Real-Time Application**: Schedule automatically applied based on current time

### Schedule Operation
1. **Time Check**: System checks current time every 30 seconds
2. **Period Detection**: Determines if current time is in day or night period
3. **Temperature Application**: Applies scheduled temperatures to thermostat setpoints
4. **MQTT Updates**: Publishes schedule status and active period
5. **Override Handling**: Manages temporary and permanent overrides
6. **Home Assistant Integration**: Schedule entities available in HA

### Default Schedule
- **Day Period**: 6:00 AM - Heat: 72°F, Cool: 76°F, Auto: 74°F
- **Night Period**: 10:00 PM - Heat: 68°F, Cool: 78°F, Auto: 73°F
- **All Days Enabled**: Default schedule active for all 7 days
- **Master Schedule**: Disabled by default (manual operation)

## MQTT Integration

### Home Assistant Auto-Discovery
Automatically configures Home Assistant with:
- Climate entity with full thermostat control
- Temperature and humidity sensors
- Mode and fan mode controls
- Real-time status updates

### MQTT Topics
#### Command Topics (Subscribed)
- `esp32_thermostat/mode/set`: Thermostat mode control
- `esp32_thermostat/fan_mode/set`: Fan mode control
- `esp32_thermostat/target_temperature/set`: Temperature setpoint

#### Status Topics (Published)
- `esp32_thermostat/current_temperature`: Current temperature
- `esp32_thermostat/current_humidity`: Current humidity
- `esp32_thermostat/target_temperature`: Current setpoint
- `esp32_thermostat/mode`: Current thermostat mode
- `esp32_thermostat/fan_mode`: Current fan mode
- `esp32_thermostat/availability`: Online/offline status

#### Schedule Topics (New in v1.1.0)
- `<hostname>/schedule_enabled`: Schedule master enable/disable status
- `<hostname>/active_period`: Current active period ("day", "night", "manual")
- `<hostname>/schedule_override`: Schedule override status
- Alert topics with improved hysteresis and non-retained delivery

#### Home Assistant Auto-Discovery
- Automatic climate entity creation with full thermostat control
- Schedule status sensors for monitoring active periods
- Temperature and humidity sensors with device information
- All entities grouped under single thermostat device

## Touch Interface

### Main Display Elements
- **Time Display**: Current time and day of week
- **Temperature/Humidity**: Right-side vertical display
- **Setpoint**: Large center display
- **Status Indicators**: Color-coded HEAT/COOL/FAN status
- **Control Buttons**: Bottom row for user interaction

### Button Layout
- **Minus (-)**: Decrease temperature setpoint
- **WiFi Setup**: Enter WiFi configuration mode
- **Mode**: Cycle through Off/Heat/Cool/Auto
- **Fan**: Cycle through Auto/On/Cycle modes
- **Plus (+)**: Increase temperature setpoint

### WiFi Setup Interface
- **On-screen Keyboard**: QWERTY layout for credentials
- **Case Toggle**: Uppercase/lowercase switching
- **Special Characters**: Numbers and symbols row
- **Clear/Delete**: Text editing functions

## PCB Design

### Professional Layout
The custom PCB provides:
- **Clean Integration**: All components on a single board
- **Proper Grounding**: Dedicated ground planes
- **Signal Integrity**: Proper trace routing and impedance
- **Connector Organization**: Labeled terminal blocks for easy wiring
- **Mounting Options**: Standard mounting holes for enclosure installation

### Manufacturing Files
Located in `ESP32-Simple-Thermostat-PCB/jlcpcb/`:
- **Gerber Files**: Manufacturing layer files
- **Drill Files**: Via and hole positioning
- **BOM**: Bill of Materials for component sourcing
- **CPL**: Component Placement List for assembly

## Installation and Setup

### Hardware Assembly
1. **PCB Assembly**: Solder all components per BOM
2. **Display Connection**: Wire TFT display per pin configuration
3. **Sensor Installation**: Mount DHT11 and DS18B20 sensors
4. **Relay Wiring**: Connect HVAC equipment to relay outputs
5. **Power Supply**: Provide stable 5V power to ESP32

### Software Installation
1. **PlatformIO Setup**: Install PlatformIO IDE
2. **Library Dependencies**: Auto-installed via platformio.ini
3. **Upload Firmware**: Compile and upload Main-Thermostat.cpp
4. **Initial Configuration**: Use touch interface for WiFi setup

### HVAC System Integration
1. **Safety First**: Turn off power to HVAC system
2. **Wire Identification**: Identify common, heating, cooling, and fan wires
3. **Relay Connection**: Connect HVAC wires to appropriate relay outputs
4. **Testing**: Verify proper operation before final installation

## Configuration

### Initial Setup
1. **Touch Calibration**: Automatic on first boot
2. **WiFi Configuration**: Use on-screen keyboard
3. **Temperature Units**: Fahrenheit/Celsius selection
4. **Time Zone**: Select from comprehensive list
5. **HVAC Type**: Configure staging and fan requirements

### MQTT Configuration
1. **Server Settings**: IP address and port
2. **Authentication**: Username and password
3. **Home Assistant**: Enable auto-discovery
4. **Testing**: Verify connectivity and control

### Advanced Settings
1. **Staging Parameters**: Runtime and temperature thresholds
2. **Hydronic Settings**: Enable and configure water temperature monitoring
3. **Fan Scheduling**: Set cycle time and frequency
4. **Temperature Swings**: Fine-tune hysteresis for comfort

## Troubleshooting

### Common Issues

#### WiFi Connection Problems
- **Symptom**: Cannot connect to WiFi
- **Solution**: 
  - Verify SSID and password accuracy
  - Check signal strength at installation location
  - Ensure 2.4GHz network (ESP32 doesn't support 5GHz)
  - Use WiFi setup button to reconfigure

#### Touch Screen Unresponsive
- **Symptom**: Touch input not working
- **Solution**:
  - Check display connections
  - Verify touch screen calibration
  - Ensure proper grounding
  - Reboot system to recalibrate

#### MQTT Connection Failures
- **Symptom**: No Home Assistant integration
- **Solution**:
  - Verify MQTT broker is running
  - Check network connectivity
  - Validate MQTT credentials
  - Confirm port accessibility (usually 1883)

#### Temperature Reading Issues
- **Symptom**: Invalid or missing temperature readings
- **Solution**:
  - Check DHT11 sensor connections
  - Verify sensor power supply
  - Replace sensor if readings remain invalid
  - Check for electromagnetic interference

#### Relay Control Problems
- **Symptom**: HVAC equipment not responding
- **Solution**:
  - Verify relay connections and power
  - Check HVAC wire connections
  - Test relays manually through web interface
  - Ensure proper common wire connection

### Factory Reset
- **Method**: Hold boot button for 10+ seconds while system is running
- **Result**: All settings restored to defaults
- **Recovery**: System will reboot and require reconfiguration

## Maintenance

### Regular Maintenance
- **Sensor Cleaning**: Keep DHT11 sensor free of dust and debris
- **Display Care**: Clean touch screen with soft, dry cloth
- **Connection Check**: Periodically verify all wire connections
- **Software Updates**: Check for firmware updates via OTA interface

### Monitoring
- **Web Interface**: Regular status checks via web browser
- **Serial Output**: Debug information available via USB serial
- **MQTT Logs**: Monitor Home Assistant logs for communication issues
- **Temperature Trends**: Track heating/cooling efficiency over time

## Safety Considerations

### Electrical Safety
- **Power Disconnection**: Always disconnect HVAC power before wiring
- **Proper Grounding**: Ensure all components are properly grounded
- **Overcurrent Protection**: Use appropriate fuses/breakers
- **Professional Installation**: Consider professional installation for complex systems

### System Safety
- **Backup Thermostat**: Keep manual thermostat as backup
- **Failure Modes**: System defaults to safe off mode on errors
- **Temperature Limits**: Built-in minimum/maximum temperature limits
- **Watchdog Timer**: Automatic system reset on software failures

## Development and Customization

### Code Structure
- **Modular Design**: Functions organized by purpose
- **Configuration Management**: Centralized settings storage
- **Error Handling**: Comprehensive error checking and recovery
- **Debug Output**: Extensive serial logging for troubleshooting

### Customization Options
- **Temperature Limits**: Modify min/max temperature ranges
- **Display Layout**: Customize screen appearance and layout
- **Additional Sensors**: Add support for more sensor types
- **Custom MQTT Topics**: Modify topic structure for specific needs
- **Web Interface**: Add custom web pages and functionality

### Extension Possibilities
- **Weather Integration**: Add outdoor temperature compensation
- **Scheduling**: Implement time-based temperature schedules
- **Energy Monitoring**: Add power consumption tracking
- **Additional Zones**: Support for multiple zone control
- **Mobile App**: Develop companion mobile application

## Support and Resources

### Documentation
- This comprehensive documentation
- Inline code comments
- PlatformIO library documentation
- ESP32 technical reference

### Troubleshooting Resources
- Serial debug output
- Web interface status pages
- MQTT debug topics
- Home Assistant entity states

### Community Support
- GitHub issues for bug reports
- Community forums for general questions
- Documentation updates and improvements
- Feature requests and enhancements

## Version History

### Version 1.3.5 (Current - December 2025)
- **Weather Integration**: Modular Weather.h/Weather.cpp with dual-source support
- **OpenWeatherMap Support**: Free API integration with city, state, country configuration
- **Home Assistant Weather**: Direct HA weather entity integration via REST API
- **Weather Display**: Color-coded standard OWM icons (36x36) with temperature and conditions
- **Weather Web Tab**: Dedicated configuration tab with AJAX form submission
- **Anti-Flicker Optimization**: Cached redraw for time and weather display elements
- **Enhanced Time Display**: Format "HH:MM Weekday Mon D YYYY" with flicker elimination
- **Weather Settings Storage**: Complete settings persistence with state field for US cities
- **URL Encoding**: Proper handling of city names with spaces and special characters
- **Icon Color Coding**: Weather-appropriate colors (orange sun, blue rain, white snow, etc.)

### Version 1.3.0 (November 2025)
- **Enhanced OTA Update System**: Real-time progress tracking for firmware uploads
- **Flash Write Progress**: Visual feedback during flash write operations with percentage updates
- **Improved OTA User Experience**: Integrated OTA interface in System tab with clear status messages
- **Optimized Reboot Timing**: Proper response transmission before device restart
- **Version Verification**: Automatic firmware version confirmation after successful update
- **Code Cleanup**: Removed redundant standalone update page, streamlined codebase

### Version 1.2.4 (November 2025)
- **OTA Progress Improvements**: Added upload progress bar and flash write tracking
- **Enhanced Status Messages**: Clear indication of update stages
- **Reboot Timing Fix**: Improved timing to prevent connection error messages

### Version 1.1.0 (November 2025)
- **7-Day Scheduling System**: Complete inline scheduling with day/night periods and editable Heat/Cool/Auto temperatures
- **Modern Tabbed Web Interface**: All features embedded in main page - Status, Settings, Schedule, and System tabs
- **Enhanced Schedule Features**: Per-day enable/disable, time controls, override system, and MQTT integration
- **MQTT Alert Improvements**: Non-retained alerts with hysteresis-based reset logic for better Home Assistant integration
- **Hydronic Safety Enhancements**: [LOCKOUT] labels replacing [SAFETY] for clearer system status indication
- **Schedule MQTT Integration**: Real-time schedule status publishing to Home Assistant with device grouping
- **Persistent Schedule Storage**: All schedule settings saved to ESP32 preferences with automatic loading
- **Auto Temperature Control**: Independent user-editable auto temperatures for each schedule period
- **Inline Interface Design**: No separate pages - all functionality embedded in tabbed main interface
- **Always-Visible Options**: All schedule controls permanently displayed without hidden menus

### Version 1.0.8 (November 2025)
- **ESP32-S3-WROOM-1-N16 Platform**: Optimized for 16MB flash with default_16mb.csv partition
- **Dual-Core FreeRTOS Architecture**: Core 0 for UI/network, Core 1 for sensors/control
- **Option C Centralized Display Management**: Mutex-protected display updates with task coordination
- **Modern Material Design UI**: Enhanced color scheme with improved readability
- **Advanced Multi-Stage HVAC**: Hybrid time/temperature staging with configurable parameters
- **Comprehensive MQTT Integration**: Home Assistant auto-discovery with temperature precision
- **Hydronic Heating Support**: DS18B20 water temperature monitoring with safety interlocks
- **Adaptive Display Brightness**: Light sensor integration with PWM backlight control
- **Factory Reset Capability**: 10-second boot button press for complete settings reset
- **Professional PCB Integration**: Full compatibility with Stefan Meisner's hardware design
- **Robust Error Handling**: Watchdog timer, graceful offline operation, comprehensive logging
- **Touch Interface Optimization**: Maximum responsiveness with keyboard support for WiFi setup
- **OTA Update Support**: Web-based firmware updates
- **Production-Ready Stability**: Extensive testing and optimization for 24/7 operation

### Previous Versions
- **1.0.7**: Material Design colors, ESP32-S3-WROOM-1-N16 optimization
- **1.0.6**: Visual improvements and UI enhancements
- **1.0.5**: Option C display system implementation
- **1.0.4**: HVAC logic working, stable baseline
- **1.0.3**: Initial multi-stage HVAC support
- **1.0.2**: Basic thermostat functionality
- **1.0.1**: Initial ESP32-S3 port
- **1.0.0**: Original ESP32 implementation

## License

This project is released under the GNU General Public License v3.0. You are free to use, modify, and distribute this software according to the terms of the GPL v3.0 license.

## Conclusion

The Firmware represents a complete, professional-grade smart thermostat solution. With its combination of local control, smart home integration, and advanced HVAC features, it provides a cost-effective alternative to commercial smart thermostats while offering superior customization and control options.

The addition of the custom PCB elevates this from a prototype to a production-ready solution suitable for permanent installation in residential and commercial environments.
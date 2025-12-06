# Smart Thermostat Alt Firmware - Project Structure

This document provides an overview of the project structure and organization for the ESP32-S3 based Smart Thermostat Alt Firmware.

## 📁 Project Directory Structure

```
Smart-Thermostat-Alt-Firmware/
├── 📄 README.md                          # Project overview and quick start guide
├── 📄 DOCUMENTATION.md                   # Comprehensive technical documentation
├── 📄 DEVELOPMENT_GUIDE.md               # Developer guide and best practices
├── 📄 PROJECT_STRUCTURE.md              # This file - project organization guide
├── 📄 platformio.ini                    # PlatformIO build configuration
├── 📄 Smart-Thermostat-Alt-Firmware.code-workspace # VS Code workspace settings
├── 📄 Thermostat-sch.pdf                # Hardware schematic reference
│
├── 📁 src/                              # Source code directory
│   ├── 📄 Main-Thermostat.cpp          # Main application source (3640+ lines)
│   └── 📄 Weather.cpp                  # Weather module implementation with dual API support
│
├── 📁 include/                          # Header files directory
│   ├── 📄 TFT_Setup_ESP32_S3_Thermostat.h # TFT display configuration (legacy)
│   ├── 📄 Weather.h                     # Weather module interface with WeatherSource enum
│   ├── 📄 WebInterface.h                # Modern web interface CSS, icons, and JavaScript
│   └── 📄 WebPages.h                    # HTML page generation functions
│
├── 📁 lib/                              # Custom library configurations
│   └── 📁 TFT_eSPI_Setup/
│       └── 📄 User_Setup.h              # Custom TFT_eSPI configuration (legacy)
│
├── 📁 .pio/                             # PlatformIO build files (auto-generated)
│   ├── 📁 build/esp32-s3-wroom-1-n16/  # Build artifacts
│   └── 📁 libdeps/esp32-s3-wroom-1-n16/ # Library dependencies
│
├── 📁 .vscode/                          # VS Code configuration
│   ├── 📄 extensions.json              # Recommended extensions
│   └── 📄 settings.json                # Project settings
│
└── 📁 img/                              # Project images and screenshots
    ├── 📄 web-status.png                # Web interface status page
    ├── 📄 web-settings.png              # Web interface settings page
    └── 📄 *.png                         # Additional project images
```

## 📋 File Descriptions

### Core Project Files

#### `README.md`
- Project overview and introduction
- Quick start guide and setup instructions
- Key features and capabilities overview
- Hardware requirements and pin configurations
- Web interface screenshots
- Home Assistant integration guide

#### `DOCUMENTATION.md`
- Comprehensive technical documentation
- Detailed architecture explanation
- Complete function reference
- Hardware integration guide
- Troubleshooting and maintenance
- Development and customization guide

#### `DEVELOPMENT_GUIDE.md`
- Developer guide and best practices
- ESP32-S3 specific development guidelines
- Dual-core FreeRTOS programming patterns
- TFT_eSPI configuration for custom hardware
- Build system configuration and optimization

#### `platformio.ini`
- PlatformIO build configuration for ESP32-S3-WROOM-1-N16
- Library dependencies with specific versions
- Build flags for TFT_eSPI hardware pin configuration
- Partition scheme: default_16mb.csv for maximum application space
- Serial monitor settings and upload configuration

#### `Thermostat-sch.pdf`
- Hardware schematic reference document
- Complete circuit design and component specifications
- Pin assignments for ESP32-S3-WROOM-1-N16
- Relay control circuits and sensor connections

### Source Code Architecture

#### `src/Main-Thermostat.cpp` (3640+ lines)
- Single comprehensive source file containing complete thermostat implementation with 7-day scheduling, LD2410 motion detection, and weather integration
- Organized into logical function groups with clear separation of concerns
- **Version**: 1.3.5 with advanced ESP32-S3 dual-core architecture, scheduling system, motion sensor integration, and weather display

#### `src/Weather.cpp`
- Weather module implementation with dual API support (OpenWeatherMap and Home Assistant)
- OpenWeatherMap integration with URL-encoded city names, state field for US disambiguation
- Home Assistant REST API integration with Bearer token authentication
- Color-coded weather icon rendering with standard OWM icon codes (01-50)
- Anti-flicker display optimization with cached redraw logic
- WeatherData struct with temperature, conditions, humidity, wind, and icon code

**Code Organization:**
- **Lines 1-50**: Header, license, and hardware credits
- **Lines 51-200**: Constants, pin definitions, and hardware configuration (including LD2410 pins)
- **Lines 201-300**: Global variables, system state, scheduling structures, and motion sensor variables
- **Lines 301-400**: Function prototypes organized by category (including schedule and motion sensor functions)
- **Lines 401-650**: Setup, initialization, schedule loading, and LD2410 initialization functions
- **Lines 651-850**: Main loop with non-blocking task scheduling, schedule checking, and motion detection
- **Lines 851-1000**: Dual-core FreeRTOS task functions
- **Lines 1001-1600**: Display management (Option C centralized approach with motion wake)
- **Lines 1601-2300**: HVAC control logic with multi-stage support and schedule application
- **Lines 2301-2900**: Communication systems (MQTT, WiFi, web server with schedule routes and motion sensor discovery)
- **Lines 2901-3200**: Settings management, schedule persistence, and preferences
- **Lines 3201-3400**: Motion sensor functions (testLD2410Connection, readMotionSensor)
- **Lines 3401-3640+**: Utility functions and hardware abstraction

#### `include/Weather.h`
- Weather module interface definition with WeatherSource enum (DISABLED, OPENWEATHERMAP, HOMEASSISTANT)
- WeatherData struct with temperature, high/low, conditions, humidity, wind speed, icon code
- Weather class with dual API configuration methods and display integration
- Public methods: begin(), setSource(), setOpenWeatherMapConfig(), setHomeAssistantConfig(), update(), displayOnTFT()
- Private members: API credentials, update intervals, cached weather data, last error tracking

### Web Interface Architecture

#### `include/WebInterface.h`
- Modern Material Design CSS framework
- SVG icon library for consistent UI elements
- JavaScript for dynamic functionality and tab management
- Responsive design supporting desktop and mobile devices
- Auto-refresh capabilities for real-time status updates

#### `include/WebPages.h`
- HTML page generation functions using modern templates with tabbed interface
- `generateStatusPage()`: Real-time system status with embedded Settings, Schedule, and System tabs
- **Schedule Tab Integration**: Complete 7-day scheduling interface embedded in main page
- **Schedule Data Structures**: `SchedulePeriod` and `DaySchedule` structs for comprehensive scheduling
- `generateSettingsPage()`: Standalone comprehensive configuration interface (legacy)
- `generateFactoryResetPage()`: System reset confirmation
- `generateOTAPage()`: Over-the-air firmware update interface
- Responsive design with always-visible schedule options and no hidden menus

### Configuration Files

#### `include/TFT_Setup_ESP32_S3_Thermostat.h` (Legacy)
- Legacy TFT display configuration file (unused in current build)
- Superseded by build flags in platformio.ini
- Kept for reference and potential future modularization

#### `lib/TFT_eSPI_Setup/User_Setup.h` (Legacy)
- Custom TFT_eSPI configuration (unused in current build)
- Hardware-specific pin definitions and display settings
- Replaced by platformio.ini build flags for better integration

**Active Configuration**: Display settings now managed via build flags in `platformio.ini`:
```ini
build_flags = 
    -DUSER_SETUP_LOADED=1
    -DILI9341_DRIVER=1
    -DTFT_MISO=37
    -DTFT_MOSI=35
    -DTFT_SCLK=36
    # ... additional pin definitions
```

### Documentation Images

#### Web Interface Screenshots
- `web-status.png`: Real-time status monitoring page showing current system state
- `web-settings.png`: Configuration interface page for thermostat settings
- Additional screenshots showing various system states and interface modes

## 🔧 Development Workflow

### Getting Started
1. **Clone Repository**: `git clone <repository-url>`
2. **Open in PlatformIO**: Load project in VS Code with PlatformIO extension
3. **Install Dependencies**: PlatformIO automatically installs libraries from `platformio.ini`
4. **Build Project**: PlatformIO Build (Ctrl+Alt+B) 
5. **Upload Firmware**: PlatformIO Upload (Ctrl+Alt+U) to ESP32-S3

### Development Tools
- **Primary IDE**: VS Code with PlatformIO extension
- **Target Platform**: ESP32-S3-WROOM-1-N16 (16MB Flash, No PSRAM)
- **Version Control**: Git (GitHub repository)
- **Serial Monitor**: PlatformIO Serial Monitor (115200 baud)
- **MQTT Testing**: MQTT.fx, MQTT Explorer, or Home Assistant
- **Web Interface**: Built-in web server for configuration and monitoring

### Code Organization Strategy
- **Single Source File**: Simplifies deployment and compilation (3096 lines)
- **Dual-Core Architecture**: FreeRTOS tasks on Core 0 (UI) and Core 1 (sensors/control)
- **Function Grouping**: Related functions organized together with clear separation
- **Thread Safety**: Mutex-protected shared resources and display management
- **Extensive Logging**: Serial debug output throughout for troubleshooting
- **Modular Functions**: Each major feature in dedicated functions for maintainability

## 📚 Learning Resources

### Understanding the Codebase
1. **Start with `README.md`**: Get project overview and setup
2. **Review `DOCUMENTATION.md`**: Understand ESP32-S3 architecture and features
3. **Study `DEVELOPMENT_GUIDE.md`**: Learn development patterns and best practices
4. **Examine `Main-Thermostat.cpp`**: Follow code organization and dual-core logic flow

### Key Code Sections to Study
1. **`setup()` Function**: ESP32-S3 initialization sequence and hardware setup
2. **`loop()` Function**: Main task scheduling and non-blocking state management
3. **FreeRTOS Tasks**: `sensorTaskFunction()` and `displayUpdateTaskFunction()`
4. **`controlRelays()`**: Core thermostat control logic with multi-stage support
5. **`updateDisplay()`**: Centralized display management (Option C approach)
6. **MQTT Functions**: Home Assistant auto-discovery and integration

### ESP32-S3 Specific Learning
1. **Dual-Core Programming**: Study FreeRTOS task implementation
2. **Pin Configuration**: Review platformio.ini build flags for hardware setup
3. **Memory Management**: 16MB Flash partition usage and optimization
4. **Display Integration**: TFT_eSPI configuration for ILI9341 with XPT2046 touch

## 🚀 Extension Points

### Code Extensibility
- **Additional Sensors**: Add support for more sensor types (BME280, SHT30)
- **New Display Elements**: Extend UI with weather information, graphs
- **Enhanced MQTT**: Add more Home Assistant entities and sensors
- **Data Logging**: Add historical data collection to SD card or cloud
- **Energy Monitoring**: Track HVAC runtime and energy usage

### Hardware Extensions
- **Outdoor Temperature**: Weather compensation with external sensor
- **Multiple Zones**: Support for multi-zone HVAC systems (additional relays)
- **Touch Improvements**: Upgrade to capacitive touch for better response
- **Wireless Sensors**: ESP-NOW for remote temperature sensors
- **Display Upgrades**: Larger TFT displays or e-ink for low power

### ESP32-S3 Specific Enhancements
- **USB Support**: Native USB for programming and debugging
- **WiFi 6**: Utilize advanced WiFi capabilities
- **Additional GPIO**: Leverage ESP32-S3's expanded I/O capabilities
- **Crypto Acceleration**: Use hardware crypto for secure communications
- **Machine Learning**: TinyML for predictive temperature control

## 🔍 Troubleshooting File Locations

### Debug Information Sources
- **Serial Output**: Connect to ESP32-S3 USB port at 115200 baud
- **Web Interface**: Status page shows real-time system state at `http://<IP>/`
- **MQTT Topics**: Monitor published topics for Home Assistant communication
- **Preferences Storage**: Settings stored in ESP32-S3 flash memory (NVS)
- **Build Output**: Check `.pio/build/esp32-s3-wroom-1-n16/` for compilation details

### Common Issues and Solutions
- **Compilation Errors**: Check `platformio.ini` library versions and build flags
- **Upload Failures**: Verify ESP32-S3 connection, drivers, and boot mode
- **Display Problems**: Review build flags for TFT_eSPI pin configuration
- **Memory Issues**: Monitor flash usage (currently 30.3% of 16MB)
- **FreeRTOS Crashes**: Check stack sizes and inter-task communication
- **MQTT Connection**: Verify WiFi credentials and broker settings

### Performance Monitoring
- **Flash Usage**: Current build uses ~3.14MB of 16MB available
- **RAM Usage**: Monitor heap free space via serial output
- **Task Performance**: Dual-core load balancing between UI and control tasks
- **MQTT Response**: Home Assistant auto-discovery and command processing

## 📊 Current Project Status

### Version Information
- **Firmware Version**: 1.3.5 (December 2025)
- **Platform**: ESP32-S3-WROOM-1-N16 (16MB Flash, No PSRAM)
- **Code Size**: 3640+ lines of production-ready C++ with comprehensive scheduling and weather integration
- **Flash Utilization**: 18.5% (1,210,272 bytes - plenty of room for expansion)

### Key Features Implemented
- **Weather Integration**: Modular Weather.h/Weather.cpp with dual-source support (OpenWeatherMap and Home Assistant)
- **Weather Display**: Color-coded standard OWM icons with temperature, conditions, and high/low on TFT
- **Anti-Flicker Display**: Cached redraw optimization for time and weather elements
- **7-Day Scheduling System**: Complete inline scheduling with day/night periods and editable Heat/Cool/Auto temperatures
- **LD2410 Motion Sensor Integration**: 24GHz mmWave radar with automatic display wake and robust detection logic
- **Modern Tabbed Web Interface**: All features embedded in main page including Weather tab - no separate pages, always-visible options
- **Dual-Core Architecture**: Core 0 (UI/Network), Core 1 (Sensors/Control)
- **Material Design UI**: Modern, responsive touch interface with scheduling and weather integration
- **Enhanced Home Assistant Integration**: MQTT auto-discovery with schedule status publishing, motion sensor entities, and comprehensive device grouping
- **Multi-Stage HVAC**: Advanced heating/cooling with staging support and schedule integration
- **Schedule MQTT Integration**: Real-time schedule status and override control via MQTT
- **Motion-Based Display Wake**: Automatic display wake on motion detection with seamless touch integration
- **Comprehensive Web Configuration**: Tabbed interface with Status, Settings, Schedule, Weather, and System tabs
- **Persistent Schedule Storage**: All schedule settings saved to NVS with automatic loading
- **Enhanced Safety Features**: Improved hydronic [LOCKOUT] system, watchdog timers, and sensor validation

This project structure provides a robust foundation for both learning and extending the ESP32-S3 Smart Thermostat system. The single-file architecture maintains simplicity while the dual-core FreeRTOS implementation ensures professional-grade performance and reliability.
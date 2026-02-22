# ESP32-S3-Simple-Thermostat

A comprehensive, feature-rich simple thermostat system built on the ESP32 platform with professional PCB design. Perfect for DIY smart home automation with full Home Assistant integration.

![Main-Display](img/KIMG20251212_231454862.JPG)

## 🌟 Key Features

- **📱 Local Touch Control**: ILI9341 TFT LCD with intuitive touch interface
- **🏠 Smart Home Ready**: Full MQTT integration with Home Assistant auto-discovery
- **📅 7-Day Scheduling**: Comprehensive inline scheduling with day/night periods and editable Heat/Cool/Auto temperatures
- **🌡️ Multiple Sensors**: AHT20/BME280 for ambient conditions + DS18B20 for hydronic systems
- **⚡ Multi-Stage HVAC**: Support for 2-stage heating and cooling systems
- **💨 Advanced Fan Control**: Auto, continuous, and scheduled cycling modes
- **🚿 Shower Mode**: Pause heating for 5-120 minutes with countdown timer and buzzer alert
- **🌤️ Weather Integration**: OpenWeatherMap and Home Assistant weather with color-coded icons on display
- **🌐 Modern Web Interface**: Complete tabbed interface with embedded scheduling - no separate pages
- **📡 Offline Operation**: Full functionality without WiFi connection
- **🔧 Professional PCB**: Custom PCB design for clean, permanent installation
- **🔄 OTA Updates**: Over-the-air firmware updates with real-time progress tracking
- **🔒 Factory Reset**: Built-in reset capability via boot button
- **🎭 Motion Detection**: LD2410 24GHz mmWave sensor for automatic display wake

## 🚀 Quick Start

![Hardware-Main-Display](pcb/ESP32-DevKitC3-Simple-Thermostat-PCB_front.png)
![Hardware-Main-Display](pcb/ESP32-DevKitC3-Simple-Thermostat-PCB_back.png)

### Hardware Requirements
- ESP32-S3-WROOM-1-N16 (16MB Flash, No PSRAM) or N8/N32 variants
- ILI9341 320x240 TFT LCD with XPT2046 Touch Controller
- AHT20 or BME280 Temperature/Humidity Sensor (I2C)
- DS18B20 Temperature Sensor (optional, for hydronic heating)
- LD2410 24GHz mmWave Motion Sensor (optional, for display wake)
- 5x Relay Module for HVAC control
- Custom PCB that can use either onboard relay or external relays via pin header

### Software Setup

#### Option 1: Use Prebuilt Firmware (Recommended)
1. Clone this repository
2. Navigate to `firmware/` directory
3. Find the latest build folder (e.g., `build_YYYYMMDD-HHMMSS/`)
4. Flash using the variant-specific script:
   - **Linux/Mac**: 
     - Default: `./latest_flash_N16.sh` (uses /dev/ttyACM0)
     - Custom port: `./latest_flash_N16.sh /dev/ttyUSB0`
   - **Windows**: 
     - Default: `latest_flash_N16.bat` (uses COM3)
     - Custom port: `latest_flash_N16.bat COM4`
5. Use touch interface to configure WiFi and settings

#### Option 2: Build from Source
1. Install [PlatformIO](https://platformio.org/) IDE
2. Clone this repository
3. Open project in PlatformIO
4. Build firmware using `./build.sh` with options:
   - **Interactive mode**: `./build.sh` (select from menu)
   - **Specific variant**: `./build.sh 1` (N8), `./build.sh 2` (N16), or `./build.sh 3` (N32)
   - **All variants**: `./build.sh all` or `./build.sh 4`
   - **Additional flags**: `clean`, `quiet`, `cleanlibs`
   - **Examples**: 
     - `./build.sh 2` - Build 16MB variant (default)
     - `./build.sh all clean` - Clean build all variants
     - `./build.sh 3 quiet` - Build 32MB silently
     - `./build.sh cleanlibs` - Remove all libraries and packages
5. Memory usage: RAM 25.2% (82728/327680 bytes); Flash 19.0% (1246KB/6553KB for N16)
6. Firmware organized in `firmware/N8/`, `firmware/N16/`, `firmware/N32/` directories
7. Flash using variant-specific scripts:
   - **Linux/Mac**: 
     - Default: `./firmware/latest_flash_N16.sh` (uses /dev/ttyACM0)
     - Custom port: `./firmware/latest_flash_N16.sh /dev/ttyUSB0`
   - **Windows**: 
     - Default: `firmware\latest_flash_N16.bat` (uses COM3)
     - Custom port: `firmware\latest_flash_N16.bat COM5`
8. Use touch interface to configure WiFi and settings

#### Flashing Requirements
- **esptool.py** (installed automatically with PlatformIO)
- **USB connection** to ESP32-S3
- **Boot mode**: Hold BOOT button while connecting USB (if needed)
- **Default serial port**: `/dev/ttyACM0` (Linux/Mac) or `COM3` (Windows)

#### Manual Flashing with esptool

If you need to flash manually using esptool, use the following command:

**Linux/Mac:**
```bash
esptool.py --chip esp32s3 --port /dev/ttyACM0 --baud 460800 --before default_reset --after hard_reset write_flash -z \
    --flash_mode dio --flash_freq 80m --flash_size 16MB \
    0x0 bootloader.bin \
    0x8000 partitions.bin \
    0x10000 firmware.bin
```

**Windows:**
```bash
esptool.py --chip esp32s3 --port COM3 --baud 460800 --before default_reset --after hard_reset write_flash -z \
    --flash_mode dio --flash_freq 80m --flash_size 16MB \
    0x0 bootloader.bin \
    0x8000 partitions.bin \
    0x10000 firmware.bin
```

**Flash addresses for the 3 required files:**
- `bootloader.bin` at address `0x0`
- `partitions.bin` at address `0x8000`
- `firmware.bin` at address `0x10000`

**Adjust for your hardware:**
- Change `--port` to your serial port (Linux: `/dev/ttyACM0` or `/dev/ttyUSB0`, Windows: `COM3`, `COM4`, etc.)
- Change `--flash_size` to `8MB`, `16MB`, or `32MB` to match your ESP32-S3 variant (N8, N16, or N32)

For detailed build and flash instructions, see [USER_MANUAL.md](USER_MANUAL.md)

## 💻 Web Interface

Access the thermostat's web interface by navigating to its IP address:

### Tabbed Interface with Embedded Features

**Status Tab**: Real-time monitoring of:
- Current temperature and humidity
- Thermostat and fan modes
- Relay states and system status
- Weather information (when configured)
- Barometric pressure (with BME280 sensor)

**Settings Tab**: Complete configuration interface for:

- Temperature setpoints and control modes
- MQTT/Home Assistant integration
- WiFi network settings
- Multi-stage HVAC parameters
- Hydronic heating controls
- Fan scheduling options
- Shower mode enable/disable and duration (5-120 minutes)
- Display brightness and sleep settings
- Temperature/humidity sensor calibration

**Schedule Tab**: Comprehensive 7-day scheduling:
- Day and night periods for each day of the week
- Editable Heat, Cool, and Auto temperatures for each period
- Time controls for period transitions
- Schedule enable/disable and override controls
- All options always visible - no hidden menus

**Weather Tab**: Weather data configuration:
- OpenWeatherMap API integration
- Home Assistant weather entity integration
- Configurable update intervals (5-60 minutes)
- City/state/country configuration

**System Tab**: Device information and firmware updates:
- System information and uptime
- Firmware version details
- OTA firmware upload with progress tracking
- Reboot and factory reset options

For complete usage instructions, see [USER_MANUAL.md](USER_MANUAL.md)

## 🏠 Home Assistant Integration

### Automatic Discovery & Control
Automatic discovery and integration with Home Assistant:

1. Enable MQTT in thermostat settings
2. Configure MQTT broker details
3. Thermostat appears automatically in Home Assistant (91 entities per thermostat)
4. Full control via Home Assistant interface
5. Supports climate entity with heating/cooling modes

### Bidirectional Schedule Sync 🔄
Full synchronization between thermostat and Home Assistant:

**Device → HA (Inbound)**: 
- Thermostat publishes complete schedule on boot and config changes
- HA automations automatically update 77 helper entities (per device)
- Changes made on device instantly visible in HA

**HA → Device (Outbound)**:
- Change any schedule helper in HA (time, temperature, enabled status)
- Automation publishes change to device via MQTT immediately
- Device receives and applies change instantly
- No manual sync needed - fully automatic bidirectional flow

**Multi-Thermostat Support**:
- Each device gets its own set of helpers (shop_thermostat, studio_thermostat, house_thermostat, etc.)
- Centralized multi-device automation handles all thermostats
- Proper hostname normalization for MQTT topics vs. helper IDs
- Can manage 10+ thermostats from single HA instance

**Setup Instructions**:
1. Ensure MQTT is enabled on thermostat
2. Copy `multi_thermostat_schedule_sync.yaml` to HA packages directory
3. Generate per-device packages: `./generate_schedule_package.sh shop_thermostat`
4. Add package to HA configuration
5. Reload automations/scripts in HA
6. Helpers auto-populate from device schedule state

## 🛠️ Advanced Features

### Multi-Stage Operation
- Intelligent staging based on time and temperature
- Configurable stage 2 activation parameters
- Prevents system short-cycling
- Optimizes energy efficiency

### Hydronic Heating Support
- DS18B20 water temperature monitoring
- Safety interlocks prevent operation when water is too cold
- Configurable high/low temperature thresholds
- Perfect for radiant floor heating systems

### Fan Control Options
- **Auto**: Fan runs only with heating/cooling
- **On**: Continuous fan operation
- **Cycle**: Scheduled fan operation (configurable minutes per hour)

## 🖨️ 3D Printable Case

A professional two-part case design is included for clean wall-mount installation:

### Case Features
- **Two-part screw-retained design**: Front (display side) and back (wall-mount side) halves
- **Integrated PCB mounting**: Built-in standoffs (13mm height) for secure PCB installation
- **Countersunk front screws**: 4× M2.5 countersunk (DIN 7991), 10–12mm length, 7mm inset
- **Keyhole wall mounting**: Easy installation without removing case (83mm spacing)
- **Display opening**: Precise 50mm × 68mm opening for 3.2" ILI9341 touchscreen (rotated 90°)
- **AHT20 sensor + LDR cutouts**: 12.5×6mm rectangle (rotated 90°) and Ø5.5mm hole
- **Wire management**: 22mm back pass-through plus side ventilation slots
- **Ventilation**: Strategically placed slots for optimal heat dissipation
- **Professional finish**: Smooth surfaces and rounded edges

### Files Included
- `case/freecad_front_case.py` - Front case (display side) FreeCAD Python script
- `case/freecad_back_case.py` - Back case (wall-mount side) FreeCAD Python script
- `case/freecad_outputs/front_case_display_freecad.stl` - Front half for 3D printing
- `case/freecad_outputs/back_case_wall_freecad.stl` - Back half for 3D printing
- `case/freecad_outputs/front_case_display_freecad.FCStd` - Front case FreeCAD project
- `case/freecad_outputs/back_case_wall_freecad.FCStd` - Back case FreeCAD project
- `case/freecad_outputs/front_case_display_freecad.step` - Front case STEP format
- `case/freecad_outputs/back_case_wall_freecad.step` - Back case STEP format
- `case/freecad_outputs/CE5_front_case_display_freecad.gcode` - Creality Ender 5 gcode (front)
- `case/freecad_outputs/CE5_back_case_wall_freecad.gcode` - Creality Ender 5 gcode (back)

### Print Specifications
- **Material**: PLA or PETG recommended
- **Layer height**: 0.2mm
- **Infill**: 15-20%
- **Supports**: None required
- **Print time**: ~7–9 hours total
- **Dimensions**: 149.0mm × 105.5mm × ~47mm (assembled)
  - Front: ~17.1mm (2.5mm wall; 13mm standoffs; front walls 1.6mm above standoffs)
  - Back: ~30.1mm (2.5mm wall; 20mm component clearance; bosses/keyholes)

**STL Files Location**: All printable STL files are in the `case/freecad_outputs/` directory.

### Motion Detection
- **LD2410 24GHz mmWave Sensor**: Automatic display wake on motion detection
- **Hardware Pins**: RX=15, TX=16, Motion=18
- **Robust Detection**: Works with sensors that don't respond to UART commands
- **Home Assistant Integration**: Motion sensor auto-discovery and status publishing
- **Energy Saving**: Display automatically sleeps when no motion detected
- **Seamless Operation**: Works alongside existing touch controls

### Safety Features
- Watchdog timer prevents system lockups
- Factory reset via boot button (10+ seconds)
- Temperature limit enforcement
- Graceful offline operation

## 🔧 Factory Reset

Press and hold the boot button for more than 10 seconds while the thermostat is running to restore all settings to defaults.

## 📄 License

This project is released under the GNU General Public License v3.0. Free to use, modify, and distribute.

## 🤝 Contributing

Contributions welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

## 📞 Support

- **Issues**: Use GitHub Issues for bug reports
- **Discussions**: GitHub Discussions for questions
- **Documentation**: Comprehensive docs included
- **Serial Debug**: Detailed logging at 115200 baud

## ⭐ Version

**Current Version**: 1.4.009 (February 2026)

### Latest Features (v1.4.009)
- **Touch Screen Calibration System**: On-device calibration management via System Info page
  - Calibrate button: Interactive touch calibration with corner-tap routine
  - Clear Cal button: Reset calibration data and reboot to defaults
  - Non-blocking startup: Uses hardware-tested default values when no calibration saved
  - Debug logging: Calibration values displayed in boot log for troubleshooting
  - Improved default values: Updated from real hardware calibration (426, 3526, 248, 3417, 7)
- **Enhanced Fan Control**: Fan forced off during hydronic lockout (prevents cold air circulation)
- **Dual Hydronic Sensor Support**: Display supply and return temperatures simultaneously
  - S: (supply) and R: (return) temperature indicators on main screen
  - Graceful handling of missing sensors with "--" display
- **Display Optimizations**: 
  - Tightened sensor spacing for better information density
  - Relocated status indicators (Heating/Cooling left, Fan center)
  - Improved settings page layouts for better fit
- **LDR Dimming Control**: Optional light-sensor-based dimming (default: disabled)
  - User brightness setting used as maximum ceiling when enabled
  - Prevents unexpected brightness changes on fresh installs
- **Debug Buffer Expansion**: Increased from 32KB to 64KB for better boot log retention
- **Display Sleep Default Changed**: Now defaults to OFF for immediate touch response

### Previous Features (v1.4.001)
- **Bidirectional MQTT Schedule Sync**: Complete synchronization between thermostat and Home Assistant
  - 77 outbound automations per thermostat for all schedule parameters
  - Centralized multi-thermostat inbound automation
  - Automatic helper generation via script
  - Full support for unlimited thermostats
  - Proper hostname normalization (MQTT vs. helper IDs)
  - Fixed day index mapping (MQTT 0=Monday ↔ Firmware 0=Sunday)
- **Multi-Thermostat Support**: Manage multiple thermostats with automatic hostname handling
- **MQTT Helper Auto-Discovery**: 77 helpers per thermostat auto-created from device state
- **Schedule Automation Pack**: Single command generation for all outbound automations

### Previous Features (v1.3.9)
- **Shower Mode**: Pause heating for configurable duration (5-120 minutes) with countdown timer
  - Touch screen toggle on/off
  - Web interface enable/disable and duration control
  - MQTT/Home Assistant switch integration with auto-discovery
  - Visual countdown display with buzzer alert (5 beeps during last 5 seconds)
  - Heating automatically blocked while shower mode is active
- **Enhanced Fan Cycle Control**: Skip first fan cycle on boot to prevent immediate fan run
- **Status Page Reload**: Force page refresh when clicking Status tab to clear stale cache
- **BME280 Barometric Pressure**: Display atmospheric pressure on main screen when BME280 sensor is used
- **Weather on Status Tab**: Weather data now appears on main Status page for quick access

### Previous Features
- **Weather Integration** (v1.3.8): Dual-source weather support (OpenWeatherMap and Home Assistant)
- **Weather Display**: Color-coded standard OWM icons with temperature, conditions, and high/low display
- **Weather Web Interface**: Dedicated weather tab with AJAX form submission
- **Anti-Flicker Display**: Cached redraw optimization for time and weather elements
- **Enhanced Time Display**: Improved format "HH:MM Weekday Mon D YYYY" with flicker elimination
- **Weather Settings**: Configurable update intervals (5-60 minutes) with state field for US cities
- **Enhanced OTA Updates**: Real-time progress tracking for upload and flash write operations
- **Improved OTA UX**: Integrated OTA interface in System tab with status messages and reboot timing
- **7-Day Scheduling System**: Complete inline scheduling with day/night periods and editable Heat/Cool/Auto temperatures
- **LD2410 Motion Sensor Integration**: 24GHz mmWave radar for automatic display wake with robust detection
- **Modern Tabbed Web Interface**: All features embedded in main page - Status, Settings, Schedule, Weather, and System tabs
- **Enhanced MQTT Integration**: Motion sensor auto-discovery and status publishing to Home Assistant
- ESP32-S3-WROM-1-N16 platform with 16MB flash optimization
- Modern Material Design color scheme with enhanced readability
- Complete thermostat functionality with Option C display system
- Enhanced MQTT/Home Assistant integration with temperature precision
- Professional PCB design
- Multi-stage HVAC support with intelligent staging
- Dual-core FreeRTOS architecture for ESP32-S3

## 🙏 Credits & Acknowledgments

**Firmware**: Jonn Taylor - Enhanced firmware implementation

This project demonstrates the power of open-source collaboration - combining excellent hardware design with advanced firmware capabilities.

---

**Created for the DIY smart home community**

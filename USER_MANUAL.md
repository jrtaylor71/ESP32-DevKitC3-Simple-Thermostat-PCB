# ESP32-S3 Simple Thermostat - User Manual

**Version 1.3.9 | January 2026**

---

## Table of Contents

1. [Introduction](#introduction)
2. [Initial Setup](#initial-setup)
3. [Touch Screen Interface](#touch-screen-interface)
4. [Web Interface](#web-interface)
5. [Home Assistant Integration](#home-assistant-integration)
6. [Shower Mode](#shower-mode)
7. [Weather Integration](#weather-integration)
8. [Advanced Features](#advanced-features)
9. [Troubleshooting](#troubleshooting)
10. [Maintenance](#maintenance)

---

## Introduction

The ESP32-S3 Simple Thermostat is a fully-featured smart thermostat designed for DIY home automation enthusiasts. It combines local touch control with modern web and MQTT/Home Assistant integration.

### Key Capabilities
- **Local Control**: Full functionality via touch screen, no internet required
- **Web Interface**: Complete configuration and monitoring from any browser
- **Smart Home Integration**: Native MQTT and Home Assistant support
- **Advanced Scheduling**: 7-day schedule with day/night periods
- **Multi-Stage HVAC**: Support for 2-stage heating and cooling
- **Weather Display**: Live weather data from OpenWeatherMap or Home Assistant
- **Shower Mode**: Temporarily pause heating with countdown timer

---

## Initial Setup

### First Power-On

1. **Power Connection**: Connect 5V power to the thermostat
2. **Display Activation**: The TFT display will initialize and show the boot screen
3. **WiFi Configuration**: 
   - Tap anywhere on the boot screen
   - Enter your WiFi SSID and password using the on-screen keyboard
   - Tap "Connect" to join the network

### Finding Your Thermostat

Once connected to WiFi, the thermostat will display its IP address on screen. You can:
- Access the web interface at `http://[IP-ADDRESS]`
- Find it in your router's DHCP client list
- Use the hostname (default: `ESP32-Thermostat`) if your network supports mDNS

### Basic Configuration

1. Open the web interface in your browser
2. Navigate to the **Settings** tab
3. Configure essential settings:
   - **Temperature Unit**: Fahrenheit or Celsius
   - **Heat/Cool Setpoints**: Default comfort temperatures
   - **Temperature Swing**: Hysteresis to prevent short-cycling (default 1.0°)
   - **Time Zone**: Your local timezone for accurate scheduling

---

## Touch Screen Interface

### Main Display

The touch screen shows real-time information:

**Top Section**:
- Current time and date
- WiFi signal strength icon
- MQTT connection status (when enabled)

**Center Section**:
- **Large Temperature**: Current room temperature
- **Humidity**: Current relative humidity percentage
- **Target Temperature**: Current heating/cooling setpoint
- **Weather** (when configured): Outdoor temperature and conditions
- **Barometric Pressure** (with BME280): Atmospheric pressure in inHg

**Bottom Section**:
- **Mode Indicator**: Heat/Cool/Auto/Off with color coding
  - Red: Heating active
  - Blue: Cooling active
  - Orange: Auto mode
  - Gray: System off
- **Fan Status**: Shows when fan is running
- **Shower Mode Timer** (when active): Countdown display "ON for X min Y sec"

### Touch Controls

**Tap Mode Button (Bottom Left)**:
- Cycles through: Off → Heat → Cool → Auto → Off
- Current mode is highlighted

**Tap Fan Button (Bottom Center)**:
- Cycles through: Auto → On → Cycle → Auto
- **Auto**: Fan runs only with heating/cooling
- **On**: Fan runs continuously
- **Cycle**: Fan runs on schedule (configurable minutes per hour)

**Tap Setpoint (Center)**:
- Displays temperature adjustment controls
- **Up/Down arrows**: Adjust temperature in 0.5° increments
- **Range**: 50°F - 90°F (10°C - 32°C)
- **Auto mode**: Tap to switch between heat and cool setpoints

**Tap Setpoint When Shower Mode Enabled**:
- Toggles shower mode on/off
- Countdown timer replaces setpoint display when active
- Heating is paused until timer expires or manually turned off

### Visual Indicators

**WiFi Icon**:
- Connected: Solid icon
- Disconnected: Flashing icon
- No configuration: No icon

**MQTT Icon**:
- Connected: Cloud icon
- Disconnected: Cloud with X
- Disabled: No icon

**Active Relay Indicators**:
- Heat Stage 1: H1
- Heat Stage 2: H2
- Cool Stage 1: C1
- Cool Stage 2: C2
- Fan: FAN
- Reversing Valve: RV (for heat pumps)

---

## Web Interface

Access the web interface by navigating to your thermostat's IP address in any browser.

### Status Tab

**Real-Time Monitoring**:
- Current temperature and humidity with large display
- Thermostat mode and fan mode indicators
- System status showing all relay states
- Weather information (when configured)
- Hydronic water temperature (when enabled)

**Relay Status Indicators**:
- Each relay shows ON/OFF state
- Active relays are highlighted
- Stage 2 relays only shown when enabled in settings

### Settings Tab

**Basic Settings**:
- **Thermostat Mode**: Off/Heat/Cool/Auto
- **Fan Mode**: Auto/On/Cycle
- **Temperature Setpoints**: Separate values for Heat, Cool, and Auto modes
- **Temperature Swing**: Hysteresis setting (0.5° - 3.0°)
- **Auto Mode Swing**: Additional swing for Auto mode (1.0° - 5.0°)

**Shower Mode Settings**:
- **Enable Shower Mode**: Master enable/disable checkbox
- **Duration**: Set pause time from 5 to 120 minutes (default 30)
- When enabled, touch screen setpoint becomes shower mode toggle
- Countdown timer appears on display when active
- Heating automatically paused until timer expires

**Multi-Stage HVAC**:
- **Stage 2 Heating**: Enable/disable secondary heat
- **Stage 2 Cooling**: Enable/disable secondary cooling
- **Reversing Valve**: For heat pump systems (mutually exclusive with Stage 2 Heat)
- **Stage 1 Runtime**: Minimum time before activating Stage 2 (default 3 minutes)
- **Stage 2 Delta**: Temperature difference required for Stage 2 (default 2.0°)

**Fan Settings**:
- **Fan Relay Needed**: Enable if fan requires separate relay control
- **Fan Minutes Per Hour**: For Cycle mode, set how many minutes per hour fan runs (5-60)

**Hydronic Heating**:
- **Enable Hydronic Heating**: For radiant floor or baseboard systems
- **Low Temperature Threshold**: Minimum water temperature to enable heating (default 110°F)
- **High Temperature Threshold**: Maximum water temperature cutoff (default 140°F)
- Requires DS18B20 sensor connected to GPIO 4

**Sensor Calibration**:
- **Temperature Offset**: Correct sensor bias (-10° to +10°)
- **Humidity Offset**: Correct humidity sensor bias (-20% to +20%)

**Display Settings**:
- **Brightness**: Adjust TFT backlight (0-255, default 64)
- **Display Sleep**: Enable automatic dimming
- **Sleep Timeout**: Minutes of inactivity before sleep (1-60, default 5)
- **Use 24-Hour Clock**: Toggle 12/24 hour time format
- **Use Fahrenheit**: Toggle between °F and °C

**WiFi Settings**:
- **SSID**: Network name
- **Password**: Network password
- Changing WiFi requires reboot to take effect

**MQTT/Home Assistant**:
- **Enable MQTT**: Turn on Home Assistant integration
- **MQTT Server**: Broker IP address or hostname
- **MQTT Port**: Broker port (default 1883)
- **Username**: MQTT authentication username
- **Password**: MQTT authentication password
- **Time Zone**: Required for proper scheduling with Home Assistant

**Save Settings**:
- Click "Save All Settings" button at bottom
- Settings are immediately applied and saved to flash memory
- Some settings (WiFi, timezone) may require reboot

### Schedule Tab

**7-Day Scheduling System**:

Configure independent schedules for each day of the week.

**Day Schedule Table**:
- **Day Column**: Day of week (Sunday - Saturday)
- **Enable Column**: Toggle to enable/disable schedule for that day
- **Day Period Column**:
  - Time: Hour and minute for day period start (default 6:00 AM)
  - Active checkbox: Enable/disable this period
- **Day Temps Column**:
  - Heat: Target heating temperature
  - Cool: Target cooling temperature
- **Night Period Column**:
  - Time: Hour and minute for night period start (default 10:00 PM)
  - Active checkbox: Enable/disable this period
- **Night Temps Column**:
  - Heat: Target heating temperature
  - Cool: Target cooling temperature

**Schedule Control Section**:
- **Enable 7-Day Schedule**: Master on/off switch
- **Schedule Override**:
  - **Follow Schedule**: Normal operation, follow configured schedule
  - **Override for 2 Hours**: Temporarily use current setpoint, auto-resume after 2 hours
  - **Override Until Resumed**: Permanently use current setpoint until manually resumed

**Current Status Display**:
- Shows whether schedule is active
- Displays current active period (Day or Night)
- Indicates if override is active

**Using the Schedule**:
1. Click "Enable 7-Day Schedule" to activate
2. Configure times and temperatures for each day
3. Enable/disable individual days as needed
4. Enable/disable individual periods (day or night) for flexibility
5. Click "Save Schedule" to apply changes
6. Use override controls when you need temporary manual control

### Weather Tab

**Weather Source Selection**:
Choose between OpenWeatherMap or Home Assistant.

**OpenWeatherMap Configuration**:
1. Select "OpenWeatherMap" from Weather Source dropdown
2. Enter your API key (get free key at openweathermap.org)
3. Enter City, State/Province, and Country
4. Set update interval (5-60 minutes, default 15)
5. Click "Save Settings"

**Home Assistant Configuration**:
1. Select "Home Assistant" from Weather Source dropdown
2. Enter Home Assistant URL (e.g., http://192.168.1.100:8123)
3. Enter Long-Lived Access Token (create in HA profile settings)
4. Enter weather entity ID (e.g., weather.home)
5. Set update interval (5-60 minutes, default 15)
6. Click "Save Settings"

**Weather Display**:
- Weather appears on Status tab as a card
- Shows current temperature, conditions, and high/low
- Weather icon displays on touch screen
- Color-coded icons based on conditions

**Disabling Weather**:
- Select "Disabled" from Weather Source dropdown
- Weather information will be removed from display and Status tab

### System Tab

**System Information**:
- Firmware version and build date
- Device hostname
- WiFi network and IP address
- MAC address
- Free heap memory
- System uptime
- Flash size and chip model
- CPU frequency

**Firmware Update**:
1. Click "Select Firmware File (.bin)" button
2. Choose firmware .bin file from your computer
3. Click "Upload & Install" button
4. **Do not power off** during update
5. Progress bar shows upload and flash write progress
6. Device automatically reboots when complete
7. Page shows success message with new version number

**System Actions**:
- **Reboot Device**: Restart the thermostat (requires confirmation)
- **Factory Reset**: Restore all settings to defaults (requires confirmation)

---

## Home Assistant Integration

### Automatic Discovery

When MQTT is enabled, the thermostat automatically registers with Home Assistant:

1. Go to Settings tab in web interface
2. Enable MQTT
3. Configure MQTT broker details
4. Click "Save All Settings"
5. Thermostat will appear in Home Assistant Integrations

### Climate Entity

**Available in Home Assistant**:
- Current temperature and humidity
- Thermostat mode (off/heat/cool/auto)
- Fan mode (auto/on/cycle)
- Target temperature setpoints
- Current HVAC action (idle/heating/cooling/fan)

**Control from Home Assistant**:
- Change thermostat mode
- Adjust target temperature
- Switch fan modes
- View real-time status

### Additional Entities

**Motion Sensor** (if LD2410 connected):
- Binary sensor for motion detection
- Auto-discovery enabled
- Shows in Home Assistant as motion detector

**Barometric Pressure** (if BME280 used):
- Sensor entity showing atmospheric pressure
- Unit: inches of mercury (inHg)
- State class: measurement

**Shower Mode Switch** (if enabled in settings):
- Switch entity to toggle shower mode on/off
- Shows countdown status
- Auto-discovery when feature enabled
- Automatically removed when feature disabled

### MQTT Topics

**State Topics** (published by thermostat):
- `hostname/current_temperature`: Current temp reading
- `hostname/current_humidity`: Current humidity reading
- `hostname/target_temperature`: Current setpoint
- `hostname/mode`: Thermostat mode
- `hostname/fan_mode`: Fan mode
- `hostname/action`: Current HVAC action
- `hostname/availability`: Online/offline status
- `hostname/motion_detected`: Motion sensor state (true/false)
- `hostname/barometric_pressure`: Pressure in inHg
- `hostname/shower_mode`: Shower mode state (ON/OFF)
- `hostname/shower_time_remaining`: Minutes remaining in shower mode

**Command Topics** (subscribed by thermostat):
- `hostname/mode/set`: Set thermostat mode
- `hostname/fan_mode/set`: Set fan mode
- `hostname/target_temperature/set`: Set target temperature
- `hostname/shower_mode/set`: Toggle shower mode (ON/OFF)

---

## Shower Mode

### What is Shower Mode?

Shower Mode temporarily pauses heating to prevent the HVAC system from running while hot water is being used for showers. This is particularly useful in homes where running the furnace and water heater simultaneously can cause issues.

### Enabling Shower Mode

1. Go to **Settings** tab in web interface
2. Scroll to **Shower Mode Settings** section
3. Check **Enable Shower Mode** checkbox
4. Set **Duration** (5-120 minutes, default 30)
5. Click **Save All Settings**

Once enabled:
- Touch screen setpoint area becomes shower mode toggle
- Shower mode switch appears in Home Assistant (if MQTT enabled)
- Countdown timer replaces setpoint display when active

### Using Shower Mode

**Via Touch Screen**:
1. Tap the setpoint area in center of display
2. Shower mode activates immediately
3. Display shows "SHOWER MODE" and countdown timer
4. Tap again to cancel early
5. Heating automatically resumes when timer expires

**Via Home Assistant**:
1. Find "Shower Mode" switch entity
2. Toggle switch ON to activate
3. Switch shows remaining time
4. Toggle OFF to cancel early
5. Automatically turns OFF when timer expires

**Via Web Interface**:
- Shower mode status visible on Status tab
- Cannot be toggled directly from web interface
- Use touch screen or Home Assistant to control

### During Shower Mode

**What Happens**:
- Heating is completely blocked
- Countdown timer displays on screen: "ON for X min Y sec"
- Timer updates every second
- Cooling and fan operation are NOT affected
- Timer persists until completion (does not reset on reboot)

**5-Second Warning**:
- When 5 seconds remain, buzzer sounds once per second
- 5 total beeps: one for each remaining second
- Each beep is 100ms duration
- Helps remind you shower mode is about to end

**When Timer Expires**:
- Shower mode automatically deactivates
- Display returns to normal setpoint view
- Heating operation resumes normally
- Home Assistant switch turns OFF
- MQTT status updated

### Disabling Shower Mode Feature

To completely remove shower mode:
1. Go to **Settings** tab
2. Uncheck **Enable Shower Mode**
3. Click **Save All Settings**
4. Setpoint area returns to normal function
5. Shower mode switch removed from Home Assistant
6. Any active shower mode immediately cancelled

---

## Weather Integration

### Weather Sources

Choose between two weather data providers:

**OpenWeatherMap**:
- Free API available at openweathermap.org
- Requires API key registration
- Provides current conditions and forecast
- Updates via internet connection
- Best for most users

**Home Assistant**:
- Uses weather entity from your HA installation
- No separate API key needed
- Integrates with your existing HA weather provider
- Requires HA long-lived access token
- Best if you already use HA weather

### Setting Up OpenWeatherMap

1. **Get API Key**:
   - Visit openweathermap.org
   - Create free account
   - Go to API keys section
   - Copy your API key

2. **Configure Thermostat**:
   - Open web interface → Weather tab
   - Select "OpenWeatherMap" from dropdown
   - Paste API key
   - Enter city name (e.g., "Prairie Farm")
   - Enter state/province code (e.g., "WI")
   - Enter country code (e.g., "US")
   - Set update interval (default 15 minutes)
   - Click "Save Settings"

3. **Verify**:
   - Check Status tab for weather card
   - Touch screen should show weather icon and temperature
   - Wait for first update (may take up to interval time)

### Setting Up Home Assistant Weather

1. **Find Weather Entity**:
   - In Home Assistant, go to Developer Tools → States
   - Find your weather entity (starts with "weather.")
   - Copy the entity ID (e.g., "weather.home")

2. **Create Long-Lived Token**:
   - In Home Assistant, click your profile (bottom left)
   - Scroll to "Long-Lived Access Tokens"
   - Click "Create Token"
   - Give it a name (e.g., "Thermostat")
   - Copy the generated token

3. **Configure Thermostat**:
   - Open web interface → Weather tab
   - Select "Home Assistant" from dropdown
   - Enter HA URL (e.g., "http://192.168.1.100:8123")
   - Paste access token
   - Enter weather entity ID
   - Set update interval (default 15 minutes)
   - Click "Save Settings"

4. **Verify**:
   - Check Status tab for weather card
   - Touch screen should show weather icon and temperature
   - Updates should sync with HA weather

### Weather Display

**Status Tab Card**:
- Current temperature
- Weather description (e.g., "clear sky", "light rain")
- High and low temperatures (when available)
- Appears as a card in status grid

**Touch Screen Display**:
- Weather icon in corner
- Outdoor temperature
- Color-coded based on conditions:
  - Clear: Yellow
  - Cloudy: Gray
  - Rain: Blue
  - Snow: Light blue
  - Thunderstorm: Purple

### Update Intervals

**Configurable Range**: 5-60 minutes

**Recommended Settings**:
- 15 minutes: Good balance of freshness and API calls
- 5 minutes: For rapidly changing weather (uses more API calls)
- 30-60 minutes: For stable weather or limited API quota

**What Happens During Update**:
- Thermostat fetches new data from provider
- Display updates automatically
- Old data remains visible until new data received
- No interruption to HVAC operation

### Troubleshooting Weather

**Weather Not Showing**:
- Verify weather source is not set to "Disabled"
- Check that API key or access token is correct
- Ensure city/state/country are spelled correctly
- Verify internet connection is working
- Check serial debug output for error messages

**Weather Shows Old Data**:
- Confirm update interval is reasonable
- Check that provider service is online
- Verify API key has not expired
- For HA: ensure access token is still valid

**Wrong Location Weather**:
- Double-check city and state spelling
- Try using city name only without state
- For international locations, verify country code is correct (2-letter ISO code)

---

## Advanced Features

### Multi-Stage HVAC Operation

**How Staging Works**:

The thermostat intelligently controls 2-stage heating and cooling systems:

1. **Stage 1 Activation**:
   - When temperature crosses setpoint + swing threshold
   - Stage 1 relay activates immediately
   - Timer starts tracking Stage 1 runtime

2. **Stage 2 Activation**:
   - Requires Stage 1 to run for minimum time (Stage 1 Runtime setting)
   - Temperature must be Stage 2 Delta away from setpoint
   - Example: If setpoint is 68°F, Stage 2 Delta is 2°F, and it's heating:
     - Stage 2 activates when temp drops to 66°F or below
     - Only after Stage 1 has run for minimum runtime

3. **Staging Down**:
   - As temperature approaches setpoint, Stage 2 deactivates first
   - Stage 1 continues until temperature is within swing
   - Both stages deactivate when temperature reaches setpoint

**Configuration**:
- Enable Stage 2 Heating/Cooling in Settings tab
- Set Stage 1 Runtime: Minimum time before Stage 2 can activate (default 3 minutes)
- Set Stage 2 Delta: Temperature difference required for Stage 2 (default 2.0°)

**Benefits**:
- Prevents system short-cycling
- Optimizes energy efficiency
- Provides comfort with intelligent staging
- Protects HVAC equipment from excessive cycling

### Reversing Valve (Heat Pump)

For heat pump systems with reversing valve:

**Configuration**:
- Enable "Reversing Valve" in Settings tab
- **Note**: Mutually exclusive with Stage 2 Heating
- Reversing valve relay controls heating/cooling mode

**Operation**:
- Valve energized: Heat mode
- Valve de-energized: Cool mode
- Heat Relay 2 pin controls valve
- Automatically switches based on thermostat mode

### Hydronic Heating

For radiant floor or baseboard hydronic systems:

**Hardware Requirements**:
- DS18B20 temperature sensor on GPIO 4
- Connected to water supply or return line
- Properly sealed and waterproofed

**Configuration**:
1. Enable "Hydronic Heating" in Settings tab
2. Set Low Temperature Threshold (default 110°F)
3. Set High Temperature Threshold (default 140°F)
4. Click "Save Settings"

**Safety Operation**:
- Heating disabled if water temp < Low Threshold
- Heating disabled if water temp > High Threshold
- "BOILER LOCKOUT" warning displays on screen when locked out
- Prevents system from running with insufficient hot water
- Protects against overheating conditions

**Display**:
- Hydronic temperature shows on Status tab
- Displayed in °F regardless of main unit setting
- Updates in real-time
- Shows on touch screen when enabled

### Fan Cycle Mode

**Purpose**: Circulate air periodically even when HVAC is not running

**Configuration**:
1. Set Fan Mode to "Cycle" (touch screen or Settings tab)
2. Set "Fan Minutes Per Hour" in Settings (5-60 minutes)
3. Example: Setting of 15 means fan runs 15 minutes per hour

**Operation**:
- Fan runs at beginning of each hour
- Runs for configured number of minutes
- Automatically turns off after time elapses
- Resumes next hour on schedule
- **Boot Behavior**: First cycle is skipped after power-on/reboot to prevent immediate fan run

**Benefits**:
- Improves air circulation
- Helps even out temperature
- Reduces hot/cold spots
- Filters air even when HVAC off

### Display Sleep Mode

**Purpose**: Reduce power consumption and screen burn-in

**Configuration**:
1. Enable "Display Sleep" in Settings tab
2. Set timeout in minutes (1-60, default 5)
3. Display dims after inactivity period

**Behavior**:
- Touch activity resets timeout timer
- Motion detection (if LD2410 connected) wakes display
- Display dims to configured brightness level when sleeping
- Full brightness restored on wake

**When to Use**:
- Extended periods without interaction
- Reduce power consumption
- Minimize screen wear
- Less light pollution at night

### Motion Detection Wake

**Hardware**: LD2410 24GHz mmWave radar sensor

**Connections**:
- RX: GPIO 15
- TX: GPIO 16
- Motion Output: GPIO 18
- Power: 5V and GND

**Operation**:
- Detects motion via mmWave radar
- Automatically wakes display from sleep
- Publishes motion state to Home Assistant (if MQTT enabled)
- Works even if sensor doesn't respond to UART commands

**Configuration**:
- No setup required - auto-detected on boot
- Motion sensor appears in Home Assistant automatically
- Display wake happens instantaneously
- No false triggers from temperature changes

---

## Troubleshooting

### WiFi Connection Issues

**Can't Connect to Network**:
- Verify SSID and password are correct (case-sensitive)
- Check that network is 2.4GHz (ESP32 doesn't support 5GHz)
- Move thermostat closer to router temporarily
- Check router for MAC address filtering
- Verify router has available DHCP addresses
- Try resetting WiFi credentials via boot button

**Intermittent Disconnections**:
- Check WiFi signal strength icon on display
- Move router closer or add WiFi extender
- Check for interference from other 2.4GHz devices
- Verify router is not disconnecting idle devices
- Check router logs for connection drops

### Display Issues

**Touch Not Responding**:
- Calibrate touch screen through web interface
- Clean display surface - dirt affects capacitive touch
- Check for physical damage to touch layer
- Verify display cable connections
- Try factory reset if calibration fails

**Display Flickering**:
- Normal during updates (weather, time changes)
- Excessive flickering may indicate power issue
- Check 5V power supply is stable and adequate (2A minimum)
- Verify all display cable connections are secure

**Display Shows Garbage**:
- Power cycle the thermostat
- Check display cable for damage
- Verify SPI connections are correct
- May indicate failing display - consider replacement

**Display Stays Dim**:
- Check Display Sleep setting - may be set too low
- Adjust Brightness setting in Settings tab
- Motion sensor may not be detecting movement
- Try tapping screen to wake

### Sensor Issues

**Temperature Reading Incorrect**:
- Use Temperature Offset in Settings to calibrate
- Verify sensor is not in direct sunlight or airflow
- Check sensor is properly connected to I2C pins
- Allow 15-30 minutes for sensor to stabilize after power-on
- Consider sensor placement - away from heat sources

**Humidity Reading Incorrect**:
- Use Humidity Offset in Settings to calibrate
- Humidity sensors drift over time - recalibrate annually
- AHT20 typically accurate ±2-3%
- Verify sensor is not obstructed or covered

**No Sensor Detected**:
- Check I2C connections (SDA: GPIO 8, SCL: GPIO 9)
- Verify sensor power (3.3V)
- Check pull-up resistors on I2C lines
- Try different sensor if available
- Check serial debug for I2C scan results

**Hydronic Temp Not Showing**:
- Verify DS18B20 is connected to GPIO 4
- Check OneWire connections and 4.7K pull-up resistor
- Ensure sensor is properly sealed and waterproofed
- Enable Hydronic Heating in Settings
- Check serial debug for DS18B20 detection

### MQTT/Home Assistant Issues

**Thermostat Not Appearing in HA**:
- Verify MQTT is enabled in thermostat settings
- Check MQTT broker IP and port are correct
- Verify MQTT username and password
- Check broker is running and accessible
- Look in HA MQTT integration for discovery messages
- Check HA Configuration → Integrations for pending setup

**Thermostat Shows "Unavailable" in HA**:
- Check MQTT connection status icon on display
- Verify broker is running
- Check network connectivity
- Restart MQTT broker
- Restart Home Assistant
- Check thermostat is powered on and connected to WiFi

**Commands from HA Don't Work**:
- Verify thermostat is subscribed to command topics
- Check MQTT Explorer or similar tool for message flow
- Ensure topics match HA climate entity configuration
- Try sending commands directly via MQTT tool
- Check serial debug for received MQTT messages

**Motion Sensor Not Showing in HA**:
- Verify LD2410 is connected and detected
- Check MQTT is enabled
- Motion discovery sent only on boot - restart thermostat
- Check HA MQTT integration for sensor entity
- Verify motion detection is working on display

### Heating/Cooling Issues

**System Not Heating/Cooling**:
- Check thermostat mode is correct (Heat/Cool/Auto)
- Verify setpoint is appropriate (below temp for cool, above for heat)
- Check all relay connections to HVAC system
- Test relays manually to verify they activate
- Check HVAC system breakers and power
- Verify HVAC system works with old thermostat
- Check serial debug for relay activation messages

**Short Cycling**:
- Increase Temperature Swing setting (try 1.5° or 2.0°)
- Verify Stage 1 Runtime is adequate (minimum 3 minutes)
- Check for proper system sizing
- Ensure air filters are clean
- Verify HVAC system is functioning correctly

**Stage 2 Never Activates**:
- Verify Stage 2 is enabled in settings
- Check Stage 1 Runtime setting (must run before Stage 2)
- Increase Stage 2 Delta if Stage 2 activating too late
- Check that Stage 2 relay is connected
- Verify system has Stage 2 capability

**Hydronic Lockout**:
- Check water temperature via Status tab
- Verify DS18B20 sensor is working correctly
- Check boiler is running and heating water
- Adjust Low Temperature Threshold if too high
- Ensure circulation pump is running
- Check for air in hydronic system

### Schedule Issues

**Schedule Not Running**:
- Verify "Enable 7-Day Schedule" is turned on
- Check that current day is enabled in schedule table
- Verify at least one period (day or night) is active
- Ensure times are set correctly (24-hour format)
- Check Time Zone is configured correctly in settings
- Verify current time display is accurate

**Wrong Temperature at Scheduled Time**:
- Check which period (day or night) should be active
- Verify period transition time is correct
- Ensure period Active checkbox is checked
- Check Heat/Cool temperature values for that period
- Verify thermostat mode matches schedule intent

**Override Not Working**:
- Select "Override for 2 Hours" or "Override Until Resumed"
- Adjust setpoint via touch screen to set override temperature
- Check schedule override status on Schedule tab
- Verify "Follow Schedule" is not selected

### Weather Issues

See [Weather Integration](#weather-integration) section for detailed troubleshooting.

### General Troubleshooting

**System Unresponsive**:
- Power cycle the thermostat (unplug and replug)
- Check for adequate power supply (5V, 2A minimum)
- Try factory reset via boot button
- Check serial debug output for errors
- Verify firmware is not corrupted

**Factory Reset**:
1. Press and hold boot button (10+ seconds)
2. Release when display shows reset confirmation
3. All settings restored to defaults
4. WiFi credentials cleared
5. Reconfigure from scratch via touch screen

**Firmware Update Failed**:
- Ensure .bin file is valid firmware for ESP32-S3
- Try uploading again - check progress bar reaches 100%
- **Do not power off during update**
- Check web browser console for JavaScript errors
- Use wired connection if WiFi is unstable
- Try different browser if problems persist

---

## Maintenance

### Regular Maintenance

**Monthly**:
- Check temperature accuracy with known good thermometer
- Verify humidity reading is reasonable
- Test all thermostat modes (Heat/Cool/Auto/Off)
- Test all fan modes (Auto/On/Cycle)
- Clean display with soft, slightly damp cloth

**Every 3 Months**:
- Recalibrate temperature if drifting (use offset setting)
- Check for firmware updates
- Review and adjust schedule as seasons change
- Test HVAC system operation in all modes
- Check MQTT/Home Assistant integration still working

**Annually**:
- Recalibrate humidity sensor (can drift over time)
- Review all settings and adjust for seasonal changes
- Test factory reset procedure (in non-critical period)
- Back up schedule configuration (take screenshots)
- Clean around thermostat and sensors
- Check all wiring connections are secure

### Backup and Restore

**Manual Backup**:
The thermostat does not have automatic config export, but you can manually document:
1. Take screenshots of all Settings tab pages
2. Screenshot complete Schedule tab
3. Note MQTT settings and credentials
4. Document WiFi configuration
5. Record any calibration offsets

**Restore After Reset**:
1. Factory reset via boot button or web interface
2. Connect to WiFi via touch screen
3. Access web interface
4. Restore settings from screenshots
5. Reconfigure MQTT if needed
6. Restore schedule configuration
7. Test all functionality

### Firmware Updates

**Check for Updates**:
- Visit GitHub repository releases page
- Check current version: System tab or touch screen
- Review changelog for new features and fixes
- Download appropriate .bin file for your ESP32 variant

**Update Procedure**:
1. Navigate to System tab in web interface
2. Click "Select Firmware File (.bin)"
3. Choose downloaded .bin file
4. Click "Upload & Install"
5. Wait for upload progress (do not refresh page)
6. Wait for flash write progress
7. Device reboots automatically
8. Verify new version on System tab

**Update Best Practices**:
- Perform updates during non-critical times
- Ensure stable power supply
- Use wired connection if possible
- Don't interrupt update process
- Keep old firmware .bin file in case rollback needed
- Test all functions after update

### When to Contact Support

Contact via GitHub Issues if you experience:
- Persistent crashes or reboots
- Hardware failures not resolved by troubleshooting
- Firmware bugs or unexpected behavior
- Feature requests or enhancement ideas
- Questions not answered in this manual

**Information to Provide**:
- Firmware version
- Hardware variant (N4/N8/N16/N32)
- Detailed description of problem
- Steps to reproduce issue
- Serial debug output if possible
- Screenshots if relevant

---

## Safety Information

### Electrical Safety

- **High Voltage Warning**: HVAC systems operate at 24VAC or higher
- **Turn off power** at breaker before wiring thermostat
- **Verify power is off** with multimeter before touching wires
- Follow local electrical codes and regulations
- Hire licensed electrician if unsure about wiring
- Keep thermostat and wiring away from water sources

### Installation Safety

- Mount thermostat on interior wall away from:
  - Direct sunlight
  - Heat sources (radiators, lamps, appliances)
  - Drafts (doors, windows, vents)
  - Moisture sources (bathrooms, kitchens)
- Ensure adequate ventilation around thermostat
- Use appropriate mounting hardware for wall type
- Keep cables organized and secured

### Operation Safety

- Do not cover thermostat or block airflow around sensors
- Keep thermostat clean and dry
- Do not operate with damaged display or housing
- Replace thermostat if showing signs of damage
- Unplug during electrical storms if possible

### Child Safety

- Install at appropriate height (48-52 inches recommended)
- Consider using Schedule and Home Assistant control to prevent manual changes
- Touch screen is not dangerous but should be supervised
- Factory reset requires 10-second button hold (prevents accidental reset)

---

## Appendix

### Default Settings

- Temperature Unit: Fahrenheit
- Heat Setpoint: 68°F
- Cool Setpoint: 75°F
- Auto Setpoint: 70°F
- Temperature Swing: 1.0°
- Auto Mode Swing: 1.5°
- Fan Mode: Auto
- Stage 1 Runtime: 3 minutes
- Stage 2 Delta: 2.0°
- Fan Minutes Per Hour: 15
- Shower Mode: Disabled
- Shower Duration: 30 minutes
- Display Brightness: 64
- Display Sleep: Enabled
- Sleep Timeout: 5 minutes
- Temperature Offset: 0.0°
- Humidity Offset: 0%
- Use 24-Hour Clock: Yes

### GPIO Pin Assignments

**Display (SPI)**:
- MOSI: GPIO 11
- MISO: GPIO 13
- SCK: GPIO 12
- CS: GPIO 10
- DC: GPIO 7
- RST: GPIO 6
- Backlight: GPIO 5

**Touch (SPI)**:
- CS: GPIO 21
- IRQ: GPIO 3

**I2C (Sensors)**:
- SDA: GPIO 8
- SCL: GPIO 9

**OneWire (DS18B20)**:
- Data: GPIO 4

**LD2410 Motion Sensor**:
- RX: GPIO 15
- TX: GPIO 16
- Motion: GPIO 18

**Relays**:
- Heat Relay 1: GPIO 1
- Heat Relay 2: GPIO 2
- Cool Relay 1: GPIO 47
- Cool Relay 2: GPIO 48
- Fan Relay: GPIO 45

**User Interface**:
- Buzzer: GPIO 46
- Boot Button: GPIO 0 (built-in)

**Light Sensor**:
- LDR (analog): GPIO 14

### Specifications

**Processor**: ESP32-S3-WROOM-1
- Dual-core Xtensa LX7 @ 240MHz
- 512KB SRAM
- 384KB ROM
- Flash: 4MB/8MB/16MB/32MB variants

**Display**: ILI9341 TFT LCD
- Resolution: 320x240 pixels
- Colors: 65K (16-bit)
- Touch: XPT2046 resistive
- Interface: SPI

**Sensors**:
- AHT20: ±0.3°C, ±2% RH
- BME280: ±1.0°C, ±3% RH, ±1 hPa (optional)
- DS18B20: ±0.5°C (optional)
- LD2410: 24GHz mmWave (optional)

**Connectivity**:
- WiFi: 802.11 b/g/n (2.4GHz)
- MQTT: Version 3.1.1
- Web: HTTP/HTTPS

**Power**:
- Input: 5V DC
- Current: 500mA typical, 1A max
- Idle: ~200mA
- Active: ~400mA

**Operating Range**:
- Temperature: 32°F - 122°F (0°C - 50°C)
- Humidity: 20% - 80% RH
- Control Range: 50°F - 90°F (10°C - 32°C)

**Dimensions** (with case):
- 149mm × 105.5mm × 47mm (W × H × D)
- Weight: ~150g

---

## Quick Reference

### Common Touch Screen Actions

| Action | Result |
|--------|--------|
| Tap Mode Button | Cycle: Off → Heat → Cool → Auto |
| Tap Fan Button | Cycle: Auto → On → Cycle |
| Tap Setpoint | Adjust temperature |
| Tap Setpoint (Shower Mode enabled) | Toggle shower mode on/off |
| Long-press Boot Button (10s) | Factory reset |

### Web Interface Quick Links

| URL | Purpose |
|-----|---------|
| `http://[IP]` | Main status page |
| `http://[IP]/settings` | Settings page (redirects to main) |
| `http://[IP]/schedule` | Schedule page (redirects to main) |
| `http://[IP]/weather` | Weather page (redirects to main) |
| `http://[IP]/system` | System page (redirects to main) |

### MQTT Topic Quick Reference

| Topic | Direction | Purpose |
|-------|-----------|---------|
| `hostname/current_temperature` | → HA | Current temp |
| `hostname/target_temperature` | → HA | Setpoint |
| `hostname/mode` | → HA | Thermostat mode |
| `hostname/mode/set` | HA → | Set mode |
| `hostname/fan_mode` | → HA | Fan mode |
| `hostname/fan_mode/set` | HA → | Set fan mode |
| `hostname/target_temperature/set` | HA → | Set setpoint |
| `hostname/shower_mode` | → HA | Shower status |
| `hostname/shower_mode/set` | HA → | Toggle shower |
| `hostname/shower_time_remaining` | → HA | Minutes left |

---

**End of User Manual**

For additional support, visit the GitHub repository or contact support through GitHub Issues.

Project Repository: https://github.com/jrtaylor71/ESP32-DevKitC3-Simple-Thermostat-PCB

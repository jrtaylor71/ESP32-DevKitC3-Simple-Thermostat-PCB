# MQTT Schedule Synchronization Guide

**Version 1.4.001 | January 2026**

Complete documentation for bidirectional MQTT schedule synchronization between ESP32 thermostats and Home Assistant.

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [MQTT Topics & Payloads](#mqtt-topics--payloads)
4. [Setup Instructions](#setup-instructions)
5. [Troubleshooting](#troubleshooting)
6. [Advanced Configuration](#advanced-configuration)

---

## Overview

The thermostat system implements **bidirectional schedule synchronization** with Home Assistant:

- **Device → HA (Inbound)**: Device publishes schedule state to MQTT, HA automations create/update helpers
- **HA → Device (Outbound)**: Changing HA helpers triggers automations that send MQTT commands to device
- **Multi-Device Support**: Single automation handles unlimited thermostats
- **Zero Conflicts**: Proper hostname normalization prevents helper ID collisions
- **Automatic Generation**: Scripts generate all 77 automations and helpers per device

### Key Benefits

✅ **Zero Manual Sync**: Changes automatically sync bidirectionally  
✅ **Full HA Control**: Edit schedules in HA interface  
✅ **Device Control**: Changes on device auto-update in HA  
✅ **Scalable**: Add as many thermostats as needed  
✅ **Fault Tolerant**: No circular updates or conflicts  

---

## Architecture

### System Overview

```
┌─────────────────────────────────────────────────────────────┐
│                   Home Assistant                             │
│  ┌──────────────┐              ┌──────────────────┐         │
│  │   77 x3      │              │  Multi-Thermostat│         │
│  │  Helpers     │◄───────┬────►│  Schedule Sync   │         │
│  │ per device   │        │     │  Automation (1)  │         │
│  └──────────────┘        │     └──────────────────┘         │
│        ▲                 │              ▲                    │
│        │                 │              │                    │
│   77 outbound      1 centralized   Condition               │
│ automations per    inbound for all Check: valid day,│
│   thermostat         devices        host, payload    │
└────────┼──────────────────┼───────────────────────────────┘
         │                  │
      [MQTT]            [MQTT]
         │                  │
┌────────▼──────────────────▼────────────────────────────────┐
│                    MQTT Broker                              │
│                                                              │
│  {hostname}/schedule/set ◄──────────────────────────┐      │
│  {hostname}/schedule/{day} ────────────────────────►│      │
│                                                      │      │
└──────────────────────────┬───────────────────────────┘
                           │
                      [Inbound]
                       [Outbound]
                           │
                ┌──────────▼────────────┐
                │   ESP32 Thermostat   │
                │  weekSchedule[7]     │
                │  Parse JSON payloads │
                │  Publish state       │
                └─────────────────────┘
```

### Data Flow

#### 1. Device Startup → HA

1. Device boots and connects to MQTT broker
2. Device publishes complete schedule to 7 topics: `/schedule/sunday` through `/schedule/saturday`
3. HA inbound automation triggers for each topic
4. Automation creates or updates all 77 helpers with current device state
5. User sees helpers in HA with correct values

#### 2. User Changes Helper in HA → Device

1. User changes `input_datetime.shop_thermostat_friday_day_time` to 07:00
2. Outbound automation triggers (state changed)
3. Automation publishes to `Shop-Thermostat/schedule/set`:
   ```json
   {"day": 4, "period": "day", "hour": 7, "minute": 0}
   ```
4. Device receives and updates `weekSchedule[5].day.hour = 7` (day=5 is Friday in array)
5. Device saves to flash
6. Device publishes updated state to `/schedule/friday`
7. Inbound automation updates HA helpers to match device
8. No feedback loop - safe bidirectional sync

#### 3. User Changes Device Schedule → HA Updates

1. User touches device screen and changes Friday morning to 7:00
2. Device updates `weekSchedule` array
3. Device saves to flash
4. Device publishes `{hostname}/schedule/friday` with new state
5. HA inbound automation receives and updates all Friday helpers
6. HA UI reflects new schedule
7. Users see changes immediately

---

## MQTT Topics & Payloads

### Device → HA (State Topics)

#### Schedule State Topics (Inbound)

**Topic Pattern**: `{hostname}/schedule/{day_name}`

**Example**: 
- `Shop-Thermostat/schedule/monday`
- `Shop-Thermostat/schedule/friday`
- `Studio-Thermostat/schedule/sunday`

**Payload Format**:
```json
{
  "day_index": 0,
  "day_name": "Monday",
  "is_today": false,
  "schedule_enabled": false,
  "day_enabled": true,
  "day_period": {
    "time": "6:00",
    "heat": 72.0,
    "cool": 76.0,
    "auto": 74.0,
    "active": true
  },
  "night_period": {
    "time": "22:00",
    "heat": 68.0,
    "cool": 78.0,
    "auto": 73.0,
    "active": true
  }
}
```

**Field Descriptions**:
- `day_index`: 0=Monday through 6=Sunday (MQTT format)
- `day_name`: Display name of the day
- `is_today`: True if this is the current day
- `schedule_enabled`: Master schedule enable flag
- `day_enabled`: Enable flag for this specific day
- `day_period.time`: Start time in "HH:MM" format (24-hour)
- `day_period.heat`: Heating temperature setpoint
- `day_period.cool`: Cooling temperature setpoint
- `day_period.auto`: Auto mode temperature setpoint
- `day_period.active`: Whether day period is active

**Publish Frequency**: On boot, on configuration change, on schedule update via MQTT

**Retained**: False (fresh updates only)

### HA → Device (Command Topics)

#### Schedule Set Command Topic (Outbound)

**Topic**: `{hostname}/schedule/set`

**Example**:
- `Shop-Thermostat/schedule/set`
- `Studio-Thermostat/schedule/set`

**Payload Format** (minimal - only changed fields):
```json
{"day": 4, "period": "day", "hour": 7, "minute": 0}
```

**All Possible Fields**:
```json
{
  "day": 0-6,
  "period": "day" | "night",
  "hour": 0-23,
  "minute": 0-59,
  "heat": float,
  "cool": float,
  "auto": float,
  "active": true | false,
  "enabled": true | false
}
```

**Field Descriptions**:
- `day`: 0=Monday, 1=Tuesday, ..., 6=Sunday (MQTT format)
- `period`: "day" or "night"
- `hour`: Hour (24-hour format)
- `minute`: Minute
- `heat`: Heating setpoint (only include if changing)
- `cool`: Cooling setpoint (only include if changing)
- `auto`: Auto mode setpoint (only include if changing)
- `active`: Whether period is active (only include if changing)
- `enabled`: Whether day is enabled (only include if changing)

**Example Payloads**:

Change Friday morning time to 7:00:
```json
{"day": 4, "period": "day", "hour": 7, "minute": 0}
```

Change Friday morning heat temperature to 75:
```json
{"day": 4, "period": "day", "heat": 75}
```

Disable Monday:
```json
{"day": 0, "enabled": false}
```

Enable Tuesday night period:
```json
{"day": 1, "period": "night", "active": true}
```

### MQTT Hostname Format

**Device Publishes As**: `{hostname}` (capitalized, hyphenated)
- Example: `Shop-Thermostat`, `Studio-Thermostat`, `House-Thermostat`

**Helper IDs Use**: `{hostname_lowercase}` (lowercase, underscored)
- Example: `shop_thermostat`, `studio_thermostat`, `house_thermostat`

**Generated Automations**: Use MQTT hostname in topic, helper IDs in services

---

## Setup Instructions

### Prerequisites

- MQTT broker running and accessible to both HA and thermostat
- Thermostat on same network as HA and MQTT broker
- HA with automations enabled
- Git access to generate scripts

### Step 1: Enable MQTT on Thermostat

1. Access thermostat web interface
2. Go to Settings → MQTT tab
3. Enable MQTT checkbox
4. Enter MQTT broker IP address
5. Enter port (default 1883)
6. Enter optional username/password
7. Click "Save All Settings"
8. Verify "MQTT" shows connected status on main display

### Step 2: Prepare Home Assistant

1. Locate HA packages directory:
   ```bash
   # Typical paths:
   /config/packages/           # Docker
   ~/.homeassistant/packages/  # Native install
   /usr/share/hassio/config/packages/  # HassIO
   ```

2. Copy HA automation files:
   ```bash
   cp multi_thermostat_schedule_sync.yaml /path/to/HA/packages/
   ```

### Step 3: Generate Per-Device Packages

For each thermostat, generate the schedule package:

```bash
cd /home/jonnt/Documents/ESP32-DevKitC3-Simple-Thermostat-PCB

# For your thermostats:
./generate_schedule_package.sh shop_thermostat
./generate_schedule_package.sh studio_thermostat
./generate_schedule_package.sh house_thermostat
```

This creates:
- `shop_thermostat_schedule.yaml` (77 automations + 77 helpers)
- `studio_thermostat_schedule.yaml` (another 77 automations + 77 helpers)
- etc.

Copy generated files to HA packages directory:
```bash
cp *_schedule.yaml /path/to/HA/packages/
```

### Step 4: Add to HA Configuration

In `configuration.yaml`:
```yaml
homeassistant:
  packages:
    multi_thermostat_sync: !include packages/multi_thermostat_schedule_sync.yaml
    shop_thermostat_schedule: !include packages/shop_thermostat_schedule.yaml
    studio_thermostat_schedule: !include packages/studio_thermostat_schedule.yaml
    house_thermostat_schedule: !include packages/house_thermostat_schedule.yaml
```

### Step 5: Reload in Home Assistant

1. Go to Developer Tools > YAML
2. Click "Reload Automations"
3. Wait for success message
4. Restart Home Assistant (or use Developer Tools > Services > homeassistant.restart)

### Step 6: Verify Setup

1. Go to Settings > Devices & Services > Helpers
2. Search for thermostat hostname (e.g., "shop_thermostat")
3. Verify 77 helpers appear:
   - 14 `input_datetime` (day/night times × 7 days)
   - 42 `input_number` (heat/cool/auto × day/night × 7 days)
   - 21 `input_boolean` (enabled × day/night+day × 7 days)

4. Check helper values match device schedule
5. Try changing one helper and verify device updates within 1-2 seconds

---

## Troubleshooting

### Helpers Not Appearing

**Problem**: After setup, helpers don't appear in HA

**Solutions**:
1. Verify MQTT is connected:
   - Check thermostat display for MQTT icon (should show cloud)
   - Check Settings tab for "MQTT Connected" status

2. Check automations loaded:
   - Go to Settings > Automations & Scenes > Automations
   - Search for "Schedule - Sync All Thermostats from MQTT"
   - If missing, reload automations again

3. Verify MQTT topics:
   - Use MQTT client to monitor `+/schedule/+` wildcard
   - Device should publish to `/schedule/monday`, `/schedule/tuesday`, etc.
   - Check payload is valid JSON

4. Check HA logs:
   - Settings > System > Logs
   - Look for automation errors
   - Check for JSON parsing issues

**Debug Steps**:
```bash
# Monitor MQTT topics
mosquitto_sub -h <broker_ip> -t "+/schedule/+" -v

# Should show:
# Shop-Thermostat/schedule/monday {...json...}
# Shop-Thermostat/schedule/tuesday {...json...}
# etc.
```

### Changes Not Syncing Device → HA

**Problem**: Device schedule changes, but HA helpers don't update

**Solutions**:
1. Verify device is publishing:
   - Monitor MQTT: `mosquitto_sub -h <broker_ip> -t "+/schedule/+" -v`
   - Make change on device
   - Check if topic receives update

2. Verify automation is running:
   - Add test helper to automation
   - Check if it updates when publishing to MQTT manually

3. Check automation condition:
   - Edit automation in HA UI
   - Verify hostname is in whitelist (shop_thermostat, studio_thermostat)
   - Verify day name is valid (monday...sunday)

### Changes Not Syncing HA → Device

**Problem**: Changing HA helper doesn't update device schedule

**Solutions**:
1. Verify outbound automation exists:
   - Go to Settings > Automations & Scenes > Automations
   - Search for the specific helper (e.g., "Friday Morning Time Changed")
   - If missing, regenerate package

2. Verify automation is enabled:
   - Click automation to view
   - Check toggle is "On" (not disabled)

3. Monitor MQTT:
   - Change helper in HA
   - Watch MQTT for publish to `{hostname}/schedule/set`
   - If no publish, automation didn't trigger

4. Check device payload:
   - Monitor device logs (115200 baud via serial)
   - Look for "SCHEDULE: Via MQTT, updated day X period Y"
   - Check if day value is correct

### Circular Updates / Feedback Loop

**Problem**: Changes keep bouncing between device and HA

**This should not happen!** The system is designed to prevent this:
- Outbound automation publishes once when helper changes
- Inbound automation publishes once when device state changes
- No feedback loop because changes stabilize after 1-2 seconds

**If happening, check**:
1. Only ONE copy of `multi_thermostat_schedule_sync.yaml` loaded
2. No duplicate outbound automations for same helper
3. MQTT QoS is 0 (not 1 or 2)
4. No manual automations publishing to schedule topics

### Day Off-by-One Errors

**Problem**: Changing Friday updates Thursday, or vice versa

**This is fixed in v1.4.001!** Day index conversion is now correct:
- MQTT format: 0=Monday, 1=Tuesday, ..., 6=Sunday
- Device array: 0=Sunday, 1=Monday, ..., 6=Saturday
- Conversion: `arrayDay = (mqttDay + 1) % 7`

**Verify fix is applied**:
1. Firmware version should be v1.4.001 or later (check System tab)
2. If older, rebuild and flash latest firmware
3. Test: change Friday helper, verify Friday updates on device

### Wrong Device Responding

**Problem**: Change in HA updates wrong thermostat

**Cause**: Usually hostname mismatch between MQTT topic and HA helpers

**Fix**:
1. Check thermostat hostname:
   - Device web interface Settings > General
   - Top of main display shows IP and hostname
   - MQTT discovery message includes hostname

2. Verify hostname format:
   - Helper IDs: lowercase_with_underscores
   - MQTT topics: CamelCase-With-Hyphens
   - Device name: shown on display/web interface

3. Regenerate package with correct hostname:
   ```bash
   ./generate_schedule_package.sh <exact_hostname>
   ```

---

## Advanced Configuration

### Adding New Thermostat

To add a new thermostat to existing setup:

1. Set thermostat hostname (web interface > Settings)
2. Enable MQTT and connect
3. Verify it appears in MQTT topics
4. Generate schedule package:
   ```bash
   ./generate_schedule_package.sh new_thermostat_name
   ```
5. Copy generated YAML to HA packages
6. Add to `configuration.yaml`:
   ```yaml
   homeassistant:
     packages:
       new_thermostat_schedule: !include packages/new_thermostat_name_schedule.yaml
   ```
7. Reload automations in HA
8. Verify helpers appear

### Customizing Automation Names

Edit `template_thermostat_schedule.yaml` before generating packages:

```yaml
# Before:
- alias: "Thermostat - {{ dayname }} Morning Time Changed"

# After (custom):
- alias: "{{ hostname | title }} - {{ dayname }} Morning Schedule Time"
```

Then regenerate packages.

### Integration with Automations

Use schedule helpers in your own automations:

```yaml
# Example: Notify when schedule changes
automation:
  - alias: "Notify on Schedule Change"
    trigger:
      platform: state
      entity_id:
        - input_datetime.shop_thermostat_monday_day_time
        - input_datetime.shop_thermostat_friday_night_time
    action:
      service: notify.mobile_app
      data:
        message: "Schedule changed: {{ state_attr(trigger.entity_id, 'friendly_name') }}"
```

### MQTT Topic Monitoring

Monitor all schedule activity:

```bash
# All schedule topics
mosquitto_sub -h broker.ip -t "+/schedule/+" -v

# Specific thermostat
mosquitto_sub -h broker.ip -t "Shop-Thermostat/schedule/#" -v

# Only set commands (inbound to device)
mosquitto_sub -h broker.ip -t "+/schedule/set" -v
```

---

## Version History

### v1.4.001 (January 2026)
- ✅ Fixed day index off-by-one bug (MQTT 0=Monday vs firmware 0=Sunday)
- ✅ Multi-thermostat support with hostname normalization
- ✅ 77 outbound automations per device
- ✅ Centralized inbound automation for all devices
- ✅ Complete bidirectional sync
- ✅ Automatic helper generation via script

### v1.3.9 and earlier
- 7-day scheduling system in firmware
- MQTT discovery for climate entity
- Web interface schedule tab
- Initial MQTT implementation

---

## References

- **Firmware Code**: [Main-Thermostat.cpp](src/Main-Thermostat.cpp#L2960) - Schedule MQTT handler
- **Inbound Automation**: [multi_thermostat_schedule_sync.yaml](multi_thermostat_schedule_sync.yaml)
- **Outbound Template**: [template_thermostat_schedule.yaml](template_thermostat_schedule.yaml)
- **Generator Script**: [generate_schedule_package.sh](generate_schedule_package.sh)

---

**Last Updated**: January 2026  
**Author**: Thermostat Development Team  
**License**: GNU General Public License v3.0

# ESP32 Simple Thermostat – Home Assistant integration

Native Home Assistant integration for the ESP32 Simple Thermostat. It talks to the
thermostat's local HTTP API (v1) and receives live updates over Server-Sent Events,
so no MQTT broker or cloud is needed. Requires Home Assistant 2024.11 or newer.

## Installation

**HACS (recommended)**
1. HACS → ⋮ → *Custom repositories* → add
   `https://github.com/jrtaylor71/ESP32-DevKitC3-Simple-Thermostat-PCB`, category *Integration*.
2. Install **ESP32 Simple Thermostat** and restart Home Assistant.

**Manual**
Copy `custom_components/esp32_thermostat` into `<config>/custom_components/` and restart.

## Thermostat preparation

1. Enable the **Home Assistant API** in the thermostat settings (touch screen or web UI).
2. Note the **API token** (32 hex characters). It is shown on the device under
   *System Info → HA API* and in the web UI *Settings* tab.

## Setup

- **Discovery:** thermostats with the API enabled advertise `_esp32thermostat._tcp`
  over mDNS and appear under *Settings → Devices & services → Discovered*. Click
  *Configure* and enter the token.
- **Manual:** *Add integration → ESP32 Simple Thermostat*, enter host/IP and token.
- A changed token triggers a re-authentication prompt; a changed IP is picked up by
  discovery or via *Reconfigure*.

Entities are created based on the hardware features the thermostat reports
(humidity, pressure, air quality, hydronic, stage 2, shower mode, …). Diagnostic
entities (relays, Wi-Fi signal, uptime, free memory) are disabled by default.

When the thermostat's configuration changes on the device (features, unit, hostname, …)
the integration notices it (`info_rev`) and reloads itself automatically after ~5 s, so
entities and units follow the device.

### Device settings (configuration entities)

All of these are always created under the device's *Configuration* section and are
written to the thermostat immediately. Conflicting combinations (e.g. stage 2 heat
together with a heat pump, or a relay already used by another stage) are rejected by
the thermostat and shown as an error.

| Type | Entities |
|---|---|
| Switch | Stage 2 heating, Stage 2 cooling, Heat pump, Fan with heat, Fan with cool, Backup heat, Hydronic heat, EU humidity control, Shower mode feature |
| Select | Backup heat relay (pump / heat stage 2 / cool stage 2), EU humidity relay, Temperature unit (°F/°C), Region (US/EU), Time zone (US, UK, Central Europe, Japan, Australia East presets) |
| Number | Backup heat delay (min), Backup heat minimum rise, Backup heat maximum drop, EU humidity deadband (%), Temperature offset, Humidity offset (%) |
| Number (controls) | Heat setpoint, Cool setpoint, Auto setpoint — each mode's target, editable in any mode (the climate entity only shows the active mode's target) |

Temperature-difference settings (swing, stage 2 delta, backup heat rise/drop, temperature offset) are in
the thermostat's own unit and are not converted by Home Assistant. If the thermostat
uses a time zone that is not one of the presets, the *Time zone* select shows *unknown*.

With weather enabled on the thermostat, *Outdoor temperature* and *Outdoor condition*
sensors are added (unavailable until the thermostat has weather data).

## Lovelace card

The integration ships its own dashboard card; it is loaded automatically (no HACS
frontend resource or manual resource entry needed — refresh the browser once after
installing/updating). Add it from the dashboard editor (*Add card → ESP32 Thermostat*)
and pick the thermostat, or in YAML:

```yaml
type: custom:esp32-thermostat-card
entity: climate.shop_thermostat
# optional
name: Shop
show_status: true    # action, active stage, hydronic supply/return
show_setpoints: true # heat/cool/auto setpoints with −/+
show_schedule: true  # schedule state, hold, Resume, weekly editor
show_sensors: true   # humidity, air quality, pressure chips
show_weather: true   # outdoor temperature/condition chip
```

Features:
- Dial with current and target temperature; drag the arc or use **−/+** (sent ~1 s after
  the last change). Colour follows the HVAC action (heating, cooling, fan, idle).
- Mode buttons (off/heat/cool/auto), fan mode buttons and a shower-mode button.
- Heat, Cool and Auto setpoints with their own **−/+** (the active mode is highlighted).
- Status row with HVAC action, active stage (stage 1/2, backup heat) and hydronic temps.
- Schedule strip: enable/disable, active period, hold type and remaining time, *Resume*,
  and an **Edit weekly schedule** button. The editor shows 7 days with a Day and a Night
  period each; a period has one start time shared by all modes plus separate heat, cool
  and auto temperatures, an active flag, and *Copy to all* per day.
- Chips for humidity, air quality, pressure and outdoor weather when available.

The card finds the thermostat's other entities through the device, so renaming
entity IDs does not break it.

## Services

```yaml
# Put the thermostat on a permanent hold
action: esp32_thermostat.set_override
data:
  device_id: 0123456789abcdef0123456789abcdef
  mode: permanent   # resume | temporary | permanent

# Change Monday's morning period and disable Sunday
action: esp32_thermostat.set_schedule
data:
  device_id: 0123456789abcdef0123456789abcdef
  schedule_enabled: true
  days:
    - day: monday
      day_period: {hour: 6, minute: 30, heat: 70, cool: 76}
    - day: sunday
      enabled: false
```

`esp32_thermostat.get_schedule` returns the full schedule as a response. Example
script that copies one thermostat's schedule to others:

```yaml
script:
  sync_thermostat_schedules:
    sequence:
      - action: esp32_thermostat.get_schedule
        data:
          device_id: SOURCE_DEVICE_ID
        response_variable: source
      - repeat:
          for_each:
            - TARGET_DEVICE_ID_1
            - TARGET_DEVICE_ID_2
          sequence:
            - action: esp32_thermostat.set_schedule
              data:
                device_id: "{{ repeat.item }}"
                schedule_enabled: "{{ source.enabled }}"
                days: "{{ source.days }}"
```

Device IDs are shown in the URL of the device page or selectable in the UI editor.

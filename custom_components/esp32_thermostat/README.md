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

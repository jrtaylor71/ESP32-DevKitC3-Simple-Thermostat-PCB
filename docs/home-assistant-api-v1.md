# Home Assistant local API v1

The thermostat exposes a local HTTP API for its native Home Assistant integration.
It does not require MQTT or a cloud service. The API is disabled by default; enable
**Home Assistant API** in the thermostat's touch-screen or web settings.

The server listens on HTTP port 80. When enabled and connected to Wi-Fi, the device
advertises `_esp32thermostat._tcp.local` over mDNS. The API token is shown in the
thermostat's settings and is also visible in its web UI.

## Security

Every `/api/v1` request requires the token as a bearer credential:

```http
Authorization: Bearer <api-token>
```

The API uses plain HTTP, not TLS. Keep it on a trusted LAN, do not forward port 80
to the internet, and treat the token as a secret. Anyone with LAN access to the
thermostat's web UI can view the token.

## Common behavior

- Successful write requests return `{"status":"accepted"}` after being queued for
  application by the thermostat's main loop.
- Invalid input returns HTTP `400`; incompatible hardware/settings return `409`;
  a full request queue or too many event clients returns `503`.
- Error responses include an `error` field and may include `field` and `message`.
- JSON request bodies must be objects. Unknown keys are rejected.
- Temperature values use the unit reported by `/api/v1/info`.

## Endpoints

| Method | Path | Purpose |
|---|---|---|
| `GET` | `/api/v1/info` | Device identity, firmware, units, limits, and hardware feature flags |
| `GET` | `/api/v1/state` | Current thermostat state and settings snapshot |
| `GET` | `/api/v1/events` | Server-Sent Events stream of full state and updates |
| `POST` | `/api/v1/command` | Change operating mode, fan, setpoints, schedule behavior, or shower mode |
| `POST` | `/api/v1/settings` | Change thermostat configuration |
| `GET` | `/api/v1/schedule` | Read the complete seven-day schedule |
| `PUT` | `/api/v1/schedule` | Apply partial schedule changes |
| `POST` | `/api/v1/action` | Reboot or refresh weather data |

### Device information

`GET /api/v1/info` returns `api_version`, `device_id`, `mac`, `hostname`,
`manufacturer`, `model`, `firmware_version`, `build`, `temperature_unit`,
`min_temp`, `max_temp`, `temp_step`, `region`, `mqtt_enabled`, and a `features`
object. The feature flags report available sensors, hydronic heating, relay stages,
heat pump, backup heat, motion, shower mode, EU humidity control, and weather.

### State snapshot and events

`GET /api/v1/state` returns the complete state, including:

- Room temperature/humidity and available pressure, air-quality, and hydronic readings.
- Current HVAC mode, fan mode, action, active setpoint, and per-mode heat/cool/auto setpoints.
- Stage, backup heat, lockout, humidity demand, relay, schedule, shower, and motion state.
- `settings`, weather data (or `null`), Wi-Fi RSSI, uptime, free heap, and `info_rev`.

`info_rev` changes when device information changes. Home Assistant uses it to reload
entities and units after configuration changes made on the thermostat.

For `GET /api/v1/events`, send `Accept: text/event-stream` and the bearer token. The
first event is named `state` and contains the full snapshot. Later `update` events
contain changed top-level fields; `ping` events keep the connection alive. Up to two
clients may be connected simultaneously.

### Commands

`POST /api/v1/command` accepts any subset of these keys:

| Key | Values |
|---|---|
| `mode` | `off`, `heat`, `cool`, `auto` |
| `fan_mode` | `auto`, `on`, `cycle` |
| `target_temperature` | Number for the active mode; invalid while mode is `off` |
| `target_temp_heat`, `target_temp_cool`, `target_temp_auto` | Per-mode temperature targets |
| `shower_mode` | Boolean |
| `schedule_enabled` | Boolean |
| `schedule_override` | `resume`, `temporary`, `permanent` |

Setpoint values must be within the device's reported temperature limits. A schedule
override cannot be set while the schedule is disabled. Shower mode cannot be
activated if the thermostat's shower feature is disabled.

### Settings

`POST /api/v1/settings` accepts a partial object. These keys are supported:

| Key | Values or range |
|---|---|
| `temp_swing` | 0.2–3.0 |
| `auto_temp_swing` | 0.2–5.0 |
| `stage1_min_runtime` | Integer, 60–1800 seconds |
| `stage2_temp_delta` | 0.5–5.0 |
| `fan_minutes_per_hour` | Integer, 0–60 |
| `shower_duration` | Integer, 5–120 minutes |
| `display_brightness` | Integer, 30–255 |
| `hydronic_temp_low`, `hydronic_temp_high` | °F: 50–200; °C: 10–95; high must exceed low |
| `eu_humidity_setpoint` | 30–90% |
| `stage2_heat_enabled`, `stage2_cool_enabled`, `heat_pump_enabled` | Boolean |
| `fan_with_heat`, `fan_with_cool` | Boolean |
| `backup_heat_enabled`, `hydronic_heat_enabled`, `eu_humidity_enabled`, `shower_mode_enabled` | Boolean |
| `backup_heat_relay` | `pump`, `heat2`, `cool2` |
| `backup_heat_delay_min` | Integer, 5–180 minutes |
| `backup_heat_min_rise` | 0.1–5.0 temperature degrees |
| `backup_heat_max_drop` | 0.1–10.0 temperature degrees |
| `eu_humidity_relay` | `cool1`, `cool2`, `pump` |
| `eu_humidity_deadband` | 1–20% |
| `temp_offset` | ±18°F or ±10°C; a temperature difference in the device unit |
| `humidity_offset` | −50–50% |
| `temperature_unit` | `F`, `C` |
| `region` | `US`, `EU` |
| `time_zone` | POSIX time-zone string, 1–63 printable characters |

The settings update is validated as a whole against the resulting configuration.
Conflicting relay combinations return `409` rather than silently changing another
setting. Changing `temperature_unit` converts setpoints, schedule temperatures,
hydronic limits, and temperature-difference settings. Other settings in the same
request are interpreted in the new unit.

### Schedule

`GET /api/v1/schedule` returns `{ "enabled": boolean, "days": [...] }`. Each of the
seven entries has a Sunday-based `index` (0–6), `day` name, `enabled` flag, and
`day_period` and `night_period` objects. A period contains `hour` (0–23), `minute`
(0–59), `heat`, `cool`, and `auto` targets, plus an `active` flag.

`PUT /api/v1/schedule` accepts an optional top-level `enabled` and partial `days`.
A day may be identified by its name or index; only supplied fields are changed.
Example:

```json
{
  "days": [
    {
      "day": "monday",
      "day_period": {"hour": 6, "minute": 30, "heat": 70, "cool": 76}
    }
  ]
}
```

### Actions

`POST /api/v1/action` accepts exactly one of:

```json
{"action": "reboot"}
```

```json
{"action": "refresh_weather"}
```

Weather refresh returns `409` when weather is disabled in thermostat settings.

## Home Assistant integration

For installation, discovery, entities, services, and the dashboard card, see
[`custom_components/esp32_thermostat/README.md`](../custom_components/esp32_thermostat/README.md).

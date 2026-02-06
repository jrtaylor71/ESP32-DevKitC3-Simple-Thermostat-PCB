# Mobile App Add-on (iOS + Android)

This add-on outlines what it would take to build a mobile app for remotely managing the thermostat. It is based on the existing MQTT integration and the built-in web API.

---

## 1) Current Capabilities You Can Reuse

### MQTT (already implemented)
MQTT is the most complete and reliable remote-control path.

**State topics (published by device):**
- `hostname/current_temperature`
- `hostname/current_humidity`
- `hostname/target_temperature`
- `hostname/mode`
- `hostname/fan_mode`
- `hostname/action`
- `hostname/availability`
- `hostname/motion_detected`
- `hostname/barometric_pressure`
- `hostname/shower_mode`
- `hostname/shower_time_remaining`

**Command topics (subscribed by device):**
- `hostname/mode/set`
- `hostname/fan_mode/set`
- `hostname/target_temperature/set`
- `hostname/shower_mode/set`
- `hostname/schedule_enabled/set`
- `hostname/schedule_override/set`
- `hostname/schedule/set`

These topics already map cleanly to mobile UI actions (setpoint, mode, fan, schedule, shower mode, etc.).

### Local Web API (on-device)
The device exposes JSON endpoints you can call over the local network:
- `GET /status` — current temp, humidity, setpoints, modes
- `POST /control` — set mode, setpoint, fan, etc.
- `GET /version` — firmware version
- `POST /reboot` — reboot device

> Note: The web API is local-only unless you expose the device over the internet or use a relay service.

---

## 2) App Architecture Options

### Option A — Local LAN App (No Cloud)
**How it works:**
- App finds device on local network (mDNS or manual IP entry)
- App uses HTTP `/status` + `/control`

**Pros**
- No cloud costs
- Simple setup

**Cons**
- Only works on same Wi‑Fi
- No remote access

### Option B — MQTT + Broker (Recommended for Remote Access)
**How it works:**
- App connects to MQTT broker (local or hosted)
- Device already publishes/subscribes topics

**Pros**
- Works remotely (if broker is reachable)
- Bi-directional, real-time updates
- Already supported by firmware

**Cons**
- Requires broker + credentials

### Option C — MQTT + Small Cloud API Relay
**How it works:**
- App talks to cloud REST API
- API relays via MQTT to device
- API stores device list, users, push tokens

**Pros**
- Best UX (push notifications, device sharing)
- No direct MQTT in the app

**Cons**
- More dev work + hosting

---

## 3) Recommended Approach

**Short term (fastest):**
- Build a cross‑platform app that connects directly to MQTT.
- Users enter broker host/port/user/pass + thermostat hostname.

**Mid term (polished):**
- Add optional cloud relay API for account login, device sharing, and push alerts.

---

## 4) Mobile Tech Stack Options

### Cross‑Platform (Single codebase)
- **Flutter** (Dart)
  - Great UI, fast, excellent MQTT packages
- **React Native** (TypeScript)
  - Strong ecosystem, MQTT libraries available

### Native (Separate codebases)
- **iOS:** Swift + SwiftUI
- **Android:** Kotlin + Jetpack Compose

**Recommendation:** Flutter or React Native to keep time and cost down.

---

## 5) Core App Features (MVP)

1. Device setup
   - Add broker credentials
   - Save hostname (topic prefix)
2. Dashboard
   - Current temp/humidity
   - Current setpoint, mode, fan state
3. Controls
   - Set target temperature
   - Set mode (Off/Heat/Cool/Auto)
   - Set fan mode (Auto/On/Cycle)
4. Schedule
   - Enable/disable schedule
   - Set schedule override
5. Shower mode
   - Toggle + show remaining time
6. Device status
   - Online/offline indicator

---

## 6) Security Considerations

- **MQTT:** use TLS if broker is public
- Strong passwords, unique device IDs
- If building a cloud relay, use token auth + per-device ACLs

---

## 7) Effort & Milestones (Rough)

**MVP (4–6 weeks)**
- App skeleton + MQTT connect
- Live dashboard + basic controls
- Device list + settings

**Polish (6–10 weeks)**
- Schedule editor
- Notifications (optional)
- Cloud relay (optional)

---

## 8) Next Steps

1. Decide architecture (MQTT direct vs. cloud relay)
2. Pick mobile framework
3. Define MQTT topic contract + payload formats
4. Create UI wireframes
5. Build MVP

---

## Appendix: MQTT Topic Quick Reference
See USER_MANUAL.md for the full list of topics and behavior.

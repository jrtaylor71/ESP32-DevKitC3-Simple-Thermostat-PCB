/*
 * HomeAssistantApi - token-protected local JSON API for the native
 * Home Assistant integration (custom_components/esp32_thermostat).
 * Copyright (c) 2025 Jonn Taylor
 *
 *   GET  /api/v1/info       device info + feature flags
 *   GET  /api/v1/state      full state snapshot
 *   GET  /api/v1/events     Server-Sent Events: "state" on connect, "update" deltas, "ping"
 *   POST /api/v1/command    mode / fan / setpoints / schedule / shower
 *   POST /api/v1/settings   tunable settings
 *   GET  /api/v1/schedule   7-day schedule
 *   PUT  /api/v1/schedule   partial schedule update
 *   POST /api/v1/action     reboot / refresh_weather
 *
 * Web handlers run on the async_tcp task. They only validate requests and
 * queue them; the main loop applies changes through ThermostatControl.h and
 * publishes state, so no thermostat globals are mutated off the loop task.
 *
 * Header-only: include exactly once from Main-Thermostat.cpp after
 * ThermostatControl.h.
 */

#ifndef HOME_ASSISTANT_API_H
#define HOME_ASSISTANT_API_H

#include <map>
#include <ESPmDNS.h>
#include <AsyncJson.h>
#include "esp_mac.h"

#define HA_API_VERSION 1
#define HA_API_MAX_SSE_CLIENTS 2
#define HA_API_QUEUE_DEPTH 8
#define HA_API_SAVE_DELAY_MS 2000UL
#define HA_API_STATE_INTERVAL_MS 1000UL
#define HA_API_SLOW_KEY_INTERVAL_MS 60000UL
#define HA_API_PING_INTERVAL_MS 30000UL
#define HA_API_MDNS_RETRY_MS 30000UL
#define HA_API_TOKEN_LENGTH 32

bool haApiEnabled = false;
char haApiToken[HA_API_TOKEN_LENGTH + 1] = "";

enum HaQueueKind : uint8_t {
    HA_QUEUE_COMMAND,
    HA_QUEUE_SETTINGS,
    HA_QUEUE_SCHEDULE,
    HA_QUEUE_ACTION,
    HA_QUEUE_LEGACY_CONTROL
};

struct HaQueueItem {
    HaQueueKind kind;
    String* body;
};

static QueueHandle_t haCommandQueue = NULL;
static SemaphoreHandle_t haCacheMutex = NULL;
static String haCachedState = "{}";
static String haCachedInfo = "{}";
static volatile bool haConfigChanged = false;  // enable/token/hostname changed: restart mDNS, drop SSE clients
static volatile bool haStateDirty = false;     // publish state on the next loop pass
static volatile uint8_t haCachedModeIndex = 0; // 0=off 1=heat 2=cool 3=auto, read by validators
static bool haMdnsRunning = false;
static unsigned long haLastMdnsAttempt = 0;
static bool haSavePending = false;
static unsigned long haSaveDueAt = 0;
static uint32_t haEventId = 0;

AsyncEventSource haEvents("/api/v1/events");

static const char* const HA_MODE_NAMES[4] = {"off", "heat", "cool", "auto"};

// ---------------------------------------------------------------------------
// Token / enable management (safe from any task)
// ---------------------------------------------------------------------------

void haApiGenerateToken() {
    static const char hex[] = "0123456789abcdef";
    char token[HA_API_TOKEN_LENGTH + 1];
    for (int i = 0; i < HA_API_TOKEN_LENGTH; i += 8) {
        uint32_t r = esp_random();
        for (int j = 0; j < 8; j++) {
            token[i + j] = hex[(r >> (j * 4)) & 0x0F];
        }
    }
    token[HA_API_TOKEN_LENGTH] = '\0';
    memcpy(haApiToken, token, sizeof(token));
    haConfigChanged = true;
    debugLog("[HA API] New API token generated\n");
}

void haApiSetEnabled(bool enabled) {
    if (enabled && haApiToken[0] == '\0') {
        haApiGenerateToken();
    }
    if (enabled != haApiEnabled) {
        haApiEnabled = enabled;
        haConfigChanged = true;
        debugLog("[HA API] %s\n", enabled ? "Enabled" : "Disabled");
    }
}

void haApiRequestMdnsRestart() {
    haConfigChanged = true;
}

static bool haTokenMatches(const String& header) {
    static const char prefix[] = "Bearer ";
    const size_t prefixLen = sizeof(prefix) - 1;
    if (haApiToken[0] == '\0' || header.length() != prefixLen + HA_API_TOKEN_LENGTH) return false;
    if (!header.startsWith(prefix)) return false;
    uint8_t diff = 0;
    const char* supplied = header.c_str() + prefixLen;
    for (int i = 0; i < HA_API_TOKEN_LENGTH; i++) {
        diff |= (uint8_t)(supplied[i] ^ haApiToken[i]);
    }
    return diff == 0;
}

// ---------------------------------------------------------------------------
// Response helpers
// ---------------------------------------------------------------------------

static void haSendJson(AsyncWebServerRequest* request, int code, const String& body) {
    AsyncWebServerResponse* response = request->beginResponse(code, "application/json", body);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
}

static void haSendError(AsyncWebServerRequest* request, int code, const char* error,
                        const char* field = nullptr, const String& message = String()) {
    JsonDocument doc;
    doc["error"] = error;
    if (field) doc["field"] = field;
    if (message.length()) doc["message"] = message;
    String body;
    serializeJson(doc, body);
    haSendJson(request, code, body);
}

static void haSendAccepted(AsyncWebServerRequest* request) {
    haSendJson(request, 200, "{\"status\":\"accepted\"}");
}

static String haCachedCopy(const String& source) {
    String copy;
    if (haCacheMutex && xSemaphoreTake(haCacheMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        copy = source;
        xSemaphoreGive(haCacheMutex);
    }
    return copy;
}

static void haSendCached(AsyncWebServerRequest* request, const String& source) {
    String body = haCachedCopy(source);
    if (body.length() == 0) {
        haSendError(request, 503, "busy", nullptr, "State not available, try again");
        return;
    }
    haSendJson(request, 200, body);
}

// ---------------------------------------------------------------------------
// Request queue (web task -> main loop)
// ---------------------------------------------------------------------------

bool haApiEnqueueJson(HaQueueKind kind, JsonVariantConst json) {
    if (!haCommandQueue) return false;
    String* body = new String();
    serializeJson(json, *body);
    HaQueueItem item = {kind, body};
    if (xQueueSend(haCommandQueue, &item, 0) != pdTRUE) {
        delete body;
        return false;
    }
    return true;
}

static void haEnqueueOrBusy(AsyncWebServerRequest* request, HaQueueKind kind, JsonVariantConst json) {
    if (haApiEnqueueJson(kind, json)) {
        haSendAccepted(request);
    } else {
        haSendError(request, 503, "busy", nullptr, "Command queue full, try again");
    }
}

// ---------------------------------------------------------------------------
// Validation helpers (run on the web task; only read plain-old-data globals)
// ---------------------------------------------------------------------------

struct HaValidation {
    int code = 0;
    const char* field = nullptr;
    String message;
    bool ok() const { return code == 0; }
    void fail(int c, const char* f, const String& m) {
        if (code == 0) { code = c; field = f; message = m; }
    }
};

static void haReplyValidation(AsyncWebServerRequest* request, const HaValidation& v) {
    haSendError(request, v.code, v.code == 409 ? "conflict" : "invalid", v.field, v.message);
}

static bool haIsNumber(JsonVariantConst v) {
    return v.is<float>() || v.is<long>();
}

static bool haCheckNumber(HaValidation& result, JsonVariantConst v, const char* field, float minValue, float maxValue) {
    if (!haIsNumber(v)) {
        result.fail(400, field, "must be a number");
        return false;
    }
    float value = v.as<float>();
    if (isnan(value) || value < minValue || value > maxValue) {
        result.fail(400, field, "must be between " + String(minValue, 1) + " and " + String(maxValue, 1));
        return false;
    }
    return true;
}

static bool haCheckInteger(HaValidation& result, JsonVariantConst v, const char* field, long minValue, long maxValue) {
    if (!haIsNumber(v) || v.as<float>() != floorf(v.as<float>())) {
        result.fail(400, field, "must be an integer");
        return false;
    }
    long value = v.as<long>();
    if (value < minValue || value > maxValue) {
        result.fail(400, field, "must be between " + String(minValue) + " and " + String(maxValue));
        return false;
    }
    return true;
}

static bool haCheckBool(HaValidation& result, JsonVariantConst v, const char* field) {
    if (!v.is<bool>()) {
        result.fail(400, field, "must be true or false");
        return false;
    }
    return true;
}

static bool haCheckSetpoint(HaValidation& result, JsonVariantConst v, const char* field) {
    return haCheckNumber(result, v, field, setpointMin(), setpointMax());
}

static bool haKeyAllowed(const char* key, const char* const* allowed, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(key, allowed[i]) == 0) return true;
    }
    return false;
}

static bool haCheckKeys(HaValidation& result, JsonObjectConst obj, const char* const* allowed, size_t count) {
    if (obj.size() == 0) {
        result.fail(400, nullptr, "no fields supplied");
        return false;
    }
    for (JsonPairConst kv : obj) {
        if (!haKeyAllowed(kv.key().c_str(), allowed, count)) {
            result.fail(400, kv.key().c_str(), "unknown field");
            return false;
        }
    }
    return true;
}

static int haModeIndex(const char* mode) {
    for (int i = 0; i < 4; i++) {
        if (strcmp(mode, HA_MODE_NAMES[i]) == 0) return i;
    }
    return -1;
}

static int haDayIndex(JsonObjectConst day) {
    if (day["index"].is<int>()) {
        int index = day["index"].as<int>();
        return (index >= 0 && index < 7) ? index : -1;
    }
    if (day["day"].is<const char*>()) {
        String name = day["day"].as<String>();
        name.toLowerCase();
        for (int i = 0; i < 7; i++) {
            if (name == SCHEDULE_DAY_NAMES[i]) return i;
        }
    }
    return -1;
}

static HaValidation haValidateCommand(JsonObjectConst cmd) {
    static const char* const allowed[] = {
        "mode", "fan_mode", "target_temperature", "target_temp_heat", "target_temp_cool",
        "target_temp_auto", "shower_mode", "schedule_enabled", "schedule_override"};
    HaValidation result;
    if (!haCheckKeys(result, cmd, allowed, sizeof(allowed) / sizeof(allowed[0]))) return result;

    int effectiveMode = haCachedModeIndex;
    if (!cmd["mode"].isNull()) {
        effectiveMode = cmd["mode"].is<const char*>() ? haModeIndex(cmd["mode"].as<const char*>()) : -1;
        if (effectiveMode < 0) result.fail(400, "mode", "must be one of off, heat, cool, auto");
    }
    if (!cmd["fan_mode"].isNull() &&
        !(cmd["fan_mode"].is<const char*>() && isValidFanMode(cmd["fan_mode"].as<String>()))) {
        result.fail(400, "fan_mode", "must be one of auto, on, cycle");
    }
    if (!cmd["target_temperature"].isNull() &&
        haCheckSetpoint(result, cmd["target_temperature"], "target_temperature") && effectiveMode == 0) {
        result.fail(400, "target_temperature", "cannot set a target temperature while mode is off");
    }
    if (!cmd["target_temp_heat"].isNull()) haCheckSetpoint(result, cmd["target_temp_heat"], "target_temp_heat");
    if (!cmd["target_temp_cool"].isNull()) haCheckSetpoint(result, cmd["target_temp_cool"], "target_temp_cool");
    if (!cmd["target_temp_auto"].isNull()) haCheckSetpoint(result, cmd["target_temp_auto"], "target_temp_auto");

    if (!cmd["shower_mode"].isNull() && haCheckBool(result, cmd["shower_mode"], "shower_mode") &&
        cmd["shower_mode"].as<bool>() && !showerModeEnabled) {
        result.fail(409, "shower_mode", "shower mode is disabled in device settings");
    }

    bool effectiveSchedule = scheduleEnabled;
    if (!cmd["schedule_enabled"].isNull() && haCheckBool(result, cmd["schedule_enabled"], "schedule_enabled")) {
        effectiveSchedule = cmd["schedule_enabled"].as<bool>();
    }
    if (!cmd["schedule_override"].isNull()) {
        String type = cmd["schedule_override"].is<const char*>() ? cmd["schedule_override"].as<String>() : String();
        if (type != "resume" && type != "temporary" && type != "permanent") {
            result.fail(400, "schedule_override", "must be one of resume, temporary, permanent");
        } else if (!effectiveSchedule) {
            result.fail(409, "schedule_override", "schedule is disabled");
        }
    }
    return result;
}

static bool haCheckEnum(HaValidation& result, JsonVariantConst v, const char* field,
                        const char* const* values, size_t count, int* index = nullptr) {
    if (v.is<const char*>()) {
        const char* text = v.as<const char*>();
        for (size_t i = 0; i < count; i++) {
            if (strcmp(text, values[i]) == 0) {
                if (index) *index = (int)i;
                return true;
            }
        }
    }
    String message = "must be one of";
    for (size_t i = 0; i < count; i++) message += String(i ? ", " : " ") + values[i];
    result.fail(400, field, message);
    return false;
}

// Effective boolean: request value if valid, otherwise the current setting.
static bool haEffectiveBool(HaValidation& result, JsonObjectConst s, const char* field, bool current) {
    if (s[field].isNull()) return current;
    return haCheckBool(result, s[field], field) ? s[field].as<bool>() : current;
}

static const char* haFirstPresent(JsonObjectConst s, std::initializer_list<const char*> fields) {
    for (const char* f : fields) {
        if (!s[f].isNull()) return f;
    }
    return *fields.begin();
}

static HaValidation haValidateSettings(JsonObjectConst s) {
    static const char* const allowed[] = {
        "temp_swing", "auto_temp_swing", "stage1_min_runtime", "stage2_temp_delta", "fan_minutes_per_hour",
        "shower_duration", "display_brightness", "hydronic_temp_low", "hydronic_temp_high", "eu_humidity_setpoint",
        "stage2_heat_enabled", "stage2_cool_enabled", "heat_pump_enabled", "fan_with_heat", "fan_with_cool",
        "backup_heat_enabled", "backup_heat_relay", "backup_heat_delay_min", "backup_heat_min_rise",
        "backup_heat_max_drop", "hydronic_heat_enabled", "eu_humidity_enabled", "eu_humidity_relay",
        "eu_humidity_deadband", "shower_mode_enabled", "temp_offset", "humidity_offset", "temperature_unit",
        "region", "time_zone"};
    static const char* const units[] = {"F", "C"};
    static const char* const regions[] = {"US", "EU"};
    HaValidation result;
    if (!haCheckKeys(result, s, allowed, sizeof(allowed) / sizeof(allowed[0]))) return result;

    if (!s["temp_swing"].isNull()) haCheckNumber(result, s["temp_swing"], "temp_swing", 0.2f, 3.0f);
    if (!s["auto_temp_swing"].isNull()) haCheckNumber(result, s["auto_temp_swing"], "auto_temp_swing", 0.2f, 5.0f);
    if (!s["stage1_min_runtime"].isNull()) haCheckInteger(result, s["stage1_min_runtime"], "stage1_min_runtime", 60, 1800);
    if (!s["stage2_temp_delta"].isNull()) haCheckNumber(result, s["stage2_temp_delta"], "stage2_temp_delta", 0.5f, 5.0f);
    if (!s["fan_minutes_per_hour"].isNull()) haCheckInteger(result, s["fan_minutes_per_hour"], "fan_minutes_per_hour", 0, 60);
    if (!s["shower_duration"].isNull()) haCheckInteger(result, s["shower_duration"], "shower_duration", 5, 120);
    if (!s["display_brightness"].isNull()) haCheckInteger(result, s["display_brightness"], "display_brightness", MIN_BRIGHTNESS, MAX_BRIGHTNESS);
    if (!s["eu_humidity_setpoint"].isNull()) haCheckNumber(result, s["eu_humidity_setpoint"], "eu_humidity_setpoint", 30.0f, 90.0f);
    if (!s["backup_heat_delay_min"].isNull()) haCheckInteger(result, s["backup_heat_delay_min"], "backup_heat_delay_min", 5, 180);
    if (!s["backup_heat_min_rise"].isNull()) haCheckNumber(result, s["backup_heat_min_rise"], "backup_heat_min_rise", 0.1f, 5.0f);
    if (!s["backup_heat_max_drop"].isNull()) haCheckNumber(result, s["backup_heat_max_drop"], "backup_heat_max_drop", 0.1f, 10.0f);
    if (!s["eu_humidity_deadband"].isNull()) haCheckNumber(result, s["eu_humidity_deadband"], "eu_humidity_deadband", 1.0f, 20.0f);
    if (!s["humidity_offset"].isNull()) haCheckNumber(result, s["humidity_offset"], "humidity_offset", -50.0f, 50.0f);
    if (!s["fan_with_heat"].isNull()) haCheckBool(result, s["fan_with_heat"], "fan_with_heat");
    if (!s["fan_with_cool"].isNull()) haCheckBool(result, s["fan_with_cool"], "fan_with_cool");
    if (!s["hydronic_heat_enabled"].isNull()) haCheckBool(result, s["hydronic_heat_enabled"], "hydronic_heat_enabled");
    if (!s["shower_mode_enabled"].isNull()) haCheckBool(result, s["shower_mode_enabled"], "shower_mode_enabled");
    if (!s["region"].isNull()) haCheckEnum(result, s["region"], "region", regions, 2);
    if (!s["time_zone"].isNull()) {
        const char* tz = s["time_zone"].is<const char*>() ? s["time_zone"].as<const char*>() : nullptr;
        bool ok = tz && strlen(tz) >= 1 && strlen(tz) <= 63;
        for (const char* c = tz; ok && *c; c++) ok = *c >= 0x21 && *c <= 0x7E;
        if (!ok) result.fail(400, "time_zone", "must be a POSIX TZ string (1-63 printable characters)");
    }

    bool fahrenheit = useFahrenheit;
    if (!s["temperature_unit"].isNull()) {
        int unitIndex = 0;
        if (haCheckEnum(result, s["temperature_unit"], "temperature_unit", units, 2, &unitIndex)) {
            fahrenheit = unitIndex == 0;
        }
    }

    // Hydronic thresholds are absolute temperatures in the (possibly new) device unit.
    float hydMin = fahrenheit ? 50.0f : 10.0f;
    float hydMax = fahrenheit ? 200.0f : 95.0f;
    auto toUnit = [&](float v) {
        if (fahrenheit == useFahrenheit) return v;
        return fahrenheit ? roundf(v * 9.0f / 5.0f + 32.0f) : roundf((v - 32.0f) * 5.0f / 9.0f);
    };
    // temp_offset is a difference in the (possibly new) device unit; stored as ±10 °C.
    if (!s["temp_offset"].isNull()) {
        float offsetLimit = fahrenheit ? 18.0f : 10.0f;
        haCheckNumber(result, s["temp_offset"], "temp_offset", -offsetLimit, offsetLimit);
    }
    float low = toUnit(hydronicTempLow);
    float high = toUnit(hydronicTempHigh);
    if (!s["hydronic_temp_low"].isNull() && haCheckNumber(result, s["hydronic_temp_low"], "hydronic_temp_low", hydMin, hydMax)) {
        low = s["hydronic_temp_low"].as<float>();
    }
    if (!s["hydronic_temp_high"].isNull() && haCheckNumber(result, s["hydronic_temp_high"], "hydronic_temp_high", hydMin, hydMax)) {
        high = s["hydronic_temp_high"].as<float>();
    }
    if (result.ok() && high <= low) {
        result.fail(400, s["hydronic_temp_high"].isNull() ? "hydronic_temp_low" : "hydronic_temp_high",
                    "hydronic_temp_high must be greater than hydronic_temp_low");
    }
    if (!result.ok()) return result;

    // Relay / feature conflicts, evaluated on the merged configuration.
    bool stage2Heat = haEffectiveBool(result, s, "stage2_heat_enabled", stage2HeatingEnabled);
    bool stage2Cool = haEffectiveBool(result, s, "stage2_cool_enabled", stage2CoolingEnabled);
    bool heatPump = haEffectiveBool(result, s, "heat_pump_enabled", reversingValveEnabled);
    bool backup = haEffectiveBool(result, s, "backup_heat_enabled", backupHeatEnabled);
    bool euHumidity = haEffectiveBool(result, s, "eu_humidity_enabled", euHumidityControlEnabled);
    int backupRelay = backupHeatRelaySelection;
    int euRelay = euHumidityRelaySelection;
    if (!s["backup_heat_relay"].isNull()) haCheckEnum(result, s["backup_heat_relay"], "backup_heat_relay", BACKUP_HEAT_RELAY_NAMES, 3, &backupRelay);
    if (!s["eu_humidity_relay"].isNull()) haCheckEnum(result, s["eu_humidity_relay"], "eu_humidity_relay", EU_HUMIDITY_RELAY_NAMES, 3, &euRelay);
    if (!result.ok()) return result;

    if (stage2Heat && heatPump) {
        result.fail(409, haFirstPresent(s, {"heat_pump_enabled", "stage2_heat_enabled"}),
                    "stage 2 heat and heat pump share the H2 relay; disable one first");
    } else if (backup && backupRelay == 1 && (stage2Heat || heatPump)) {
        result.fail(409, haFirstPresent(s, {"backup_heat_relay", "backup_heat_enabled", "stage2_heat_enabled", "heat_pump_enabled"}),
                    "backup heat on the H2 relay conflicts with stage 2 heat / heat pump");
    } else if (backup && backupRelay == 2 && stage2Cool) {
        result.fail(409, haFirstPresent(s, {"backup_heat_relay", "backup_heat_enabled", "stage2_cool_enabled"}),
                    "backup heat on the C2 relay conflicts with stage 2 cool");
    } else if (euHumidity && euRelay == 1 && stage2Cool) {
        result.fail(409, haFirstPresent(s, {"eu_humidity_relay", "eu_humidity_enabled", "stage2_cool_enabled"}),
                    "EU humidity control on the C2 relay conflicts with stage 2 cool");
    }
    return result;
}

static void haValidatePeriod(HaValidation& result, JsonVariantConst period, const char* field) {
    static const char* const allowed[] = {"hour", "minute", "heat", "cool", "auto", "active"};
    if (!period.is<JsonObjectConst>()) {
        result.fail(400, field, "must be an object");
        return;
    }
    JsonObjectConst p = period.as<JsonObjectConst>();
    if (!haCheckKeys(result, p, allowed, sizeof(allowed) / sizeof(allowed[0]))) return;
    if (!p["hour"].isNull()) haCheckInteger(result, p["hour"], field, 0, 23);
    if (!p["minute"].isNull()) haCheckInteger(result, p["minute"], field, 0, 59);
    if (!p["heat"].isNull()) haCheckSetpoint(result, p["heat"], field);
    if (!p["cool"].isNull()) haCheckSetpoint(result, p["cool"], field);
    if (!p["auto"].isNull()) haCheckSetpoint(result, p["auto"], field);
    if (!p["active"].isNull()) haCheckBool(result, p["active"], field);
}

static HaValidation haValidateSchedule(JsonObjectConst body) {
    static const char* const allowed[] = {"enabled", "days"};
    static const char* const dayAllowed[] = {"day", "index", "enabled", "day_period", "night_period"};
    HaValidation result;
    if (!haCheckKeys(result, body, allowed, 2)) return result;
    if (!body["enabled"].isNull()) haCheckBool(result, body["enabled"], "enabled");
    if (!body["days"].isNull()) {
        if (!body["days"].is<JsonArrayConst>()) {
            result.fail(400, "days", "must be a list");
            return result;
        }
        for (JsonVariantConst entry : body["days"].as<JsonArrayConst>()) {
            if (!entry.is<JsonObjectConst>()) {
                result.fail(400, "days", "each entry must be an object");
                break;
            }
            JsonObjectConst day = entry.as<JsonObjectConst>();
            if (!haCheckKeys(result, day, dayAllowed, sizeof(dayAllowed) / sizeof(dayAllowed[0]))) break;
            if (haDayIndex(day) < 0) {
                result.fail(400, "days", "each entry needs a valid 'day' name or 'index' (0=sunday..6=saturday)");
                break;
            }
            if (!day["enabled"].isNull()) haCheckBool(result, day["enabled"], "days.enabled");
            if (!day["day_period"].isNull()) haValidatePeriod(result, day["day_period"], "days.day_period");
            if (!day["night_period"].isNull()) haValidatePeriod(result, day["night_period"], "days.night_period");
            if (!result.ok()) break;
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// JSON builders
// ---------------------------------------------------------------------------

String haDeviceId() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char id[13];
    snprintf(id, sizeof(id), "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(id);
}

static String haMacAddress() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char text[18];
    snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(text);
}

void buildInfoJson(JsonDocument& doc) {
    JsonObject root = doc.to<JsonObject>();
    root["api_version"] = HA_API_VERSION;
    root["device_id"] = haDeviceId();
    root["mac"] = haMacAddress();
    root["hostname"] = hostname;
    root["manufacturer"] = MQTT_MANUFACTURER;
    root["model"] = MQTT_MODEL;
    root["firmware_version"] = sw_version;
    root["build"] = build_date + " " + build_time;
    root["temperature_unit"] = useFahrenheit ? "F" : "C";
    root["min_temp"] = serialized(String(setpointMin(), 1));
    root["max_temp"] = serialized(String(setpointMax(), 1));
    root["temp_step"] = 0.5;
    root["region"] = thermostatRegion;
    root["mqtt_enabled"] = mqttEnabled;

    JsonObject features = root["features"].to<JsonObject>();
    features["sensor"] = sensorName;
    features["humidity"] = sensorHasHumidity();
    features["pressure"] = sensorHasPressure();
    features["air_quality"] = sensorHasAirQuality();
    features["hydronic_heat"] = hydronicHeatingEnabled;
    features["hydronic_supply_sensor"] = hydronicHeatingEnabled && ds18b20SensorPresent;
    features["hydronic_return_sensor"] = hydronicHeatingEnabled && ds18b20ReturnSensorPresent;
    features["stage2_heat"] = stage2HeatingEnabled;
    features["stage2_cool"] = stage2CoolingEnabled;
    features["heat_pump"] = reversingValveEnabled;
    features["backup_heat"] = backupHeatEnabled;
    features["motion_sensor"] = ld2410Connected;
    features["shower_mode"] = showerModeEnabled;
    features["eu_humidity_control"] = euHumidityControlEnabled;
    features["weather"] = weatherSource != 0;
}

static void haAddPeriodJson(JsonObject obj, const SchedulePeriod& period) {
    obj["hour"] = period.hour;
    obj["minute"] = period.minute;
    obj["heat"] = serialized(String(period.heatTemp, 1));
    obj["cool"] = serialized(String(period.coolTemp, 1));
    obj["auto"] = serialized(String(period.autoTemp, 1));
    obj["active"] = period.active;
}

void buildScheduleJson(JsonDocument& doc) {
    JsonObject root = doc.to<JsonObject>();
    root["enabled"] = scheduleEnabled;
    JsonArray days = root["days"].to<JsonArray>();
    for (int i = 0; i < 7; i++) {
        JsonObject day = days.add<JsonObject>();
        day["index"] = i;
        day["day"] = SCHEDULE_DAY_NAMES[i];
        day["enabled"] = weekSchedule[i].enabled;
        haAddPeriodJson(day["day_period"].to<JsonObject>(), weekSchedule[i].day);
        haAddPeriodJson(day["night_period"].to<JsonObject>(), weekSchedule[i].night);
    }
}

// ---------------------------------------------------------------------------
// Apply queued requests (main loop only)
// ---------------------------------------------------------------------------

static void haRequestSave() {
    haSavePending = true;
    haSaveDueAt = millis() + HA_API_SAVE_DELAY_MS;
}

static bool haApplyCommand(JsonObjectConst cmd) {
    bool persist = false;
    bool changed = false;
    if (!cmd["schedule_enabled"].isNull() && applyScheduleEnabled(cmd["schedule_enabled"].as<bool>())) {
        persist = changed = true;
    }
    if (!cmd["schedule_override"].isNull() && applyScheduleOverride(cmd["schedule_override"].as<String>())) {
        persist = changed = true;
    }
    if (!cmd["mode"].isNull() && applyThermostatMode(cmd["mode"].as<String>())) {
        persist = changed = true;
    }
    if (!cmd["target_temp_heat"].isNull() && applySetpoint(SETPOINT_HEAT, cmd["target_temp_heat"].as<float>())) {
        persist = changed = true;
    }
    if (!cmd["target_temp_cool"].isNull() && applySetpoint(SETPOINT_COOL, cmd["target_temp_cool"].as<float>())) {
        persist = changed = true;
    }
    if (!cmd["target_temp_auto"].isNull() && applySetpoint(SETPOINT_AUTO, cmd["target_temp_auto"].as<float>())) {
        persist = changed = true;
    }
    if (!cmd["target_temperature"].isNull() && applyActiveSetpoint(cmd["target_temperature"].as<float>())) {
        persist = changed = true;
    }
    if (!cmd["fan_mode"].isNull() && applyFanMode(cmd["fan_mode"].as<String>())) {
        persist = changed = true;
    }
    if (!cmd["shower_mode"].isNull() && applyShowerMode(cmd["shower_mode"].as<bool>())) {
        changed = true;
    }
    if (persist) haRequestSave();
    return changed;
}

static bool haApplySettings(JsonObjectConst s) {
    // Unit first: other temperatures in the same request are already in the new unit.
    if (!s["temperature_unit"].isNull()) {
        applyTemperatureUnit(strcmp(s["temperature_unit"].as<const char*>(), "F") == 0);
    }
    if (!s["temp_swing"].isNull()) tempSwing = s["temp_swing"].as<float>();
    if (!s["auto_temp_swing"].isNull()) autoTempSwing = s["auto_temp_swing"].as<float>();
    if (!s["stage1_min_runtime"].isNull()) stage1MinRuntime = s["stage1_min_runtime"].as<unsigned long>();
    if (!s["stage2_temp_delta"].isNull()) stage2TempDelta = s["stage2_temp_delta"].as<float>();
    if (!s["fan_minutes_per_hour"].isNull()) fanMinutesPerHour = s["fan_minutes_per_hour"].as<int>();
    if (!s["shower_duration"].isNull()) showerModeDuration = s["shower_duration"].as<int>();
    if (!s["hydronic_temp_low"].isNull()) hydronicTempLow = s["hydronic_temp_low"].as<float>();
    if (!s["hydronic_temp_high"].isNull()) hydronicTempHigh = s["hydronic_temp_high"].as<float>();
    if (!s["eu_humidity_setpoint"].isNull()) euHumiditySetpoint = s["eu_humidity_setpoint"].as<float>();
    if (!s["display_brightness"].isNull()) {
        currentBrightness = s["display_brightness"].as<int>();
        setBrightness(currentBrightness);
    }
    if (!s["stage2_heat_enabled"].isNull()) stage2HeatingEnabled = s["stage2_heat_enabled"].as<bool>();
    if (!s["stage2_cool_enabled"].isNull()) stage2CoolingEnabled = s["stage2_cool_enabled"].as<bool>();
    if (!s["heat_pump_enabled"].isNull()) reversingValveEnabled = s["heat_pump_enabled"].as<bool>();
    if (!s["fan_with_heat"].isNull()) fanRelayNeededHeat = s["fan_with_heat"].as<bool>();
    if (!s["fan_with_cool"].isNull()) fanRelayNeededCool = s["fan_with_cool"].as<bool>();
    if (!s["backup_heat_enabled"].isNull()) {
        backupHeatEnabled = s["backup_heat_enabled"].as<bool>();
        if (!backupHeatEnabled) clearBackupHeatState("disabled via local API");
    }
    if (!s["backup_heat_relay"].isNull()) {
        int index = relayNameIndex(BACKUP_HEAT_RELAY_NAMES, s["backup_heat_relay"].as<const char*>());
        if (index >= 0 && index != backupHeatRelaySelection) {
            clearBackupHeatState("relay changed via local API");
            backupHeatRelaySelection = index;
        }
    }
    if (!s["backup_heat_delay_min"].isNull()) backupHeatDelayMinutes = s["backup_heat_delay_min"].as<int>();
    if (!s["backup_heat_min_rise"].isNull()) backupHeatMinTempRise = s["backup_heat_min_rise"].as<float>();
    if (!s["backup_heat_max_drop"].isNull()) backupHeatMaxTempDrop = s["backup_heat_max_drop"].as<float>();
    if (!s["hydronic_heat_enabled"].isNull()) hydronicHeatingEnabled = s["hydronic_heat_enabled"].as<bool>();
    if (!s["eu_humidity_enabled"].isNull()) euHumidityControlEnabled = s["eu_humidity_enabled"].as<bool>();
    if (!s["eu_humidity_relay"].isNull()) {
        int index = relayNameIndex(EU_HUMIDITY_RELAY_NAMES, s["eu_humidity_relay"].as<const char*>());
        if (index >= 0) euHumidityRelaySelection = index;
    }
    if (!s["eu_humidity_deadband"].isNull()) euHumidityDeadband = s["eu_humidity_deadband"].as<float>();
    if (!s["shower_mode_enabled"].isNull()) {
        showerModeEnabled = s["shower_mode_enabled"].as<bool>();
        if (!showerModeEnabled) applyShowerMode(false);
    }
    if (!s["temp_offset"].isNull()) {
        float offset = s["temp_offset"].as<float>();
        tempOffset = constrain(useFahrenheit ? offset / 1.8f : offset, -10.0f, 10.0f);
    }
    if (!s["humidity_offset"].isNull()) humidityOffset = s["humidity_offset"].as<float>();
    if (!s["region"].isNull()) thermostatRegion = s["region"].as<String>();
    if (!s["time_zone"].isNull()) {
        timeZone = s["time_zone"].as<String>();
        setenv("TZ", timeZone.c_str(), 1);
        tzset();
    }
    // Validation already rejected conflicts; these keep the relay rules authoritative.
    if (stage2HeatingEnabled && reversingValveEnabled) stage2HeatingEnabled = false;
    enforceBackupHeatRelayConflicts();
    enforceEUHumidityRelayConflicts();
    controlRelays(currentTemp);
    setDisplayUpdateFlag();
    haRequestSave();
    publishHomeAssistantDiscovery(); // keep MQTT discovery (if enabled) in step with features/unit, as /set does
    return true;
}

static void haApplyPeriod(JsonObjectConst p, SchedulePeriod& period) {
    if (!p["hour"].isNull()) period.hour = p["hour"].as<int>();
    if (!p["minute"].isNull()) period.minute = p["minute"].as<int>();
    if (!p["heat"].isNull()) period.heatTemp = p["heat"].as<float>();
    if (!p["cool"].isNull()) period.coolTemp = p["cool"].as<float>();
    if (!p["auto"].isNull()) period.autoTemp = p["auto"].as<float>();
    if (!p["active"].isNull()) period.active = p["active"].as<bool>();
}

static bool haApplySchedule(JsonObjectConst body) {
    bool todayEdited = false;
    int today = getCurrentDayOfWeek();
    for (JsonObjectConst day : body["days"].as<JsonArrayConst>()) {
        int index = haDayIndex(day);
        if (index < 0) continue;
        if (!day["enabled"].isNull()) weekSchedule[index].enabled = day["enabled"].as<bool>();
        if (!day["day_period"].isNull()) haApplyPeriod(day["day_period"].as<JsonObjectConst>(), weekSchedule[index].day);
        if (!day["night_period"].isNull()) haApplyPeriod(day["night_period"].as<JsonObjectConst>(), weekSchedule[index].night);
        if (index == today) todayEdited = true;
    }
    bool enabledChanged = !body["enabled"].isNull() && applyScheduleEnabled(body["enabled"].as<bool>());
    if (todayEdited && !enabledChanged) {
        reapplyCurrentSchedulePeriod();
    }
    debugLog("SCHEDULE: Updated via local API\n");
    haRequestSave();
    return true;
}

static void haRebootSoon() {
    xTaskCreate([](void*) {
        vTaskDelay(pdMS_TO_TICKS(500));
        ESP.restart();
    }, "ha_reboot", 2048, nullptr, 1, nullptr);
}

static bool haApplyAction(JsonObjectConst body) {
    String action = body["action"].as<String>();
    if (action == "reboot") {
        if (systemRebootInProgress) return false;
        systemRebootInProgress = true;
        debugLog("[HA API] Reboot requested\n");
        if (haSavePending) {
            haSavePending = false;
            saveSettings();
        }
        haRebootSoon();
    } else if (action == "refresh_weather") {
        weather.forceUpdate();
    }
    return false;
}

// /control (legacy web endpoint): lenient, clamps instead of rejecting, saves immediately.
static bool haApplyLegacyControl(JsonObjectConst c) {
    if (!c["target_temp_heat"].isNull()) applySetpoint(SETPOINT_HEAT, c["target_temp_heat"].as<float>());
    if (!c["target_temp_cool"].isNull()) applySetpoint(SETPOINT_COOL, c["target_temp_cool"].as<float>());
    if (!c["target_temp_auto"].isNull()) applySetpoint(SETPOINT_AUTO, c["target_temp_auto"].as<float>());
    if (!c["temp_swing"].isNull()) tempSwing = constrain(c["temp_swing"].as<float>(), 0.2f, 3.0f);
    if (!c["mode"].isNull()) applyThermostatMode(c["mode"].as<String>());
    if (!c["fan_mode"].isNull()) applyFanMode(c["fan_mode"].as<String>());
    saveSettings();
    sendMQTTData();
    return true;
}

static void haProcessQueue() {
    if (!haCommandQueue) return;
    HaQueueItem item;
    bool anyChanged = false;
    while (xQueueReceive(haCommandQueue, &item, 0) == pdTRUE) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, *item.body);
        delete item.body;
        if (error || !doc.is<JsonObject>()) continue;
        JsonObjectConst obj = doc.as<JsonObjectConst>();
        switch (item.kind) {
            case HA_QUEUE_COMMAND:        anyChanged |= haApplyCommand(obj); break;
            case HA_QUEUE_SETTINGS:       anyChanged |= haApplySettings(obj); break;
            case HA_QUEUE_SCHEDULE:       anyChanged |= haApplySchedule(obj); break;
            case HA_QUEUE_ACTION:         haApplyAction(obj); break;
            case HA_QUEUE_LEGACY_CONTROL: anyChanged |= haApplyLegacyControl(obj); break;
        }
    }
    if (anyChanged) {
        updateDisplay(currentTemp, currentHumidity);
        mqttFeedbackNeeded = true;
        haStateDirty = true;
    }
}

// ---------------------------------------------------------------------------
// State cache + SSE publishing (main loop only)
// ---------------------------------------------------------------------------

static bool haIsSlowKey(const char* key) {
    return strcmp(key, "uptime_s") == 0 || strcmp(key, "free_heap") == 0 || strcmp(key, "wifi_rssi") == 0;
}

static void haPublishState() {
    static std::map<String, String> lastSent;
    static unsigned long lastSlowSend = 0;
    unsigned long now = millis();
    bool includeSlow = (now - lastSlowSend) >= HA_API_SLOW_KEY_INTERVAL_MS;
    if (includeSlow) lastSlowSend = now;

    JsonDocument info;
    buildInfoJson(info);
    String infoText;
    serializeJson(info, infoText);

    // FNV-1a of /info: stable across reboots, changes whenever features/unit/limits change.
    uint32_t infoRev = 2166136261u;
    for (size_t i = 0; i < infoText.length(); i++) {
        infoRev = (infoRev ^ (uint8_t)infoText[i]) * 16777619u;
    }

    JsonDocument state;
    buildStateJson(state);
    state["info_rev"] = infoRev;
    String full;
    serializeJson(state, full);

    if (haCacheMutex && xSemaphoreTake(haCacheMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        haCachedState = full;
        haCachedInfo = infoText;
        xSemaphoreGive(haCacheMutex);
    }
    int modeIndex = haModeIndex(thermostatMode.c_str());
    haCachedModeIndex = modeIndex < 0 ? 0 : modeIndex;

    JsonDocument delta;
    for (JsonPair kv : state.as<JsonObject>()) {
        const char* key = kv.key().c_str();
        if (haIsSlowKey(key) && !includeSlow) continue;
        String value;
        serializeJson(kv.value(), value);
        auto it = lastSent.find(key);
        if (it == lastSent.end() || it->second != value) {
            delta[key] = kv.value();
            lastSent[key] = value;
        }
    }
    if (delta.size() > 0 && haEvents.count() > 0) {
        String text;
        serializeJson(delta, text);
        haEvents.send(text.c_str(), "update", ++haEventId);
    }
}

// mDNS advertises _esp32thermostat._tcp only while the API is enabled and WiFi is up.
static void haUpdateMdns() {
    bool wanted = haApiEnabled && WiFi.status() == WL_CONNECTED;
    bool restart = haConfigChanged;
    if (restart) {
        haConfigChanged = false;
        haEvents.close(); // clients must reconnect with the current token
    }
    if (haMdnsRunning && (!wanted || restart)) {
        MDNS.end();
        haMdnsRunning = false;
        haLastMdnsAttempt = 0;
    }
    if (!wanted || haMdnsRunning) return;
    unsigned long now = millis();
    if (haLastMdnsAttempt != 0 && now - haLastMdnsAttempt < HA_API_MDNS_RETRY_MS) return;
    haLastMdnsAttempt = now;
    if (!MDNS.begin(hostname.c_str())) {
        debugLog("[HA API] mDNS start failed\n");
        return;
    }
    MDNS.addService("esp32thermostat", "tcp", 80);
    MDNS.addServiceTxt("esp32thermostat", "tcp", "id", haDeviceId().c_str());
    MDNS.addServiceTxt("esp32thermostat", "tcp", "name", hostname.c_str());
    MDNS.addServiceTxt("esp32thermostat", "tcp", "fw", sw_version.c_str());
    MDNS.addServiceTxt("esp32thermostat", "tcp", "model", MQTT_MODEL);
    MDNS.addServiceTxt("esp32thermostat", "tcp", "api", String(HA_API_VERSION).c_str());
    MDNS.addServiceTxt("esp32thermostat", "tcp", "unit", useFahrenheit ? "F" : "C");
    haMdnsRunning = true;
    debugLog("[HA API] mDNS advertising %s.local (_esp32thermostat._tcp)\n", hostname.c_str());
}

// Runs every loop pass, including while the settings/WiFi screens are open.
void haApiServiceTick() {
    static unsigned long lastStateTime = 0;
    static unsigned long lastPingTime = 0;
    unsigned long now = millis();

    haUpdateMdns();

    if (haStateDirty || now - lastStateTime >= HA_API_STATE_INTERVAL_MS) {
        haStateDirty = false;
        lastStateTime = now;
        haPublishState();
    }
    if (now - lastPingTime >= HA_API_PING_INTERVAL_MS) {
        lastPingTime = now;
        if (haEvents.count() > 0) {
            haEvents.send("{}", "ping", ++haEventId);
        }
    }
    if (haSavePending && (long)(now - haSaveDueAt) >= 0) {
        haSavePending = false;
        saveSettings();
    }
}

// Applies queued requests; only called when the normal thermostat screen is active.
void haApiLoop() {
    haProcessQueue();
    haApiServiceTick();
}

// ---------------------------------------------------------------------------
// Route registration
// ---------------------------------------------------------------------------

AsyncMiddlewareFunction haAuthMiddleware([](AsyncWebServerRequest* request, ArMiddlewareNext next) {
    if (!haApiEnabled) {
        haSendError(request, 403, "api_disabled", nullptr, "Enable the Home Assistant API in the thermostat settings");
        return;
    }
    const AsyncWebHeader* auth = request->getHeader("Authorization");
    if (!auth || !haTokenMatches(auth->value())) {
        haSendError(request, 401, "unauthorized");
        return;
    }
    next();
});

AsyncMiddlewareFunction haSseLimitMiddleware([](AsyncWebServerRequest* request, ArMiddlewareNext next) {
    if (haEvents.count() >= HA_API_MAX_SSE_CLIENTS) {
        haSendError(request, 503, "too_many_clients");
        return;
    }
    next();
});

static void haAddJsonRoute(const char* uri, WebRequestMethod method, size_t maxLength,
                           std::function<void(AsyncWebServerRequest*, JsonObjectConst)> handler) {
    AsyncCallbackJsonWebHandler* route = new AsyncCallbackJsonWebHandler(uri,
        [handler](AsyncWebServerRequest* request, JsonVariant& json) {
            if (!json.is<JsonObject>()) {
                haSendError(request, 400, "invalid", nullptr, "body must be a JSON object");
                return;
            }
            handler(request, json.as<JsonObjectConst>());
        });
    route->setMethod(method);
    route->setMaxContentLength(maxLength);
    route->addMiddleware(&haAuthMiddleware);
    server.addHandler(route);
}

void haApiRegisterRoutes() {
    server.on("/api/v1/info", HTTP_GET, [](AsyncWebServerRequest* request) {
        haSendCached(request, haCachedInfo);
    }).addMiddleware(&haAuthMiddleware);

    server.on("/api/v1/state", HTTP_GET, [](AsyncWebServerRequest* request) {
        haSendCached(request, haCachedState);
    }).addMiddleware(&haAuthMiddleware);

    server.on("/api/v1/schedule", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        buildScheduleJson(doc);
        String body;
        serializeJson(doc, body);
        haSendJson(request, 200, body);
    }).addMiddleware(&haAuthMiddleware);

    haAddJsonRoute("/api/v1/command", HTTP_POST, 1024, [](AsyncWebServerRequest* request, JsonObjectConst body) {
        HaValidation v = haValidateCommand(body);
        if (!v.ok()) return haReplyValidation(request, v);
        haEnqueueOrBusy(request, HA_QUEUE_COMMAND, body);
    });

    haAddJsonRoute("/api/v1/settings", HTTP_POST, 1024, [](AsyncWebServerRequest* request, JsonObjectConst body) {
        HaValidation v = haValidateSettings(body);
        if (!v.ok()) return haReplyValidation(request, v);
        haEnqueueOrBusy(request, HA_QUEUE_SETTINGS, body);
    });

    haAddJsonRoute("/api/v1/schedule", HTTP_PUT, 4096, [](AsyncWebServerRequest* request, JsonObjectConst body) {
        HaValidation v = haValidateSchedule(body);
        if (!v.ok()) return haReplyValidation(request, v);
        haEnqueueOrBusy(request, HA_QUEUE_SCHEDULE, body);
    });

    haAddJsonRoute("/api/v1/action", HTTP_POST, 256, [](AsyncWebServerRequest* request, JsonObjectConst body) {
        String action = body["action"].is<const char*>() ? body["action"].as<String>() : String();
        if (body.size() != 1 || (action != "reboot" && action != "refresh_weather")) {
            haSendError(request, 400, "invalid", "action", "must be one of reboot, refresh_weather");
            return;
        }
        if (action == "refresh_weather" && weatherSource == 0) {
            haSendError(request, 409, "conflict", "action", "weather is disabled");
            return;
        }
        haEnqueueOrBusy(request, HA_QUEUE_ACTION, body);
    });

    haEvents.onConnect([](AsyncEventSourceClient* client) {
        String state = haCachedCopy(haCachedState);
        client->send(state.c_str(), "state", ++haEventId);
    });
    haEvents.addMiddleware(&haAuthMiddleware);
    haEvents.addMiddleware(&haSseLimitMiddleware);
    server.addHandler(&haEvents);

    // Web UI helper (same trust level as the rest of the unauthenticated settings UI)
    server.on("/ha_api/regenerate_token", HTTP_POST, [](AsyncWebServerRequest* request) {
        haApiGenerateToken();
        saveSettings();
        haSendJson(request, 200, String("{\"token\":\"") + haApiToken + "\"}");
    });
}

// Call from setup() after loadSettings() and before server.begin().
void haApiBegin() {
    if (!haCommandQueue) haCommandQueue = xQueueCreate(HA_API_QUEUE_DEPTH, sizeof(HaQueueItem));
    if (!haCacheMutex) haCacheMutex = xSemaphoreCreateMutex();
    haApiRegisterRoutes();
    haPublishState();
}

#endif // HOME_ASSISTANT_API_H

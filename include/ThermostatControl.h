/*
 * ThermostatControl - shared command handling and state snapshot
 * Copyright (c) 2025 Jonn Taylor
 *
 * Every control path (touch screen, MQTT, web UI and the local Home Assistant
 * API) changes thermostat state through these functions so validation,
 * limits and schedule-override rules behave the same everywhere.
 *
 * Header-only: include exactly once from Main-Thermostat.cpp, after the
 * global variables and debugLog() are defined. The apply* functions touch the
 * relays and display state and must only be called from the main loop task.
 * They do not persist anything; callers decide when to save.
 */

#ifndef THERMOSTAT_CONTROL_H
#define THERMOSTAT_CONTROL_H

#include <ArduinoJson.h>

static const char* const SCHEDULE_DAY_NAMES[7] = {
    "sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"
};

enum SetpointTarget { SETPOINT_HEAT, SETPOINT_COOL, SETPOINT_AUTO };

float setpointMin() { return useFahrenheit ? 50.0f : 10.0f; }
float setpointMax() { return useFahrenheit ? 95.0f : 35.0f; }
float clampSetpoint(float value) { return constrain(value, setpointMin(), setpointMax()); }

bool isValidThermostatMode(const String& mode) {
    return mode == "off" || mode == "heat" || mode == "cool" || mode == "auto";
}

bool isValidFanMode(const String& mode) {
    return mode == "auto" || mode == "on" || mode == "cycle";
}

// Setpoint used by a thermostat mode; returns false for "off".
bool setpointTargetForMode(const String& mode, SetpointTarget& target) {
    if (mode == "heat") { target = SETPOINT_HEAT; return true; }
    if (mode == "cool") { target = SETPOINT_COOL; return true; }
    if (mode == "auto") { target = SETPOINT_AUTO; return true; }
    return false;
}

float& setpointRef(SetpointTarget target) {
    switch (target) {
        case SETPOINT_HEAT: return setTempHeat;
        case SETPOINT_COOL: return setTempCool;
        default: return setTempAuto;
    }
}

// Manual setpoint changes while the schedule is running start a temporary override.
bool startTemporaryOverrideIfScheduled() {
    if (!scheduleEnabled || scheduleOverride) return false;
    scheduleOverride = true;
    overrideEndTime = millis() + (scheduleOverrideDuration * 60000UL);
    debugLog("SCHEDULE: Manual temperature change triggered temporary override\n");
    return true;
}

bool applyThermostatMode(const String& mode) {
    if (!isValidThermostatMode(mode) || mode == thermostatMode) return false;
    debugLog("[CONTROL] Mode %s -> %s\n", thermostatMode.c_str(), mode.c_str());
    thermostatMode = mode;
    lastModeSwitchTime = millis();
    controlRelays(currentTemp);
    setDisplayUpdateFlag();
    return true;
}

bool applyFanMode(const String& mode) {
    if (!isValidFanMode(mode) || mode == fanMode) return false;
    debugLog("[CONTROL] Fan mode %s -> %s\n", fanMode.c_str(), mode.c_str());
    fanMode = mode;
    controlRelays(currentTemp);
    setDisplayUpdateFlag();
    return true;
}

// Clamps to the unit-aware limits. Returns true if the setpoint changed.
bool applySetpoint(SetpointTarget target, float value) {
    value = clampSetpoint(value);
    float& setpoint = setpointRef(target);
    if (setpoint == value) return false;
    setpoint = value;
    debugLog("[CONTROL] Setpoint %s = %.1f\n",
             target == SETPOINT_HEAT ? "heat" : (target == SETPOINT_COOL ? "cool" : "auto"), value);
    startTemporaryOverrideIfScheduled();
    controlRelays(currentTemp);
    setDisplayUpdateFlag();
    return true;
}

// Sets the setpoint for the current mode. Returns false when the mode is off or nothing changed.
bool applyActiveSetpoint(float value) {
    SetpointTarget target;
    if (!setpointTargetForMode(thermostatMode, target)) return false;
    return applySetpoint(target, value);
}

// Legacy retained topics published by the touch +/- buttons (kept for existing MQTT setups).
void publishLegacySetpointTopic(SetpointTarget target) {
    if (!mqttEnabled || !mqttClient.connected()) return;
    const char* topic = target == SETPOINT_HEAT ? "thermostat/setTempHeat"
                      : (target == SETPOINT_COOL ? "thermostat/setTempCool" : "thermostat/setTempAuto");
    mqttClient.publish(topic, String(setpointRef(target)).c_str(), true);
}

// Forces checkSchedule() to re-apply the current period's temperatures now.
void reapplyCurrentSchedulePeriod() {
    if (!scheduleEnabled || scheduleOverride) return;
    activePeriod = "manual";
    checkSchedule();
}

bool applyScheduleEnabled(bool enabled) {
    if (enabled == scheduleEnabled) return false;
    scheduleEnabled = enabled;
    scheduleOverride = false;
    overrideEndTime = 0;
    activePeriod = "manual";
    debugLog("SCHEDULE: enabled=%s\n", enabled ? "true" : "false");
    if (enabled) {
        checkSchedule();
    }
    setDisplayUpdateFlag();
    return true;
}

// type: "resume", "temporary" or "permanent". Ignored while the schedule is disabled.
bool applyScheduleOverride(const String& type) {
    if (!scheduleEnabled) return false;
    if (type == "resume") {
        if (!scheduleOverride) return false;
        scheduleOverride = false;
        overrideEndTime = 0;
        debugLog("SCHEDULE: Override cleared, resuming schedule\n");
        reapplyCurrentSchedulePeriod();
    } else if (type == "temporary") {
        scheduleOverride = true;
        overrideEndTime = millis() + (scheduleOverrideDuration * 60000UL);
        debugLog("SCHEDULE: Temporary override (%lu min)\n", scheduleOverrideDuration);
    } else if (type == "permanent") {
        if (scheduleOverride && overrideEndTime == 0) return false;
        scheduleOverride = true;
        overrideEndTime = 0;
        debugLog("SCHEDULE: Permanent override\n");
    } else {
        return false;
    }
    setDisplayUpdateFlag();
    return true;
}

bool applyShowerMode(bool active) {
    if (active && !showerModeEnabled) return false;
    if (active == showerModeActive) return false;
    showerModeActive = active;
    if (active) {
        showerModeStartTime = millis();
    }
    debugLog("[SHOWER MODE] %s - duration %d minutes\n", active ? "Activated" : "Deactivated", showerModeDuration);
    setDisplayUpdateFlag();
    return true;
}

// ---------------------------------------------------------------------------
// State snapshot (shared by the local API, SSE and future consumers)
// ---------------------------------------------------------------------------

// Fixed-decimal formatting keeps JSON output stable for change detection.
static void setFixedOrNull(JsonObject obj, const char* key, bool available, float value, unsigned int decimals = 1) {
    if (available && !isnan(value)) {
        obj[key] = serialized(String(value, decimals));
    } else {
        obj[key] = nullptr;
    }
}

String currentHvacAction() {
    bool heatRelayOn = digitalRead(HEAT_RELAY_1_PIN) == HIGH || digitalRead(HEAT_RELAY_2_PIN) == HIGH;
    bool coolRelayOn = digitalRead(COOL_RELAY_1_PIN) == HIGH || digitalRead(COOL_RELAY_2_PIN) == HIGH ||
                       digitalRead(PUMP_RELAY_PIN) == HIGH;
    bool fanRelayOn = digitalRead(FAN_RELAY_PIN) == HIGH;
    if (backupHeatActive || (thermostatMode != "off" && heatRelayOn)) return "heating";
    if (thermostatMode != "off" && coolRelayOn) return "cooling";
    if (fanRelayOn) return "fan";
    return thermostatMode == "off" ? "off" : "idle";
}

bool sensorHasPressure() { return activeSensor == SENSOR_BME280 || activeSensor == SENSOR_BME680; }
bool sensorHasAirQuality() { return activeSensor == SENSOR_BME680; }
bool sensorHasHumidity() { return activeSensor != SENSOR_NONE; }

long overrideRemainingMinutes() {
    if (!scheduleOverride || overrideEndTime == 0) return -1;
    unsigned long now = millis();
    if (now >= overrideEndTime) return 0;
    return (long)((overrideEndTime - now + 59999UL) / 60000UL);
}

long showerRemainingMinutes() {
    if (!showerModeActive) return 0;
    unsigned long total = showerModeDuration * 60000UL;
    unsigned long elapsed = millis() - showerModeStartTime;
    if (elapsed >= total) return 0;
    return (long)((total - elapsed + 59999UL) / 60000UL);
}

// Builds the full device state. Must run on the main loop task (reads String globals).
void buildStateJson(JsonDocument& doc) {
    JsonObject root = doc.to<JsonObject>();
    bool sensorOk = activeSensor != SENSOR_NONE;

    setFixedOrNull(root, "current_temperature", sensorOk, currentTemp);
    setFixedOrNull(root, "current_humidity", sensorHasHumidity(), currentHumidity);
    setFixedOrNull(root, "pressure_hpa", sensorHasPressure(), currentPressure);
    setFixedOrNull(root, "air_quality", sensorHasAirQuality(), currentAirQuality, 0);
    setFixedOrNull(root, "gas_resistance_kohm", sensorHasAirQuality(), currentGasResistance);
    setFixedOrNull(root, "hydronic_supply_temperature", hydronicHeatingEnabled && ds18b20SensorPresent, hydronicTemp);
    setFixedOrNull(root, "hydronic_return_temperature", hydronicHeatingEnabled && ds18b20ReturnSensorPresent, hydronicReturnTemp);

    root["mode"] = thermostatMode;
    root["fan_mode"] = fanMode;
    root["action"] = currentHvacAction();
    SetpointTarget target;
    if (setpointTargetForMode(thermostatMode, target)) {
        root["target_temperature"] = serialized(String(setpointRef(target), 1));
    } else {
        root["target_temperature"] = nullptr;
    }
    root["target_temp_heat"] = serialized(String(setTempHeat, 1));
    root["target_temp_cool"] = serialized(String(setTempCool, 1));
    root["target_temp_auto"] = serialized(String(setTempAuto, 1));

    root["stage1_active"] = stage1Active;
    root["stage2_active"] = stage2Active;
    root["backup_heat_active"] = backupHeatActive;
    root["hydronic_lockout"] = hydronicLockout;
    root["eu_humidity_demand"] = euHumidityDemandActive;

    JsonObject relays = root["relays"].to<JsonObject>();
    relays["heat1"] = digitalRead(HEAT_RELAY_1_PIN) == HIGH;
    relays["heat2"] = digitalRead(HEAT_RELAY_2_PIN) == HIGH;
    relays["cool1"] = digitalRead(COOL_RELAY_1_PIN) == HIGH;
    relays["cool2"] = digitalRead(COOL_RELAY_2_PIN) == HIGH;
    relays["fan"] = digitalRead(FAN_RELAY_PIN) == HIGH;
    relays["pump"] = digitalRead(PUMP_RELAY_PIN) == HIGH;

    JsonObject schedule = root["schedule"].to<JsonObject>();
    schedule["enabled"] = scheduleEnabled;
    schedule["override"] = scheduleEnabled && scheduleOverride;
    if (!scheduleEnabled || !scheduleOverride) {
        schedule["override_type"] = "none";
        schedule["override_remaining_min"] = nullptr;
    } else if (overrideEndTime == 0) {
        schedule["override_type"] = "permanent";
        schedule["override_remaining_min"] = nullptr;
    } else {
        schedule["override_type"] = "temporary";
        schedule["override_remaining_min"] = overrideRemainingMinutes();
    }
    schedule["active_period"] = getCurrentPeriod();
    JsonArray daysEnabled = schedule["days_enabled"].to<JsonArray>();
    for (int day = 0; day < 7; day++) {
        daysEnabled.add(weekSchedule[day].enabled);
    }

    JsonObject shower = root["shower"].to<JsonObject>();
    shower["active"] = showerModeActive;
    shower["remaining_min"] = showerRemainingMinutes();

    if (ld2410Connected) {
        root["motion"] = motionDetected;
    } else {
        root["motion"] = nullptr;
    }

    JsonObject settings = root["settings"].to<JsonObject>();
    settings["temp_swing"] = serialized(String(tempSwing, 1));
    settings["auto_temp_swing"] = serialized(String(autoTempSwing, 1));
    settings["stage1_min_runtime"] = stage1MinRuntime;
    settings["stage2_temp_delta"] = serialized(String(stage2TempDelta, 1));
    settings["fan_minutes_per_hour"] = fanMinutesPerHour;
    settings["shower_duration"] = showerModeDuration;
    settings["display_brightness"] = currentBrightness;
    settings["hydronic_temp_low"] = serialized(String(hydronicTempLow, 1));
    settings["hydronic_temp_high"] = serialized(String(hydronicTempHigh, 1));
    settings["eu_humidity_setpoint"] = serialized(String(euHumiditySetpoint, 0));

    root["wifi_rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    root["uptime_s"] = millis() / 1000UL;
    root["free_heap"] = (uint32_t)ESP.getFreeHeap();
}

#endif // THERMOSTAT_CONTROL_H

# Settings Save Mechanism Analysis

## Overview
The firmware has three independent save pathways plus a periodic check mechanism. Intermittent save failures may stem from timing issues, missing flags, or I2C bus contention.

---

## Save Pathways

### 1. **Web Interface (`/set` endpoint)** - Lines 3571-3791
**Status**: ✅ SAVES IMMEDIATELY
- Collects all form parameters
- Updates global variables
- Calls `saveSettings()` once at line 3770
- Calls `sendMQTTData()` for broadcast

**Potential Issues**:
- ❌ No retry mechanism if `saveSettings()` fails
- ❌ No error response to client about save success/failure
- ✅ Good: Single centralized `saveSettings()` call

---

### 2. **Web `/control` endpoint** - Lines 3865-3907
**Status**: ✅ SAVES IMMEDIATELY  
- Simpler parameter set (setTemps, tempSwing, modes only)
- Calls `saveSettings()` at line 3906
- Calls `sendMQTTData()` for broadcast

**Potential Issues**:
- ❌ Same—no retry or error feedback

---

### 3. **Touch/Display Buttons** - Lines 2162-2340 (in `handleButtonPress()`)
**Status**: ⚠️ SAVES WITH TIMING ISSUES

#### "+" Button (line 2163-2221):
```cpp
if (scheduleEnabled && !scheduleOverride) {
    scheduleOverride = true;
    overrideEndTime = millis() + (scheduleOverrideDuration * 60000UL);
    debugLog("SCHEDULE: Override enabled due to manual temperature adjustment\n");
    saveScheduleSettings();  // ← Saves override state
}
// ... update setTemp...
saveSettings();  // ← Saves temperature setpoint
sendMQTTData();
updateDisplay(currentTemp, currentHumidity);
```

#### "-" Button (line 2241-2340):
```cpp
if (scheduleEnabled && !scheduleOverride) {
    scheduleOverride = true;
    overrideEndTime = millis() + (scheduleOverrideDuration * 60000UL);
    debugLog("SCHEDULE: Override enabled due to manual temperature adjustment\n");
    saveScheduleSettings();  // ← Saves override state
}
// ... update setTemp...
saveSettings();  // ← Saves temperature setpoint
sendMQTTData();
updateDisplay(currentTemp, currentHumidity);
```

**Potential Issues**:
- ⚠️ **TWO separate save calls** (`saveScheduleSettings()` + `saveSettings()`) 
- ⚠️ If first save is interrupted (touch released, power flicker), second may not complete
- ⚠️ No acknowledgment to user that save succeeded
- ⚠️ Display update happens AFTER saves—if display task preempts, MQTT and settings may race

---

### 4. **MQTT Temperature Commands** - Lines 2637-2680
**Status**: ⚠️ SAVES WITH FLAG-BASED DELAY

```cpp
bool settingsNeedSaving = false;
bool scheduleNeedsSaving = false;

// When temp changes:
if (tempChanged && scheduleEnabled && !scheduleOverride) {
    scheduleOverride = true;
    overrideEndTime = millis() + (scheduleOverrideDuration * 60000UL);
    debugLog("SCHEDULE: MQTT temperature change triggered override\n");
    scheduleNeedsSaving = true;
}

// ... later in function:
if (settingsNeedSaving) {
    debugLog("Saving settings changed via MQTT\n");
    saveSettings();
    updateDisplay(currentTemp, currentHumidity);
    mqttFeedbackNeeded = true;
}

if (scheduleNeedsSaving) {
    debugLog("Saving schedule settings changed via MQTT\n");
    saveScheduleSettings();
}
```

**Potential Issues**:
- ⚠️ **Uses flags** that must be evaluated later
- ⚠️ If `mqttCallback()` is interrupted or preempted before flags are checked, saves may be skipped
- ⚠️ **Order dependency**: `settingsNeedSaving` checked BEFORE `scheduleNeedsSaving`—both must be true to avoid logical gaps
- ⚠️ **No verification** that settings actually made it to NVS after save

---

### 5. **Settings UI (Touch Menu)** - `include/SettingsUI.h` lines 546, 617
**Status**: ⚠️ SAVES BUT MISSING IN SOME PATHS

- `saveSettings()` called at lines 546 and 617 in settings menu handler
- **But**: No indication which menu items trigger saves

**Potential Issues**:
- ❓ Unclear which settings menu changes persist
- ❓ Possible menu items save immediately vs. on exit

---

### 6. **Schedule Web Interface (`/schedule_set`)** - Lines 4075-4166
**Status**: ⚠️ USES FLAG-BASED SAVE

```cpp
bool settingsChanged = false;
// ... update schedule...
if (settingsChanged) {
    scheduleUpdatedFlag = true;
    saveScheduleSettings();  // ← Immediate call
    debugLog("SCHEDULE: Settings updated via web interface\n");
}
```

**Potential Issues**:
- ⚠️ Also sets `scheduleUpdatedFlag = true`
- ⚠️ In `saveSettings()` (line 4750), this flag triggers **another** `saveScheduleSettings()` call
- ⚠️ **Double-save possible**: Once here, potentially again if `saveSettings()` is called later by main loop

---

## Core Issues Identified

### **Issue #1: Race Condition in MQTT Handler**
- Lines 2605-2712: Uses `settingsNeedSaving` flag checked after all parameter parsing
- If `mqttCallback()` is preempted mid-function, the conditional save may never execute
- **Fix**: Call `saveSettings()` directly after each parameter change, not conditionally

### **Issue #2: Missing Sync Point**
- Display buttons call BOTH `saveScheduleSettings()` AND `saveSettings()` sequentially
- No atomic/transactional guarantee—if device reboots between the two, one may be lost
- **Fix**: Consolidate schedule state into `saveSettings()` to single call

### **Issue #3: Schedule Update Flag Double-Save**
- `/schedule_set` sets `scheduleUpdatedFlag = true` AND calls `saveScheduleSettings()` 
- `saveSettings()` checks this flag (line 4750) and calls `saveScheduleSettings()` AGAIN
- **Fix**: Remove flag logic; always save directly

### **Issue #4: No Error Handling or Verification**
- `preferences.put*()` calls have no return value checks
- No verification that data was actually written to NVS
- **Fix**: Add error checking and retry logic to critical saves

### **Issue #5: I2C Bus Contention**
- Sensor reads and Preferences access both use I2C/NVS
- In dual-core system: Core 0 (UI) and Core 1 (sensors) may collide
- **Fix**: Use semaphores to coordinate I2C/NVS access

---

## Recommended Fixes

### **Priority 1: Immediate Consolidation**
1. Remove `scheduleUpdatedFlag` logic—always save schedule to NVS directly
2. Merge `saveScheduleSettings()` calls into main `saveSettings()` or call both atomically

### **Priority 2: Add Verification**
```cpp
bool saveSettings() {
    // ... existing code ...
    
    // Add sync/commit (if Preferences supports)
    debugLog("SETTINGS: Attempting to save to NVS\n");
    
    // Verify critical values were saved
    float testSetHeat = preferences.getFloat("setHeat", -999);
    if (testSetHeat != setTempHeat) {
        debugLog("ERROR: Settings verification failed—save may not have persisted!\n");
        return false;
    }
    
    debugLog("SETTINGS: Successfully saved and verified\n");
    return true;
}
```

### **Priority 3: Atomic MQTT Saves**
Replace flag-based logic with direct save calls:
```cpp
void mqttCallback(...) {
    // ... parse parameters ...
    
    if (tempChanged) {
        saveSettings();  // ← Direct, immediate
    }
    if (scheduleChanged) {
        saveScheduleSettings();  // ← Direct, immediate
    }
}
```

### **Priority 4: Centralize Display Button Saves**
```cpp
// In handleButtonPress() for +/- buttons:
if (settingsChanged) {
    debugLog("SETTINGS: Manual adjustment—saving...\n");
    
    // Save both in sequence without delay
    if (scheduleOverride) saveScheduleSettings();
    saveSettings();
    
    sendMQTTData();
    updateDisplay(currentTemp, currentHumidity);
    
    debugLog("SETTINGS: Save complete\n");
}
```

### **Priority 5: Add Save Synchronization**
```cpp
// Define a save semaphore at global scope
static SemaphoreHandle_t saveSemaphore = xSemaphoreCreateBinary();

bool atomicSaveSettings() {
    if (xSemaphoreTake(saveSemaphore, pdMS_TO_TICKS(500))) {
        bool result = saveSettings();
        xSemaphoreGive(saveSemaphore);
        return result;
    }
    debugLog("ERROR: Save operation timeout\n");
    return false;
}
```

---

## Testing Checklist

- [ ] Set temperature via touch +/- button → verify persistent after reboot
- [ ] Set temperature via web `/set` → verify persistent after reboot
- [ ] Set temperature via MQTT → verify persistent after reboot
- [ ] Enable schedule, adjust setpoint → verify override persists and expires correctly
- [ ] Edit schedule times/temps via web → verify persistent after reboot
- [ ] Rapid button presses → verify all saves complete without loss
- [ ] Pull power during save operation → verify most recent state recovers
- [ ] Enable MQTT + touch buttons simultaneously → verify no race condition

---

## Conclusion

The save mechanism is **fragmented across multiple independent pathways** with:
- **No atomic operations** (multi-step saves can be interrupted)
- **No verification** that saves actually persisted
- **Timing-dependent bugs** (flags, delays, preemption)
- **No synchronization** between Core 0 (UI) and Core 1 (control)

Consolidating all saves into a single, atomic, verified function would eliminate most intermittent failures.

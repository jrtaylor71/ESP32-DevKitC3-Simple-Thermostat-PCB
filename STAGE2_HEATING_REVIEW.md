# Stage 2 Heating Logic Review

**Date:** January 20, 2026  
**Status:** ANALYSIS COMPLETE - Issues Identified

---

## Current Implementation

### Stage 2 Activation Logic (activateHeating function, line 3319-3327)

```cpp
// Check if it's time to activate stage 2 based on hybrid approach
else if (!stage2Active && 
         ((millis() - stage1StartTime) / 1000 >= stage1MinRuntime) && // Minimum run time condition
         (currentTemp < setTempHeat - tempSwing - stage2TempDelta) && // Temperature delta condition
         stage2HeatingEnabled) { // Check if stage 2 heating is enabled
    debugLog("[HVAC] Stage 2 HEATING activated\n");
    digitalWrite(HEAT_RELAY_2_PIN, HIGH); // Activate stage 2
    stage2Active = true;
}
```

### Stage 2 Deactivation

**Problem 1: Stage 2 Has NO Active Deactivation Logic**

Currently, stage 2 only deactivates via:
1. `turnOffAllRelays()` when heating is turned off entirely
2. Reset to false when switching modes (heat→cool)
3. Reset to false in OFF mode

**There is NO logic to deactivate stage 2 when:**
- Temperature rises above activation threshold (e.g., reaches setpoint)
- Stage 1 alone becomes sufficient to maintain temperature
- Overshoot occurs

### State Diagram Issue

```
ACTIVATION CONDITION:
  stage1Active=true AND
  (millis - stage1StartTime) >= stage1MinRuntime AND
  temp < (setHeat - swing - delta) AND
  stage2HeatingEnabled

DEACTIVATION CONDITION:
  ❌ MISSING - Currently only through turnOffAllRelays()
```

---

## Problems Identified

### Issue 1: Stage 2 Never Deactivates During Active Heating ⚠️

**Scenario:**
1. Temperature is 65°F
2. Setpoint is 72°F (swing=1°F, delta=2°F)
3. Activation threshold: 65°F < 69°F ✓ Stage 1 activates
4. After 5 minutes: Activation threshold: 65°F < 69°F ✓ Stage 2 activates
5. System heats to 71°F
6. **Stage 2 is STILL ON** - No deactivation logic!
7. System overshoots to 73°F
8. Finally `turnOffAllRelays()` called when temp >= 72°F
9. But stage 2 was running too long, causing overshoot

**Impact:** Excessive overshoot, uncomfortable temperature swings, energy waste

---

### Issue 2: Temperature Delta Calculation Confusing ⚠️

Current activation uses: `currentTemp < setTempHeat - tempSwing - stage2TempDelta`

**Example:**
- setTempHeat = 72°F
- tempSwing = 1°F  
- stage2TempDelta = 2°F
- Activation threshold = 72 - 1 - 2 = 69°F

**Question:** Why subtract tempSwing? This creates a double hysteresis:
- Heat turns ON at: 72 - 1 = 71°F ✓ (correct hysteresis)
- Stage 2 turns ON at: 72 - 1 - 2 = 69°F (aggressive)

This means stage 2 activates 2°F below heat activation, but there's NO corresponding deactivation logic.

---

### Issue 3: Stage 2 Doesn't Reset When Heating is Deactivated ⚠️

**Scenario:**
1. Stage 1 + Stage 2 both active, heating
2. Temperature reaches setpoint (72°F), `turnOffAllRelays()` called
3. stage2Active reset to false ✓
4. 10 seconds later: temperature dips to 70.9°F
5. `activateHeating()` called again
6. **Stage 1 starts fresh** with new stage1StartTime ✓
7. But stage1MinRuntime timer resets! ⚠️
8. If stage1MinRuntime=300s (5 min), need to wait 5 more minutes before stage 2 can activate again ✓

**This is actually correct behavior**, but the stage1MinRuntime is 300 seconds by default - means a 5-minute wait between stage 2 deactivation and re-activation. This could cause temperature to drop significantly.

---

### Issue 4: Missing Stage 2 Deactivation Threshold ⚠️

**Standard HVAC behavior:**
- Stage 2 activates when: temp < (setpoint - delta)
- Stage 2 deactivates when: temp >= setpoint (or >= setpoint - small_hysteresis)

**Current behavior:**
- Stage 2 activates when: temp < (setpoint - swing - delta)
- Stage 2 deactivates when: ALL relays off AND temp >= setpoint

**Missing:** Intermediate deactivation of stage 2 while stage 1 continues

---

### Issue 5: Reversing Valve Edge Case ⚠️

In reversing valve mode (heat pump):
```cpp
if (reversingValveEnabled) {
    if (!stage2Active) {
        debugLog("[HVAC] Reversing valve energized for HEAT mode\n");
        digitalWrite(HEAT_RELAY_2_PIN, HIGH);
        stage2Active = true; // Use stage2Active flag to track valve state
    }
}
```

**Problem:** Reversing valve is energized as soon as heating starts, not based on temperature delta. This is correct for heat pump operation, but:
- No deactivation logic until heating stops completely
- Reversing valve held ON continuously while in heat mode
- No ability to temporarily deactivate if not needed

---

## Recommended Fixes

### Fix 1: Add Stage 2 Deactivation Logic

Add in `activateHeating()` function to deactivate stage 2 when temperature recovers:

```cpp
// Handle stage 2 deactivation (hysteresis)
if (stage2Active && (currentTemp >= setTempHeat - tempSwing)) {
    // Temperature has risen back to reasonable level, deactivate stage 2
    // but keep stage 1 running to maintain setpoint
    debugLog("[HVAC] Stage 2 HEATING deactivated - temp %.1f >= %.1f\n", 
             currentTemp, (setTempHeat - tempSwing));
    digitalWrite(HEAT_RELAY_2_PIN, LOW);
    stage2Active = false;
}
```

**Location:** In `activateHeating()`, after stage 2 activation block (around line 3327)

**Benefit:** 
- Prevents overshoot by deactivating stage 2 when temperature recovers
- Maintains stage 1 for fine temperature control
- Reduces energy waste from excessive heating
- Improves comfort (fewer temperature swings)

---

### Fix 2: Clarify Temperature Delta Purpose

Document the stage2TempDelta parameter clearly:

**Current interpretation:**
- `stage2TempDelta = 2.0°F` means "activate stage 2 when 2°F below heat activation point"
- This creates: activation at setpoint - swing - delta

**Recommended:**
- Change to: "activate stage 2 when temperature falls stage2TempDelta below setpoint"
- Simpler calculation: `currentTemp < setTempHeat - stage2TempDelta`
- Remove the extra swing from stage 2 calculation

**New formula:**
```cpp
else if (!stage2Active && 
         ((millis() - stage1StartTime) / 1000 >= stage1MinRuntime) &&
         (currentTemp < setTempHeat - stage2TempDelta) &&  // Simpler: just delta
         stage2HeatingEnabled) {
    // ... activate
}
```

**Deactivation:**
```cpp
if (stage2Active && (currentTemp >= setTempHeat - (stage2TempDelta * 0.5))) {
    // Deactivate with hysteresis: half of the activation delta
    // ... deactivate
}
```

---

### Fix 3: Add Stage 2 Minimum Runtime

Prevent stage 2 rapid cycling by adding minimum runtime:

```cpp
static unsigned long stage2StartTime = 0;
static const unsigned long stage2MinRuntime = 60;  // 60 seconds minimum

// On activation:
if (!stage2Active && /* other conditions */) {
    stage2StartTime = millis();
    // ... activate
}

// On deactivation check:
if (stage2Active && 
    (millis() - stage2StartTime) / 1000 >= stage2MinRuntime &&
    (currentTemp >= setTempHeat - (stage2TempDelta * 0.5))) {
    // ... deactivate
}
```

**Benefit:** Prevents stage 2 from rapid on/off cycling which wastes energy and wears compressor (if heat pump)

---

### Fix 4: Document Configuration Parameters

Create clear documentation for:

| Parameter | Current Default | Purpose | Recommended Range |
|-----------|-----------------|---------|-------------------|
| `stage2TempDelta` | 2.0°F | How far below setpoint to activate stage 2 | 1.0 - 5.0°F |
| `stage1MinRuntime` | 300s (5 min) | Minimum heating before stage 2 allowed | 60-600s |
| `stage2HeatingEnabled` | false | Master enable/disable for stage 2 | bool |

**Missing:** 
- Stage 2 deactivation threshold/hysteresis
- Stage 2 minimum runtime before deactivation allowed
- Overshoot prevention mechanism

---

### Fix 5: Add Overshoot Protection

Monitor temperature velocity to prevent excessive heating:

```cpp
// In activateHeating(), after stage 2 deactivation check:
if (stage2Active) {
    static unsigned long lastTempCheckTime = 0;
    unsigned long currentTime = millis();
    
    // Check if temperature is rising quickly (possible overshoot)
    if ((currentTime - lastTempCheckTime) > 10000) {  // Every 10 seconds
        float tempChangePerMin = (currentTemp - lastTemp) * 6.0;  // Per minute
        
        if (tempChangePerMin > 2.0) {  // Rising > 2°F per minute
            debugLog("[HVAC] Overshoot detected! Rate=%.1f°F/min, deactivating stage 2\n", tempChangePerMin);
            digitalWrite(HEAT_RELAY_2_PIN, LOW);
            stage2Active = false;
        }
        
        lastTempCheckTime = currentTime;
    }
}
```

---

## Testing Checklist

- [ ] **Test 1:** Enable stage 2, set temp 10°F below setpoint, verify stage 2 activates after min runtime
- [ ] **Test 2:** Once stage 2 active, raise temperature 2°F, verify stage 2 deactivates while stage 1 continues
- [ ] **Test 3:** Set temperature at setpoint, verify no overshoot beyond setpoint
- [ ] **Test 4:** Rapidly toggle temperature up/down, verify stage 2 doesn't rapid-cycle
- [ ] **Test 5:** With reversing valve enabled, verify valve energizes/de-energizes properly
- [ ] **Test 6:** Disable stage 2 heating, verify only stage 1 activates and maintains temperature
- [ ] **Test 7:** Monitor actual HVAC equipment operation during heating cycle

---

## Summary

**Current Status:** Stage 2 heating can activate but cannot independently deactivate during active heating. It only turns off when ALL heating stops.

**Risk Level:** MEDIUM - System will heat properly but with potential overshoot and energy inefficiency.

**Recommended Action:** Implement Fix 1 (Add Stage 2 Deactivation Logic) as priority. This is the minimal change needed to prevent overshoot while maintaining stability.

**Estimated Implementation Time:** 30 minutes (code changes) + 1 hour (testing)

---

## Related Configuration

**Web Interface Location:** Settings → HVAC Advanced  
**Configuration Parameters:**
- `stage2HeatingEnabled` - Enable/disable checkbox
- `stage2TempDelta` - Temperature delta input (0.5 - 5.0°F)
- `stage1MinRuntime` - Minimum runtime input (60 - 600 seconds)

**MQTT Topics:**
- Publishing: `thermostat/stage2` = ON/OFF status
- Subscription: `thermostat/set` (no direct stage2 control, only manual setpoint)

---

## Next Steps

Recommend implementing Fix 1 (Stage 2 Deactivation Logic) to prevent overshooting. Would you like me to:

1. Implement the recommended fixes?
2. Add monitoring/debugging to understand current stage 2 behavior?
3. Create test scenarios to verify stage 2 works as expected?

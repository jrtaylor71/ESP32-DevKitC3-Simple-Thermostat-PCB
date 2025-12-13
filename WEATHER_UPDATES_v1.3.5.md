# Weather Module Updates - v1.3.6

## Overview
Ported weather improvements from Smart-Thermostat-Alt-Firmware to ESP32-DevKitC3 project. These changes fix the high/low temperature display for negative temperatures and add force update functionality.

## Files Modified

### 1. `src/Weather.cpp`

#### Change 1: Update Interval Default (Line 14)
**Before:**
```cpp
_updateInterval = 600000; // Default: 10 minutes
```

**After:**
```cpp
_updateInterval = 300000; // Default: 5 minutes
```

**Reason:** Faster weather updates for more responsive data

---

#### Change 2: Add OpenWeatherMap HTTP Timeout (Line ~139)
**Before:**
```cpp
http.begin(url);
Serial.println("[Weather] OWM - Sending HTTP GET request...");
int httpCode = http.GET();
```

**After:**
```cpp
http.begin(url);
http.setTimeout(5000);
Serial.println("[Weather] OWM - Sending HTTP GET request...");
int httpCode = http.GET();
```

**Reason:** Prevents indefinite hanging on HTTP requests

---

#### Change 3: Add Home Assistant HTTP Timeout (Line ~211)
**Before:**
```cpp
http.begin(url);
http.addHeader("Authorization", "Bearer " + _haToken);
http.addHeader("Content-Type", "application/json");
Serial.println("[Weather] HA - Headers set, sending HTTP GET request...");

int httpCode = http.GET();
```

**After:**
```cpp
http.begin(url);
http.setTimeout(5000);
http.addHeader("Authorization", "Bearer " + _haToken);
http.addHeader("Content-Type", "application/json");
Serial.println("[Weather] HA - Headers set, sending HTTP GET request...");

int httpCode = http.GET();
```

**Reason:** Prevents indefinite hanging on HTTP requests

---

#### Change 4: Fix High/Low Temperature Display for Negative Temps (Line ~479)
**Before:**
```cpp
// Draw high/low if available (moved right 5px, down 5px)
if (_data.tempHigh > 0 || _data.tempLow > 0) {
    tft.setTextSize(1);
    tft.setCursor(x + 10, y + 30);
    char hiLoStr[16];
    snprintf(hiLoStr, sizeof(hiLoStr), "H:%.0f L:%.0f", _data.tempHigh, _data.tempLow);
    tft.print(hiLoStr);
}
```

**After:**
```cpp
// Draw high/low if available (moved right 5px, down 5px)
// Check if values are not the sentinel value (-999) instead of checking > 0
if (_data.tempHigh > -900 || _data.tempLow > -900) {
    tft.setTextSize(1);
    tft.setCursor(x + 10, y + 30);
    char hiLoStr[16];
    snprintf(hiLoStr, sizeof(hiLoStr), "H:%.0f L:%.0f", _data.tempHigh, _data.tempLow);
    tft.print(hiLoStr);
}
```

**Reason:** **CRITICAL FIX** - Original condition `> 0` prevented display of high/low temps in cold climates. Values like -3.3°F and -5.7°F would never display. New condition checks against sentinel value (-999) instead.

---

#### Change 5: Add forceUpdate() Method (After Line 102)
**Added:**
```cpp
void Weather::forceUpdate() {
    _forceNextUpdate = true;
    update();
}
```

**Reason:** Allows manual triggering of weather updates from web UI or other sources

---

### 2. `include/Weather.h`

#### Change 1: Add forceUpdate() Method Declaration (Line ~54)
**Before:**
```cpp
// Update weather data
bool update();

// Get weather data
```

**After:**
```cpp
// Update weather data
bool update();
void forceUpdate();

// Get weather data
```

**Reason:** Declares the new force update method

---

### 3. `src/Main-Thermostat.cpp`

#### Change 1: Default Weather Update Interval (Line 131)
**Before:**
```cpp
int weatherUpdateInterval = 10; // Update interval in minutes (default 10)
```

**After:**
```cpp
int weatherUpdateInterval = 5; // Update interval in minutes (default 5)
```

**Reason:** Aligns with Weather.cpp default interval

---

#### Change 2: IP Address Logging - WiFi Init (Line ~938)
**Before:**
```cpp
if (WiFi.status() == WL_CONNECTED)
{
    Serial.println("\nConnected to WiFi");
    wifiConnected = true;
    
    // Only start web server and MQTT if connected
    handleWebRequests();
    server.begin();
```

**After:**
```cpp
if (WiFi.status() == WL_CONNECTED)
{
    Serial.println("\nConnected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    wifiConnected = true;
    
    // Only start web server and MQTT if connected
    handleWebRequests();
    server.begin();
```

**Reason:** Helps users easily find device on network

---

#### Change 3: IP Address Logging - checkWiFiConnection() #1 (Line ~1344)
**Before:**
```cpp
if (WiFi.status() == WL_CONNECTED)
{
    Serial.println("Connected to WiFi");
}
```

**After:**
```cpp
if (WiFi.status() == WL_CONNECTED)
{
    Serial.println("Connected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}
```

**Reason:** IP logging for all WiFi connection points

---

#### Change 4: IP Address Logging - checkWiFiConnection() #2 (Line ~1381)
**Before:**
```cpp
if (WiFi.status() == WL_CONNECTED)
{
    Serial.println("Connected to WiFi");
}
```

**After:**
```cpp
if (WiFi.status() == WL_CONNECTED)
{
    Serial.println("Connected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}
```

**Reason:** IP logging for all WiFi connection points

---

#### Change 5: IP Address Logging - enterWiFiCredentials() (Line ~1592)
**Before:**
```cpp
if (WiFi.status() == WL_CONNECTED)
{
    tft.fillScreen(COLOR_BACKGROUND);
    tft.setCursor(50, 100);
    tft.setTextColor(COLOR_SUCCESS, COLOR_BACKGROUND);
    tft.println("Connected!");
    tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    tft.setCursor(30, 130);
    tft.println("Restarting...");
    Serial.println("Connected to WiFi");
    delay(2000);
    ESP.restart();
}
```

**After:**
```cpp
if (WiFi.status() == WL_CONNECTED)
{
    tft.fillScreen(COLOR_BACKGROUND);
    tft.setCursor(50, 100);
    tft.setTextColor(COLOR_SUCCESS, COLOR_BACKGROUND);
    tft.println("Connected!");
    tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    tft.setCursor(30, 130);
    tft.println("Restarting...");
    Serial.println("Connected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    delay(2000);
    ESP.restart();
}
```

**Reason:** IP logging before restart

---

#### Change 6: Add /weather_refresh Endpoint (Before handleWebRequests() closing brace, ~Line 4444)
**Added:**
```cpp
// Weather refresh endpoint for manual force update
server.on("/weather_refresh", HTTP_POST, [](AsyncWebServerRequest *request) {
    weather.forceUpdate();
    request->send(200, "text/plain", "Weather update forced");
});
```

**Reason:** Allows web UI to trigger immediate weather updates

---

### 4. `include/WebPages.h`

#### Change 1: Add Force Update Button (Line ~743)
**Before:**
```cpp
html += "<div class='button-group' style='padding: 16px;'>";
html += "<button type='submit' class='btn btn-primary'>💾 Save Weather Settings</button>";
html += "</div>";
html += "</form>";
html += "</div>"; // End weather-content tab
```

**After:**
```cpp
html += "<div class='button-group' style='padding: 16px;'>";
html += "<button type='submit' class='btn btn-primary'>💾 Save Weather Settings</button>";
html += "<button type='button' class='btn btn-secondary' onclick='forceWeatherUpdate()'>🔄 Force Update Now</button>";
html += "</div>";
html += "</form>";
html += "</div>"; // End weather-content tab
```

**Reason:** Adds UI button to manually trigger weather updates

---

### 5. `include/WebInterface.h`

#### Change 1: Add forceWeatherUpdate() JavaScript Function (Line ~747)
**Added:**
```javascript
// Force weather update
function forceWeatherUpdate() {
    fetch('/weather_refresh', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
        }
    })
    .then(response => response.text())
    .then(data => {
        alert('Weather update triggered! Checking for new data...');
    })
    .catch(error => {
        console.error('Error:', error);
        alert('Failed to trigger weather update');
    });
}
```

**Reason:** JavaScript handler for the Force Update button that calls the /weather_refresh endpoint

---

## Summary of Improvements

| Feature | Impact |
|---------|--------|
| 5-second HTTP timeouts | Prevents indefinite hanging on network requests |
| Reduced update interval (10→5 min) | Fresher weather data |
| Negative temperature fix | **CRITICAL** - Allows high/low display in cold climates |
| Force update method | Manual trigger for immediate updates |
| IP address logging | Easier device discovery on network |
| Web UI force button | User-friendly manual refresh |

## Testing Checklist

- [ ] Compile and upload firmware
- [ ] Verify weather data displays with negative temps (high/low should show)
- [ ] Check serial monitor for IP address after WiFi connection
- [ ] Test "Force Update Now" button in Weather tab
- [ ] Verify weather updates every 5 minutes (default)
- [ ] Check that HTTP requests timeout after 5 seconds (no hanging)

## Notes

- The critical fix is the high/low temperature display condition change from `> 0` to `> -900`
- All changes maintain backward compatibility with existing configuration
- HTTP timeouts prevent the device from freezing on slow/unresponsive APIs
- The force update feature helps troubleshoot weather issues quickly

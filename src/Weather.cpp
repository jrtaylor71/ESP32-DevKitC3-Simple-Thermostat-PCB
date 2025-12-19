/*
 * Weather.cpp - Weather integration implementation
 */

#include "Weather.h"

// Color definitions (matching main thermostat colors)
#define COLOR_BACKGROUND   0x1082
#define COLOR_TEXT         0xFFFF
#define COLOR_PRIMARY      0x1976

Weather::Weather() {
    _source = WEATHER_DISABLED;
    _updateInterval = 300000; // Default: 5 minutes
    _useFahrenheit = true;
    _data.valid = false;
    _lastUpdateAttempt = 0;
    _forceNextUpdate = true; // Force first update regardless of interval
}

void Weather::begin() {
    Serial.println("[Weather] begin() called - initializing weather module");
    _data.valid = false;
    _lastError = "";
    Serial.printf("[Weather] Source: %d, Update interval: %lu ms\n", _source, _updateInterval);
}

void Weather::setSource(WeatherSource source) {
    Serial.printf("[Weather] setSource() called - changing from %d to %d\n", _source, source);
    _source = source;
}

void Weather::setOpenWeatherMapConfig(String apiKey, String city, String state, String countryCode) {
    Serial.printf("[Weather] setOpenWeatherMapConfig() - City: %s, State: %s, Country: %s, API Key: %s\n", 
                  city.c_str(), 
                  state.c_str(),
                  countryCode.c_str(), 
                  apiKey.isEmpty() ? "[NOT SET]" : "[SET]");
    _owmApiKey = apiKey;
    _owmCity = city;
    _owmState = state;
    _owmCountryCode = countryCode;
}

void Weather::setHomeAssistantConfig(String haUrl, String haToken, String entityId) {
    Serial.printf("[Weather] setHomeAssistantConfig() - URL: %s, Entity: %s, Token: %s\n", 
                  haUrl.c_str(), 
                  entityId.c_str(), 
                  haToken.isEmpty() ? "[NOT SET]" : "[SET]");
    _haUrl = haUrl;
    _haToken = haToken;
    _haEntityId = entityId;
}

void Weather::setUpdateInterval(unsigned long intervalMs) {
    _updateInterval = intervalMs;
}

void Weather::setUseFahrenheit(bool useFahrenheit) {
    _useFahrenheit = useFahrenheit;
}

bool Weather::update() {
    unsigned long currentTime = millis();
    
    // Check if it's time to update (unless forced)
    if (!_forceNextUpdate && currentTime - _lastUpdateAttempt < _updateInterval) {
        // Serial.printf("[Weather] update() - skipping, %lu ms until next update\n", 
        //               _updateInterval - (currentTime - _lastUpdateAttempt));
        return _data.valid;
    }
    
    Serial.printf("[Weather] update() - starting update (source: %d, forced: %d)\n", _source, _forceNextUpdate);
    _forceNextUpdate = false; // Clear force flag after first use
    _lastUpdateAttempt = currentTime;
    
    if (_source == WEATHER_DISABLED) {
        Serial.println("[Weather] update() - weather source is DISABLED");
        _lastError = "Weather disabled";
        return false;
    }
    
    bool success = false;
    if (_source == WEATHER_OPENWEATHERMAP) {
        Serial.println("[Weather] update() - calling updateFromOpenWeatherMap()");
        success = updateFromOpenWeatherMap();
    } else if (_source == WEATHER_HOMEASSISTANT) {
        Serial.println("[Weather] update() - calling updateFromHomeAssistant()");
        success = updateFromHomeAssistant();
    } else {
        Serial.printf("[Weather] update() - UNKNOWN source: %d\n", _source);
    }
    
    if (success) {
        _data.lastUpdate = currentTime;
        Serial.println("[Weather] update() - SUCCESS");
    } else {
        Serial.printf("[Weather] update() - FAILED: %s\n", _lastError.c_str());
    }
    
    return success;
}

void Weather::forceUpdate() {
    _forceNextUpdate = true;
    update();
}

bool Weather::updateFromOpenWeatherMap() {
    Serial.println("[Weather] updateFromOpenWeatherMap() - starting");
    
    if (_owmApiKey.isEmpty() || _owmCity.isEmpty()) {
        _lastError = "OpenWeatherMap not configured";
        Serial.printf("[Weather] OWM - Config error: API Key %s, City %s\n",
                      _owmApiKey.isEmpty() ? "EMPTY" : "OK",
                      _owmCity.isEmpty() ? "EMPTY" : "OK");
        return false;
    }
    
    HTTPClient http;
    String units = _useFahrenheit ? "imperial" : "metric";
    
    // URL encode the city name (replace spaces with %20)
    String encodedCity = _owmCity;
    encodedCity.replace(" ", "%20");
    
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + encodedCity;
    
    // Add state code if provided (for US cities)
    if (!_owmState.isEmpty()) {
        url += "," + _owmState;
    }
    
    // Add country code if provided
    if (!_owmCountryCode.isEmpty()) {
        url += "," + _owmCountryCode;
    }
    
    url += "&appid=" + _owmApiKey + "&units=" + units;
    
    Serial.println("[Weather] OWM - URL: " + url);
    
    http.begin(url);
    http.setTimeout(5000);
    Serial.println("[Weather] OWM - Sending HTTP GET request...");
    int httpCode = http.GET();
    Serial.printf("[Weather] OWM - HTTP response code: %d\n", httpCode);
    
    if (httpCode != 200) {
        _lastError = "HTTP error: " + String(httpCode);
        Serial.printf("[Weather] OWM - HTTP FAILED: %d\n", httpCode);
        http.end();
        return false;
    }
    
    String payload = http.getString();
    Serial.printf("[Weather] OWM - Received payload length: %d bytes\n", payload.length());
    http.end();
    
    // Parse JSON response
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        _lastError = "JSON parse error: " + String(error.c_str());
        Serial.printf("[Weather] OWM - JSON parse FAILED: %s\n", error.c_str());
        Serial.println("[Weather] OWM - Payload: " + payload);
        return false;
    }
    Serial.println("[Weather] OWM - JSON parsed successfully");
    
    // Extract weather data
    Serial.println("[Weather] OWM - Extracting weather data from JSON...");
    _data.temperature = doc["main"]["temp"];
    _data.tempHigh = doc["main"]["temp_max"];
    _data.tempLow = doc["main"]["temp_min"];
    _data.humidity = doc["main"]["humidity"];
    _data.windSpeed = doc["wind"]["speed"];
    _data.condition = doc["weather"][0]["main"].as<String>();
    _data.description = doc["weather"][0]["description"].as<String>();
    _data.iconCode = doc["weather"][0]["icon"].as<String>();
    
    _data.valid = true;
    _lastError = "";
    
    Serial.printf("[Weather] OWM - SUCCESS: Temp=%.1f%s, High=%.1f, Low=%.1f, Condition=%s, Humidity=%d%%\n", 
                  _data.temperature, 
                  _useFahrenheit ? "F" : "C",
                  _data.tempHigh,
                  _data.tempLow,
                  _data.condition.c_str(),
                  _data.humidity);
    
    return true;
}

bool Weather::updateFromHomeAssistant() {
    Serial.println("[Weather] updateFromHomeAssistant() - starting");
    
    if (_haUrl.isEmpty() || _haToken.isEmpty() || _haEntityId.isEmpty()) {
        _lastError = "Home Assistant not configured";
        Serial.printf("[Weather] HA - Config error: URL %s, Token %s, Entity %s\n",
                      _haUrl.isEmpty() ? "EMPTY" : "OK",
                      _haToken.isEmpty() ? "EMPTY" : "OK",
                      _haEntityId.isEmpty() ? "EMPTY" : "OK");
        return false;
    }
    
    HTTPClient http;
    String url = _haUrl + "/api/states/" + _haEntityId;
    
    Serial.println("[Weather] HA - URL: " + url);
    
    http.begin(url);
    http.setTimeout(5000);
    http.addHeader("Authorization", "Bearer " + _haToken);
    http.addHeader("Content-Type", "application/json");
    Serial.println("[Weather] HA - Headers set, sending HTTP GET request...");
    
    int httpCode = http.GET();
    Serial.printf("[Weather] HA - HTTP response code: %d\n", httpCode);
    
    if (httpCode != 200) {
        _lastError = "HTTP error: " + String(httpCode);
        Serial.printf("[Weather] HA - HTTP FAILED: %d\n", httpCode);
        http.end();
        return false;
    }
    
    String payload = http.getString();
    Serial.printf("[Weather] HA - Received payload length: %d bytes\n", payload.length());
    http.end();
    
    // Parse JSON response
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        _lastError = "JSON parse error: " + String(error.c_str());
        Serial.printf("[Weather] HA - JSON parse FAILED: %s\n", error.c_str());
        Serial.println("[Weather] HA - Payload: " + payload);
        return false;
    }
    Serial.println("[Weather] HA - JSON parsed successfully");
    
    // Extract weather data from Home Assistant entity
    Serial.println("[Weather] HA - Extracting weather data from JSON...");
    _data.temperature = doc["attributes"]["temperature"];
    _data.humidity = doc["attributes"]["humidity"];
    _data.condition = doc["state"].as<String>();
    
    // Optional attributes (may not be present)
    if (doc["attributes"].containsKey("forecast")) {
        Serial.println("[Weather] HA - Forecast data found");
        JsonArray forecast = doc["attributes"]["forecast"];
        if (forecast.size() > 0) {
            _data.tempHigh = forecast[0]["temperature"];
            _data.tempLow = forecast[0]["templow"];
        }
    } else {
        Serial.println("[Weather] HA - No forecast data available");
    }
    
    if (doc["attributes"].containsKey("wind_speed")) {
        _data.windSpeed = doc["attributes"]["wind_speed"];
    }
    
    _data.description = _data.condition;
    _data.valid = true;
    _lastError = "";
    
    Serial.printf("[Weather] HA - SUCCESS: Temp=%.1f%s, Condition=%s, Humidity=%d%%\n", 
                  _data.temperature, 
                  _useFahrenheit ? "F" : "C",
                  _data.condition.c_str(),
                  _data.humidity);
    
    return true;
}

WeatherData Weather::getData() {
    return _data;
}

bool Weather::isDataValid() {
    return _data.valid;
}

String Weather::getLastError() {
    return _lastError;
}

void Weather::drawWeatherIcon(TFT_eSPI &tft, int x, int y, String condition) {
    // Draw a 36x36 icon using standard OWM icon codes
    // Icon code is stored in _data.iconCode (e.g., "01d", "10n")
    // We use the numeric part: 01=clear, 02=few clouds, 03/04=clouds, 09=shower, 10=rain, 11=storm, 13=snow, 50=mist
    
    int cx = x + 18;
    int cy = y + 18;
    
    // Get icon code from weather data (e.g., "10d" -> 10)
    String iconStr = _data.iconCode;
    if (iconStr.length() < 2) iconStr = "03d"; // Default to clouds if no code
    int iconCode = iconStr.substring(0, 2).toInt();
    
    // Define colors for different weather conditions
    uint16_t sunColor = 0xFD20;      // Orange/yellow for sun
    uint16_t cloudColor = 0xCE79;    // Light gray for clouds
    uint16_t rainColor = 0x3D5F;     // Blue for rain
    uint16_t snowColor = 0xFFFF;     // White for snow
    uint16_t stormColor = 0xFC00;    // Yellow for lightning
    uint16_t mistColor = 0xAD55;     // Gray for mist
    
    auto drawSun = [&]() {
        // 01d - clear sky (orange/yellow)
        tft.fillCircle(cx, cy, 10, sunColor);
        for (int a = 0; a < 360; a += 45) {
            float rad = a * 0.0174533f;
            int x1 = cx + (int)(14 * cos(rad));
            int y1 = cy + (int)(14 * sin(rad));
            int x2 = cx + (int)(17 * cos(rad));
            int y2 = cy + (int)(17 * sin(rad));
            tft.drawLine(x1, y1, x2, y2, sunColor);
        }
    };
    
    auto drawFewClouds = [&]() {
        // 02d - few clouds (sun + small cloud)
        // Small sun in upper left (orange)
        int sx = cx - 6, sy = cy - 6;
        tft.fillCircle(sx, sy, 6, sunColor);
        for (int a = 0; a < 360; a += 90) {
            float rad = a * 0.0174533f;
            int x1 = sx + (int)(8 * cos(rad));
            int y1 = sy + (int)(8 * sin(rad));
            int x2 = sx + (int)(10 * cos(rad));
            int y2 = sy + (int)(10 * sin(rad));
            tft.drawLine(x1, y1, x2, y2, sunColor);
        }
        // Small cloud lower right (gray)
        tft.fillCircle(cx + 4, cy + 6, 5, cloudColor);
        tft.fillCircle(cx + 10, cy + 8, 4, cloudColor);
        tft.fillRect(cx + 2, cy + 8, 12, 6, cloudColor);
    };

    auto drawCloud = [&]() {
        // 03d/04d - scattered/broken clouds (gray)
        tft.fillCircle(cx - 9, cy + 4, 8, cloudColor);
        tft.fillCircle(cx,     cy,     10, cloudColor);
        tft.fillCircle(cx + 10, cy + 6, 7, cloudColor);
        tft.fillRect(cx - 14, cy + 6, 28, 10, cloudColor);
    };

    auto drawShowerRain = [&]() {
        // 09d - shower rain (cloud with short rain - blue)
        drawCloud();
        // Short rain lines in blue
        tft.drawLine(cx - 8, cy + 18, cx - 7, cy + 22, rainColor);
        tft.drawLine(cx - 2, cy + 18, cx - 1, cy + 22, rainColor);
        tft.drawLine(cx + 4, cy + 18, cx + 5, cy + 22, rainColor);
        tft.drawLine(cx + 10, cy + 18, cx + 11, cy + 22, rainColor);
    };

    auto drawRain = [&]() {
        // 10d - rain (cloud with long rain - blue)
        drawCloud();
        // Longer raindrops in blue
        tft.drawLine(cx - 8, cy + 18, cx - 6, cy + 26, rainColor);
        tft.drawLine(cx,     cy + 18, cx + 2, cy + 26, rainColor);
        tft.drawLine(cx + 8, cy + 18, cx + 10, cy + 26, rainColor);
    };

    auto drawStorm = [&]() {
        // 11d - thunderstorm (cloud + yellow lightning)
        drawCloud();
        // Lightning bolt in yellow
        tft.drawLine(cx, cy + 8, cx - 5, cy + 18, stormColor);
        tft.drawLine(cx - 5, cy + 18, cx + 2, cy + 18, stormColor);
        tft.drawLine(cx + 2, cy + 18, cx - 3, cy + 28, stormColor);
    };

    auto drawSnow = [&]() {
        // 13d - snow (cloud + white snowflakes)
        drawCloud();
        // Snowflakes in white
        auto flake = [&](int fx, int fy) {
            tft.drawPixel(fx, fy, snowColor);
            tft.drawLine(fx - 2, fy, fx + 2, fy, snowColor);
            tft.drawLine(fx, fy - 2, fx, fy + 2, snowColor);
            tft.drawLine(fx - 1, fy - 1, fx + 1, fy + 1, snowColor);
            tft.drawLine(fx - 1, fy + 1, fx + 1, fy - 1, snowColor);
        };
        flake(cx - 9, cy + 22);
        flake(cx,     cy + 24);
        flake(cx + 9, cy + 22);
    };

    auto drawMist = [&]() {
        // 50d - mist/fog (gray horizontal lines)
        tft.drawLine(cx - 16, cy + 8, cx + 16, cy + 8, mistColor);
        tft.drawLine(cx - 16, cy + 14, cx + 16, cy + 14, mistColor);
        tft.drawLine(cx - 16, cy + 20, cx + 16, cy + 20, mistColor);
    };
    
    // Draw icon based on standard OWM icon code
    switch(iconCode) {
        case 1:  // 01d/01n - clear sky
            drawSun();
            break;
        case 2:  // 02d/02n - few clouds
            drawFewClouds();
            break;
        case 3:  // 03d/03n - scattered clouds
        case 4:  // 04d/04n - broken/overcast clouds
            drawCloud();
            break;
        case 9:  // 09d/09n - shower rain
            drawShowerRain();
            break;
        case 10: // 10d/10n - rain
            drawRain();
            break;
        case 11: // 11d/11n - thunderstorm
            drawStorm();
            break;
        case 13: // 13d/13n - snow
            drawSnow();
            break;
        case 50: // 50d/50n - mist/fog
            drawMist();
            break;
        default: // Unknown - default to clouds
            drawCloud();
            break;
    }
}

void Weather::displayOnTFT(TFT_eSPI &tft, int x, int y, bool useFahrenheit) {
    Serial.printf("[Weather] displayOnTFT() - called at position (%d, %d), data valid: %d\n", x, y, _data.valid);
    
    if (!_data.valid) {
        Serial.println("[Weather] displayOnTFT() - data not valid, skipping display");
        return;
    }
    
    // Draw only when content changes to avoid flicker
    static unsigned long prevLastUpdate = 0;
    static bool prevUnitsF = true;
    static int prevX = -1, prevY = -1;
    bool needsRedraw = false;
    if (_data.lastUpdate != prevLastUpdate || prevUnitsF != useFahrenheit || prevX != x || prevY != y) {
        needsRedraw = true;
    }
    if (!needsRedraw) {
        // No changes; skip drawing to avoid flicker
        return;
    }
    prevLastUpdate = _data.lastUpdate;
    prevUnitsF = useFahrenheit;
    prevX = x; prevY = y;

    Serial.printf("[Weather] displayOnTFT() - Redraw: Temp=%.1f%s, Cond=%s\n",
                  _data.temperature,
                  useFahrenheit ? "F" : "C",
                  _data.condition.c_str());
    
    // Clear the display area (wider to fit temp + icon + hi/lo)
    tft.fillRect(x, y, 160, 40, COLOR_BACKGROUND);
    
    // Draw temperature first (moved right 5px, down 5px)
    tft.setTextSize(2);
    tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    tft.setCursor(x + 10, y + 10);
    char tempStr[8];
    dtostrf(_data.temperature, 4, 1, tempStr);
    tft.print(tempStr);
    tft.print(useFahrenheit ? "F" : "C");

    // Draw weather icon after the temperature (icon position unchanged)
    // Place icon to the right of temp area with some padding
    drawWeatherIcon(tft, x + 110, y, _data.condition);
    
    // Ensure text color is set back to white for any text after icon
    tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);

    // Draw high/low if available (moved right 5px, down 5px)
    // Check if values are not the sentinel value (-999) instead of checking > 0
    if (_data.tempHigh > -900 || _data.tempLow > -900) {
        tft.setTextSize(1);
        tft.setCursor(x + 10, y + 30);
        char hiLoStr[16];
        snprintf(hiLoStr, sizeof(hiLoStr), "H:%.0f L:%.0f", _data.tempHigh, _data.tempLow);
        tft.print(hiLoStr);
    }
}

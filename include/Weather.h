/*
 * Weather.h - Weather integration module for Smart Thermostat
 * 
 * Supports two weather sources:
 * 1. OpenWeatherMap API
 * 2. Home Assistant weather entity
 * 
 * Only one source can be active at a time.
 */

#ifndef WEATHER_H
#define WEATHER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>

// Weather source types
enum WeatherSource {
    WEATHER_DISABLED = 0,
    WEATHER_OPENWEATHERMAP = 1,
    WEATHER_HOMEASSISTANT = 2
};

// Weather data structure
struct WeatherData {
    float temperature;
    float tempHigh;
    float tempLow;
    String condition;        // e.g., "Clear", "Cloudy", "Rain"
    String description;      // e.g., "clear sky", "light rain"
    int humidity;
    float windSpeed;
    String iconCode;         // OpenWeatherMap icon code (e.g., "01d", "10n")
    bool valid;              // True if data is valid
    unsigned long lastUpdate; // Timestamp of last successful update
};

class Weather {
public:
    Weather();
    
    // Initialization
    void begin();
    
    // Configuration
    void setSource(WeatherSource source);
    void setOpenWeatherMapConfig(String apiKey, String city, String state, String countryCode);
    void setHomeAssistantConfig(String haUrl, String haToken, String entityId);
    void setUpdateInterval(unsigned long intervalMs);
    void setUseFahrenheit(bool useFahrenheit);
    
    // Update weather data
    bool update();
    void forceUpdate();
    
    // Get weather data
    WeatherData getData();
    bool isDataValid();
    String getLastError();
    
    // Display on TFT
    void displayOnTFT(TFT_eSPI &tft, int x, int y, bool useFahrenheit);
    
private:
    // Configuration
    WeatherSource _source;
    String _owmApiKey;
    String _owmCity;
    String _owmState;
    String _owmCountryCode;
    String _haUrl;
    String _haToken;
    String _haEntityId;
    unsigned long _updateInterval;
    bool _useFahrenheit;
    
    // Data
    WeatherData _data;
    String _lastError;
    unsigned long _lastUpdateAttempt;
    bool _forceNextUpdate;
    
    // Private methods
    bool updateFromOpenWeatherMap();
    bool updateFromHomeAssistant();
    void drawWeatherIcon(TFT_eSPI &tft, int x, int y, String condition);
    String getIconFromCondition(String condition);
};

#endif // WEATHER_H

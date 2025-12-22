/*
 * SettingsUI - On-screen settings menu for ESP32-S3 Simple Thermostat
 * Copyright (c) 2025 Jonn Taylor
 * 
 * Provides TFT-based settings menu for:
 * - WiFi configuration
 * - Comfort settings (temp swing, auto swing, fan relay, units)
 * - HVAC advanced (stage2 enable, runtime, delta)
 * - Hostname
 * 
 * Designed to minimize changes to Main-Thermostat.cpp; all UI logic isolated here.
 */

#ifndef SETTINGS_UI_H
#define SETTINGS_UI_H

#include <TFT_eSPI.h>
#include <Preferences.h>

// Keyboard mode for reusing on-screen keyboard
enum KeyboardMode { KB_WIFI_SSID, KB_WIFI_PASS, KB_HOSTNAME };

// External references to main code objects (defined in Main-Thermostat.cpp)
extern TFT_eSPI tft;
extern Preferences preferences;

// External references to global settings (updated via UI)
extern float tempSwing;
extern float autoTempSwing;
extern bool fanRelayNeeded;
extern bool useFahrenheit;
extern unsigned long stage1MinRuntime;
extern float stage2TempDelta;
extern bool stage2HeatingEnabled;
extern bool stage2CoolingEnabled;
extern String hostname;
extern float currentTemp;
extern float currentHumidity;
extern bool forceFullDisplayRefresh;
extern bool inSettingsMenu;
extern bool inWiFiSetupMode;
extern bool keyboardReturnToSettings;
extern String inputText;
extern bool isUpperCaseKeyboard;
extern bool isEnteringSSID;

// External references to keyboard control (defined in Main)
extern KeyboardMode keyboardMode;

// External functions from Main
extern void saveSettings();
extern void updateDisplay(float temp, float hum);
extern void drawButtons();
extern void drawKeyboard(bool isUpperCase);
extern void setDisplayUpdateFlag();
extern void buzzerBeep(int duration);

// Color scheme (defined in main, redeclare for clarity)
#define COLOR_BACKGROUND   0x1082
#define COLOR_PRIMARY      0x1976
#define COLOR_SECONDARY    0x0497
#define COLOR_ACCENT       0xFFC1
#define COLOR_TEXT         0xFFFF
#define COLOR_TEXT_LIGHT   0xE0E0
#define COLOR_SUCCESS      0x4CAF
#define COLOR_WARNING      0xFF70
#define COLOR_SURFACE      0x2124

// Settings UI state
enum SettingsPage {
    PAGE_MENU,       // Main menu: WiFi, Comfort, HVAC Advanced, Hostname, Back
    PAGE_COMFORT,    // Temp swing, auto swing, fan relay, use F
    PAGE_HVAC_ADV,   // stage2 enable (heat/cool), stage1 min runtime, stage2 delta
    PAGE_HOSTNAME    // Hostname entry (uses keyboard)
};

// Touch button structure
struct TouchButton {
    int x, y, w, h;
    const char* label;
    uint16_t color;
};

// Current settings page
static SettingsPage currentPage = PAGE_MENU;

// Temporary edit buffer for numeric values
static float editTempSwing = 1.0;
static float editAutoTempSwing = 3.0;
static bool editFanRelayNeeded = false;
static bool editUseFahrenheit = true;
static unsigned long editStage1MinRuntime = 300;
static float editStage2TempDelta = 2.0;
static bool editStage2HeatingEnabled = false;
static bool editStage2CoolingEnabled = false;

// Forward declarations
void drawSettingsMenu();
void drawComfortSettings();
void drawHVACAdvancedSettings();
bool settingsHandleTouch(uint16_t x, uint16_t y);
void enterSettingsMenu();
void exitSettingsToMain();
void startWiFiSetupUI(bool returnToSettings);
void startHostnameEntry();
void exitKeyboardToPreviousScreen();
void settingsLoopTick();

// Helper: draw a labeled button
void drawSettingsButton(int x, int y, int w, int h, const char* label, uint16_t color) {
    tft.fillRect(x, y, w, h, color);
    tft.drawRect(x, y, w, h, COLOR_TEXT);
    tft.setTextColor(TFT_BLACK, color);
    tft.setTextSize(2);
    int textLen = strlen(label);
    int textWidth = textLen * 12; // approx for size 2
    int textX = x + (w - textWidth) / 2;
    int textY = y + (h - 16) / 2;
    tft.setCursor(textX, textY);
    tft.print(label);
}

// Helper: draw small toggle indicator
void drawToggle(int x, int y, bool state) {
    uint16_t toggleColor = state ? COLOR_SUCCESS : COLOR_WARNING;
    tft.fillCircle(x, y, 10, toggleColor);
    tft.drawCircle(x, y, 10, COLOR_TEXT);
    tft.setTextColor(TFT_BLACK, toggleColor);
    tft.setTextSize(1);
    tft.setCursor(x - 6, y - 4);
    tft.print(state ? "ON" : "OFF");
}

// Helper: draw numeric value with +/- buttons
void drawNumericControl(int x, int y, const char* label, float value, int fieldId) {
    tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    tft.setTextSize(2);
    tft.setCursor(x, y);
    tft.print(label);
    
    // - button
    int btnY = y + 25;
    tft.fillRect(x, btnY, 30, 30, COLOR_WARNING);
    tft.drawRect(x, btnY, 30, 30, COLOR_TEXT);
    tft.setTextColor(TFT_BLACK, COLOR_WARNING);
    tft.setCursor(x + 10, btnY + 8);
    tft.print("-");
    
    // Value display
    tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    tft.setCursor(x + 35, btnY + 6);
    char buf[16];
    if (fieldId == 1 || fieldId == 2 || fieldId == 4) { // float fields
        snprintf(buf, sizeof(buf), "%.1f", value);
    } else { // unsigned long (stage1 runtime in seconds)
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)value);
    }
    tft.print(buf);
    
    // + button
    tft.fillRect(x + 100, btnY, 30, 30, COLOR_SUCCESS);
    tft.drawRect(x + 100, btnY, 30, 30, COLOR_TEXT);
    tft.setTextColor(TFT_BLACK, COLOR_SUCCESS);
    tft.setCursor(x + 110, btnY + 8);
    tft.print("+");
}

// Enter settings menu from main screen
void enterSettingsMenu() {
    inSettingsMenu = true;
    currentPage = PAGE_MENU;
    
    // Load current values into edit buffers
    editTempSwing = tempSwing;
    editAutoTempSwing = autoTempSwing;
    editFanRelayNeeded = fanRelayNeeded;
    editUseFahrenheit = useFahrenheit;
    editStage1MinRuntime = stage1MinRuntime;
    editStage2TempDelta = stage2TempDelta;
    editStage2HeatingEnabled = stage2HeatingEnabled;
    editStage2CoolingEnabled = stage2CoolingEnabled;
    
    drawSettingsMenu();
}

// Exit settings menu back to main display
void exitSettingsToMain() {
    inSettingsMenu = false;
    forceFullDisplayRefresh = true; // Ensure main UI fully redraws after exiting settings
    tft.fillScreen(COLOR_BACKGROUND);
    updateDisplay(currentTemp, currentHumidity);
    drawButtons();
}

// Draw main settings menu
void drawSettingsMenu() {
    tft.fillScreen(COLOR_BACKGROUND);
    tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.print("Settings Menu");
    
    // Menu buttons (stacked vertically)
    int btnX = 20, btnY = 50, btnW = 280, btnH = 35, btnSpacing = 5;
    
    drawSettingsButton(btnX, btnY, btnW, btnH, "WiFi", COLOR_PRIMARY);
    btnY += btnH + btnSpacing;
    
    drawSettingsButton(btnX, btnY, btnW, btnH, "Comfort", COLOR_SECONDARY);
    btnY += btnH + btnSpacing;
    
    drawSettingsButton(btnX, btnY, btnW, btnH, "HVAC Advanced", COLOR_ACCENT);
    btnY += btnH + btnSpacing;
    
    drawSettingsButton(btnX, btnY, btnW, btnH, "Hostname", COLOR_PRIMARY);
    btnY += btnH + btnSpacing;
    
    drawSettingsButton(btnX, btnY, btnW, btnH, "Back to Main", COLOR_WARNING);
}

// Draw comfort settings page
void drawComfortSettings() {
    tft.fillScreen(COLOR_BACKGROUND);
    tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.print("Comfort Settings");
    
    int yPos = 40;
    
    // Temperature Swing
    drawNumericControl(20, yPos, "Temp Swing:", editTempSwing, 1);
    yPos += 65;
    
    // Auto Temp Swing
    drawNumericControl(20, yPos, "Auto Swing:", editAutoTempSwing, 2);
    yPos += 65;
    
    // Fan Relay Required toggle (compact layout)
    tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    tft.setTextSize(1);
    tft.setCursor(20, yPos);
    tft.print("Fan Relay Required:");
    drawToggle(220, yPos + 5, editFanRelayNeeded);
    yPos += 20;
    
    // Use Fahrenheit toggle
    tft.setCursor(20, yPos);
    tft.print("Use Fahrenheit:");
    drawToggle(220, yPos + 5, editUseFahrenheit);
    
    // Save and Back buttons
    drawSettingsButton(20, 200, 120, 35, "Save", COLOR_SUCCESS);
    drawSettingsButton(180, 200, 120, 35, "Back", COLOR_WARNING);
}

// Draw HVAC Advanced settings page
void drawHVACAdvancedSettings() {
    tft.fillScreen(COLOR_BACKGROUND);
    tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.print("HVAC Advanced");
    
    int yPos = 40;
    
    // Stage1 Min Runtime (seconds)
    drawNumericControl(20, yPos, "Stage1 Min (s):", (float)editStage1MinRuntime, 3);
    yPos += 65;
    
    // Stage2 Temp Delta
    drawNumericControl(20, yPos, "Stage2 Delta:", editStage2TempDelta, 4);
    yPos += 65;
    
    // Stage2 Heat Enable toggle
    tft.setTextColor(COLOR_TEXT, COLOR_BACKGROUND);
    tft.setTextSize(1);
    tft.setCursor(20, yPos);
    tft.print("Stage2 Heat Enable:");
    drawToggle(220, yPos + 5, editStage2HeatingEnabled);
    yPos += 20;
    
    // Stage2 Cool Enable toggle
    tft.setCursor(20, yPos);
    tft.print("Stage2 Cool Enable:");
    drawToggle(220, yPos + 5, editStage2CoolingEnabled);
    
    // Save and Back buttons
    drawSettingsButton(20, 200, 120, 35, "Save", COLOR_SUCCESS);
    drawSettingsButton(180, 200, 120, 35, "Back", COLOR_WARNING);
}

// Launch WiFi setup (reuse existing keyboard flow)
void startWiFiSetupUI(bool returnToSettings) {
    keyboardReturnToSettings = returnToSettings;
    inWiFiSetupMode = true;
    inputText = "";
    isEnteringSSID = true;
    keyboardMode = (KeyboardMode)0; // KB_WIFI_SSID
    
    tft.fillScreen(COLOR_BACKGROUND);
    drawKeyboard(isUpperCaseKeyboard);
}

// Launch hostname entry (keyboard mode)
void startHostnameEntry() {
    inWiFiSetupMode = true;
    inputText = hostname; // pre-fill current hostname
    keyboardMode = (KeyboardMode)2; // KB_HOSTNAME
    keyboardReturnToSettings = true;
    
    tft.fillScreen(COLOR_BACKGROUND);
    drawKeyboard(isUpperCaseKeyboard);
}

// Exit keyboard back to previous screen (main or settings)
void exitKeyboardToPreviousScreen() {
    inWiFiSetupMode = false;
    
    if (keyboardReturnToSettings) {
        // Return to settings menu
        inSettingsMenu = true;
        drawSettingsMenu();
        keyboardReturnToSettings = false;
    } else {
        // Return to main display
        exitSettingsToMain();
    }
}

// Handle touch events when settings UI is active
bool settingsHandleTouch(uint16_t x, uint16_t y) {
    buzzerBeep(50);
    
    // Main settings menu
    if (currentPage == PAGE_MENU) {
        int btnX = 20, btnY = 50, btnW = 280, btnH = 35, btnSpacing = 5;
        
        // WiFi button
        if (x >= btnX && x <= btnX + btnW && y >= btnY && y <= btnY + btnH) {
            startWiFiSetupUI(true);
            return true;
        }
        btnY += btnH + btnSpacing;
        
        // Comfort button
        if (x >= btnX && x <= btnX + btnW && y >= btnY && y <= btnY + btnH) {
            currentPage = PAGE_COMFORT;
            drawComfortSettings();
            return true;
        }
        btnY += btnH + btnSpacing;
        
        // HVAC Advanced button
        if (x >= btnX && x <= btnX + btnW && y >= btnY && y <= btnY + btnH) {
            currentPage = PAGE_HVAC_ADV;
            drawHVACAdvancedSettings();
            return true;
        }
        btnY += btnH + btnSpacing;
        
        // Hostname button
        if (x >= btnX && x <= btnX + btnW && y >= btnY && y <= btnY + btnH) {
            startHostnameEntry();
            return true;
        }
        btnY += btnH + btnSpacing;
        
        // Back to Main button
        if (x >= btnX && x <= btnX + btnW && y >= btnY && y <= btnY + btnH) {
            exitSettingsToMain();
            return true;
        }
    }
    
    // Comfort settings page
    else if (currentPage == PAGE_COMFORT) {
        int yPos = 40;
        
        // Temp Swing +/-
        if (y >= yPos + 25 && y <= yPos + 55) {
            if (x >= 20 && x <= 50) { // - button
                editTempSwing -= 0.1;
                if (editTempSwing < 0.2) editTempSwing = 0.2;
                drawComfortSettings();
                return true;
            } else if (x >= 120 && x <= 150) { // + button
                editTempSwing += 0.1;
                if (editTempSwing > 3.0) editTempSwing = 3.0;
                drawComfortSettings();
                return true;
            }
        }
        yPos += 65;
        
        // Auto Temp Swing +/-
        if (y >= yPos + 25 && y <= yPos + 55) {
            if (x >= 20 && x <= 50) { // - button
                editAutoTempSwing -= 0.1;
                if (editAutoTempSwing < 0.2) editAutoTempSwing = 0.2;
                drawComfortSettings();
                return true;
            } else if (x >= 120 && x <= 150) { // + button
                editAutoTempSwing += 0.1;
                if (editAutoTempSwing > 5.0) editAutoTempSwing = 5.0;
                drawComfortSettings();
                return true;
            }
        }
        yPos += 65;
        
        // Fan Relay Required toggle
        if (x >= 200 && x <= 240 && y >= yPos && y <= yPos + 20) {
            editFanRelayNeeded = !editFanRelayNeeded;
            drawComfortSettings();
            return true;
        }
        yPos += 20;
        
        // Use Fahrenheit toggle
        if (x >= 200 && x <= 240 && y >= yPos && y <= yPos + 20) {
            editUseFahrenheit = !editUseFahrenheit;
            drawComfortSettings();
            return true;
        }
        
        // Save button
        if (x >= 20 && x <= 140 && y >= 200 && y <= 235) {
            tempSwing = editTempSwing;
            autoTempSwing = editAutoTempSwing;
            fanRelayNeeded = editFanRelayNeeded;
            useFahrenheit = editUseFahrenheit;
            saveSettings();
            setDisplayUpdateFlag();
            currentPage = PAGE_MENU;
            drawSettingsMenu();
            return true;
        }
        
        // Back button
        if (x >= 180 && x <= 300 && y >= 200 && y <= 235) {
            // Discard changes
            currentPage = PAGE_MENU;
            drawSettingsMenu();
            return true;
        }
    }
    
    // HVAC Advanced settings page
    else if (currentPage == PAGE_HVAC_ADV) {
        int yPos = 40;
        
        // Stage1 Min Runtime +/-
        if (y >= yPos + 25 && y <= yPos + 55) {
            if (x >= 20 && x <= 50) { // - button
                if (editStage1MinRuntime > 60) editStage1MinRuntime -= 30;
                drawHVACAdvancedSettings();
                return true;
            } else if (x >= 120 && x <= 150) { // + button
                if (editStage1MinRuntime < 1800) editStage1MinRuntime += 30;
                drawHVACAdvancedSettings();
                return true;
            }
        }
        yPos += 65;
        
        // Stage2 Temp Delta +/-
        if (y >= yPos + 25 && y <= yPos + 55) {
            if (x >= 20 && x <= 50) { // - button
                editStage2TempDelta -= 0.5;
                if (editStage2TempDelta < 0.5) editStage2TempDelta = 0.5;
                drawHVACAdvancedSettings();
                return true;
            } else if (x >= 120 && x <= 150) { // + button
                editStage2TempDelta += 0.5;
                if (editStage2TempDelta > 5.0) editStage2TempDelta = 5.0;
                drawHVACAdvancedSettings();
                return true;
            }
        }
        yPos += 65;
        
        // Stage2 Heat Enable toggle
        if (x >= 200 && x <= 240 && y >= yPos && y <= yPos + 20) {
            editStage2HeatingEnabled = !editStage2HeatingEnabled;
            drawHVACAdvancedSettings();
            return true;
        }
        yPos += 20;
        
        // Stage2 Cool Enable toggle
        if (x >= 200 && x <= 240 && y >= yPos && y <= yPos + 20) {
            editStage2CoolingEnabled = !editStage2CoolingEnabled;
            drawHVACAdvancedSettings();
            return true;
        }
        
        // Save button
        if (x >= 20 && x <= 140 && y >= 200 && y <= 235) {
            stage1MinRuntime = editStage1MinRuntime;
            stage2TempDelta = editStage2TempDelta;
            stage2HeatingEnabled = editStage2HeatingEnabled;
            stage2CoolingEnabled = editStage2CoolingEnabled;
            saveSettings();
            setDisplayUpdateFlag();
            currentPage = PAGE_MENU;
            drawSettingsMenu();
            return true;
        }
        
        // Back button
        if (x >= 180 && x <= 300 && y >= 200 && y <= 235) {
            // Discard changes
            currentPage = PAGE_MENU;
            drawSettingsMenu();
            return true;
        }
    }
    
    return false;
}

// Loop tick when settings UI is active (currently no background work needed)
void settingsLoopTick() {
    // Placeholder for any periodic updates while in settings
    // For now, settings is purely event-driven (touch-based)
}

#endif // SETTINGS_UI_H

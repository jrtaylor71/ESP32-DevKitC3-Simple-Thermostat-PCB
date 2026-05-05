#include <Arduino.h>
#include "TFT_Setup_ESP32_S3_Thermostat.h"

DisplayVariant activeDisplay = DISPLAY_ILI9341;
static LGFX tft;

const char* displayName() {
    switch (activeDisplay) {
        case DISPLAY_ST7789:  return "ST7789 2.8in";
        case DISPLAY_ST7796:  return "ST7796S 4.0in";
        case DISPLAY_ILI9488: return "ILI9488 4.0in";
        case DISPLAY_ILI9341:
        default: return "ILI9341 3.2in";
    }
}

void drawPattern(uint16_t c1, uint16_t c2, uint16_t c3) {
    const int w = tft.width();
    const int h = tft.height();

    tft.fillScreen(TFT_BLACK);
    tft.fillRect(0, 0, w / 3, h, c1);
    tft.fillRect(w / 3, 0, w / 3, h, c2);
    tft.fillRect((w / 3) * 2, 0, w - ((w / 3) * 2), h, c3);

    tft.drawRect(0, 0, w, h, TFT_WHITE);
    tft.drawLine(0, 0, w - 1, h - 1, TFT_WHITE);
    tft.drawLine(w - 1, 0, 0, h - 1, TFT_WHITE);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);

    char info[96];
    snprintf(info, sizeof(info), "%s  %dx%d", displayName(), dispW(), dispH());
    tft.drawString("DISPLAY TEST", 8, 8);
    tft.drawString(info, 8, 34);
    tft.drawString("2.8/3.2/4.0 module check", 8, 60);
}

void setup() {
    Serial.begin(115200);
    delay(300);

    tft.initDisplay();
    Serial.printf("[DISPLAY-TEST] display=%s type=%d size=%dx%d actual=%dx%d\n",
                  displayName(),
                  (int)activeDisplay,
                  dispW(), dispH(),
                  tft.width(), tft.height());

    drawPattern(TFT_RED, TFT_GREEN, TFT_BLUE);
}

void loop() {
    static unsigned long lastSwap = 0;
    static uint8_t phase = 0;

    unsigned long now = millis();
    if (now - lastSwap > 2000) {
        lastSwap = now;
        phase = (phase + 1) % 3;
        if (phase == 0) drawPattern(TFT_RED, TFT_GREEN, TFT_BLUE);
        if (phase == 1) drawPattern(TFT_BLUE, TFT_RED, TFT_GREEN);
        if (phase == 2) drawPattern(TFT_GREEN, TFT_BLUE, TFT_RED);
    }

    lgfx::touch_point_t tp;
    if (tft.getTouch(&tp)) {
        tft.fillCircle(tp.x, tp.y, 5, TFT_YELLOW);
        Serial.printf("[DISPLAY-TEST] touch x=%d y=%d\n", tp.x, tp.y);
        delay(50);
    }
}

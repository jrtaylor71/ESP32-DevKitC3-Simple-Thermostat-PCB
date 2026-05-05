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

void drawFrame() {
    const int w = tft.width();
    const int h = tft.height();

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(2);

    tft.drawString("TL", 4, 4);
    tft.drawString("TR", w - 30, 4);
    tft.drawString("BL", 4, h - 20);
    tft.drawString("BR", w - 30, h - 20);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCentreString("Touch Test", w / 2, h / 2 - 20, 2);
    tft.drawCentreString(displayName(), w / 2, h / 2 + 6, 2);
}

void setup() {
    Serial.begin(115200);
    delay(300);

    tft.initDisplay();
    drawFrame();

    uint16_t cal[8];
    tft.calibrateTouch(cal, TFT_WHITE, TFT_BLACK, 15);
    tft.setTouchCalibrate(cal);

    Serial.printf("[TOUCH-TEST] cal: %u %u %u %u %u %u %u %u\n",
                  cal[0], cal[1], cal[2], cal[3],
                  cal[4], cal[5], cal[6], cal[7]);
    Serial.printf("[TOUCH-TEST] display=%s type=%d screen=%dx%d expected=%dx%d\n",
                  displayName(),
                  (int)activeDisplay,
                  tft.width(), tft.height(),
                  dispW(), dispH());

    drawFrame();
}

void loop() {
    lgfx::touch_point_t tp;
    if (tft.getTouch(&tp)) {
        uint16_t rx = 0;
        uint16_t ry = 0;
        tft.getTouchRaw(&rx, &ry);

        drawFrame();

        tft.drawLine(tp.x - 12, tp.y, tp.x + 12, tp.y, TFT_RED);
        tft.drawLine(tp.x, tp.y - 12, tp.x, tp.y + 12, TFT_RED);
        tft.fillCircle(tp.x, tp.y, 4, TFT_RED);

        char line[80];
        snprintf(line, sizeof(line), "Mapped X=%d Y=%d", tp.x, tp.y);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawCentreString(line, tft.width() / 2, tft.height() / 2 + 34, 2);

        snprintf(line, sizeof(line), "Raw X=%u Y=%u", rx, ry);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawCentreString(line, tft.width() / 2, tft.height() / 2 + 56, 2);

        Serial.printf("[TOUCH-TEST] mapped x=%d y=%d | raw x=%u y=%u\n", tp.x, tp.y, rx, ry);
        delay(80);
    }
}

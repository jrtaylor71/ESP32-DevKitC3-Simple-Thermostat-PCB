#pragma once

// LovyanGFX display/touch configuration for ESP32-S3 Thermostat PCB
// Supports three display variants detected at runtime by SPI ID probe:
//   ILI9341 — 3.2" 320x240 (default/fallback)
//   ST7789  — 2.8" 320x240
//   ST7796  — 4.0" 480x320
// Touch controller: XPT2046 resistive, shared SPI bus, polling only (no IRQ)
// All pin assignments come from HardwarePins.h
// Rotation 3 = landscape with USB port on left (hardware-confirmed orientation)

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <SPI.h>
#include <Preferences.h>
#include "HardwarePins.h"

enum DisplayVariant {
	DISPLAY_ILI9341 = 0,
	DISPLAY_ST7789  = 1,
	DISPLAY_ST7796  = 2,
	DISPLAY_ILI9488 = 3,
};

extern DisplayVariant activeDisplay;

// Persist last known-good display type across ESP.restart() soft resets.
// This prevents orientation/layout flips when SPI ID probe temporarily returns all-FF.
static RTC_DATA_ATTR int g_lgfx_last_good_display = -1;
static constexpr const char* LGFX_PREFS_NAMESPACE = "thermostat";
static constexpr const char* LGFX_PREFS_KEY_DISPLAY = "dispVariant";

// dispW/dispH return logical width/height after rotation is applied
// ST7796 and ILI9488 are 480x320; all others are 320x240
inline int dispW() { return (activeDisplay == DISPLAY_ST7796 || activeDisplay == DISPLAY_ILI9488) ? 480 : 320; }
inline int dispH() { return (activeDisplay == DISPLAY_ST7796 || activeDisplay == DISPLAY_ILI9488) ? 320 : 240; }

static constexpr int LGFX_PIN_MOSI   = TFT_MOSI_PIN;
static constexpr int LGFX_PIN_MISO   = TFT_MISO_PIN;
static constexpr int LGFX_PIN_SCLK   = TFT_SCLK_PIN;
static constexpr int LGFX_PIN_DC     = TFT_DC_PIN;
static constexpr int LGFX_PIN_RST    = TFT_RST_PIN;
static constexpr int LGFX_PIN_CS     = TFT_CS_PIN;
static constexpr int LGFX_PIN_BL     = TFT_BACKLIGHT_PIN;
static constexpr int LGFX_PIN_TCH_CS = TOUCH_CS_PIN;

static constexpr uint8_t LGFX_DEFAULT_ROTATION = 3;  // Landscape, USB port on left (hardware-confirmed)

class LGFX : public lgfx::LGFX_Device {
	lgfx::Panel_ST7796  _panel_st7796;
	lgfx::Panel_ILI9341 _panel_ili9341;
	lgfx::Panel_ILI9488 _panel_ili9488;
	lgfx::Panel_ST7789  _panel_st7789;
	lgfx::Bus_SPI       _bus;
	lgfx::Light_PWM     _light;
	lgfx::Touch_XPT2046 _touch;
	uint8_t _lastRddid[3] = {0, 0, 0};

public:
	LGFX() {}

	bool initDisplay() {
		{
			auto cfg = _bus.config();
			cfg.spi_host   = SPI2_HOST;   // FSPI on ESP32-S3
			cfg.spi_mode   = 0;
			cfg.freq_write = 40000000;    // 40MHz display write
			cfg.freq_read  =  6000000;    // 6MHz for display ID probe reads
			cfg.pin_sclk   = LGFX_PIN_SCLK;
			cfg.pin_mosi   = LGFX_PIN_MOSI;
			cfg.pin_miso   = LGFX_PIN_MISO;
			cfg.pin_dc     = LGFX_PIN_DC;
			_bus.config(cfg);
		}

		{
			auto cfg = _light.config();
			cfg.pin_bl      = LGFX_PIN_BL;
			cfg.invert      = false;
			cfg.freq        = 44100;
			cfg.pwm_channel = 7;
			_light.config(cfg);
		}

		{
			auto cfg = _touch.config();
			auto buscfg = _bus.config();
			cfg.spi_host   = buscfg.spi_host;
			cfg.pin_sclk   = buscfg.pin_sclk;
			cfg.pin_mosi   = buscfg.pin_mosi;
			cfg.pin_miso   = buscfg.pin_miso;
			// x_min/x_max/y_min/y_max: raw ADC values measured from live hardware calibration
			// Axes are inverted (min > max) to match physical orientation at rotation=3
			cfg.x_min      = 3621;
			cfg.x_max      =  288;
			cfg.y_min      = 3501;
			cfg.y_max      =  292;
			cfg.pin_int    = -1;          // IRQ pin not used — polling mode only
			cfg.pin_cs     = LGFX_PIN_TCH_CS;
			cfg.bus_shared = true;        // Touch shares SPI bus with display
			cfg.offset_rotation = 0;
			cfg.freq       = 250000;      // 250kHz — XPT2046 max reliable rate on shared bus
			_touch.config(cfg);
		}

		uint16_t id = _probeDisplayId();
		bool invalidProbeAllFF = (id == 0xFFFF && _lastRddid[0] == 0xFF && _lastRddid[1] == 0xFF && _lastRddid[2] == 0xFF);
		bool ambiguousProbeAllZero = (id == 0x0000 && _lastRddid[0] == 0x00 && _lastRddid[1] == 0x00 && _lastRddid[2] == 0x00);
		bool probeAmbiguous = invalidProbeAllFF || ambiguousProbeAllZero;
		int persistedDisplay = -1;

		// On software reboot, SPI/display can briefly return floating all-FF values.
		// Retry probe with extra settle time before trusting the result.
		if (invalidProbeAllFF) {
			Serial.println("[LGFX] Probe returned all-FF; retrying ID probe for bus/display settle...");
			for (int retry = 0; retry < 2; ++retry) {
				delay(30 + (retry * 30));
				uint16_t retryId = _probeDisplayId();
				bool retryAllFF = (retryId == 0xFFFF && _lastRddid[0] == 0xFF && _lastRddid[1] == 0xFF && _lastRddid[2] == 0xFF);
				if (!retryAllFF) {
					id = retryId;
					invalidProbeAllFF = false;
					Serial.printf("[LGFX] Retry probe succeeded: ID=0x%04X RDDID=%02X %02X %02X\n",
					              id, _lastRddid[0], _lastRddid[1], _lastRddid[2]);
					break;
				}
			}
		}

		// Probe result may have changed after retry.
		probeAmbiguous = invalidProbeAllFF || ambiguousProbeAllZero;
		if (probeAmbiguous) {
			persistedDisplay = _loadPersistedDisplay();
		}

		// Some 4" panels (ILI9488, ILI9486) don't implement 0xD3 over SPI and return 0x0000.
		// Use FORCE_DISPLAY_ILI9488 build flag to bypass auto-detect for those panels.
#if defined(FORCE_DISPLAY_ILI9488)
		activeDisplay = DISPLAY_ILI9488;
		Serial.println("[LGFX] FORCED ILI9488 (4.0\") via build flag");
		_configILI9488();
#else
		if (id == 0x9488) {
			activeDisplay = DISPLAY_ILI9488;
			Serial.println("[LGFX] Detected ILI9488 (4.0\")");
			_configILI9488();
		} else if (id == 0x7796 || (id & 0xFF) == 0x77) {
			activeDisplay = DISPLAY_ST7796;
			Serial.println("[LGFX] Detected ST7796S (4.0\")");
			_configST7796();
		} else if ((id & 0xFF) == 0x85 || id == 0x7789) {
			activeDisplay = DISPLAY_ST7789;
			Serial.println("[LGFX] Detected ST7789 (2.8\")");
			_configST7789();
		} else if (id == 0x0000 && (
		           // For this 4" panel family, first RDDID byte can vary (03/C3/43),
		           // but trailing bytes are stable per module revision.
		           (_lastRddid[0] == 0xFF && _lastRddid[1] == 0xC0 && _lastRddid[2] == 0x00) ||
		           (_lastRddid[1] == 0xFF && _lastRddid[2] == 0xC0) ||
		           (_lastRddid[1] == 0xFE && _lastRddid[2] == 0x00))) {
			activeDisplay = DISPLAY_ILI9488;
			Serial.printf("[LGFX] Detected 4\" panel by RDDID signature (%02X %02X %02X), using ILI9488-compatible config\n",
			              _lastRddid[0], _lastRddid[1], _lastRddid[2]);
			_configILI9488();
		} else if ((invalidProbeAllFF || ambiguousProbeAllZero) && g_lgfx_last_good_display >= DISPLAY_ILI9341 && g_lgfx_last_good_display <= DISPLAY_ILI9488) {
			activeDisplay = static_cast<DisplayVariant>(g_lgfx_last_good_display);
			Serial.printf("[LGFX] Probe ambiguous/invalid (ID=0x%04X RDDID=%02X %02X %02X), reusing last detected display type=%d\n",
			              id, _lastRddid[0], _lastRddid[1], _lastRddid[2], activeDisplay);
			switch (activeDisplay) {
				case DISPLAY_ILI9488: _configILI9488(); break;
				case DISPLAY_ST7796: _configST7796(); break;
				case DISPLAY_ST7789: _configST7789(); break;
				case DISPLAY_ILI9341:
				default: _configILI9341(); break;
			}
		} else if ((invalidProbeAllFF || ambiguousProbeAllZero) && persistedDisplay >= DISPLAY_ILI9341 && persistedDisplay <= DISPLAY_ILI9488) {
			activeDisplay = static_cast<DisplayVariant>(persistedDisplay);
			Serial.printf("[LGFX] Probe ambiguous/invalid (ID=0x%04X RDDID=%02X %02X %02X), reusing persisted display type=%d\n",
			              id, _lastRddid[0], _lastRddid[1], _lastRddid[2], activeDisplay);
			switch (activeDisplay) {
				case DISPLAY_ILI9488: _configILI9488(); break;
				case DISPLAY_ST7796: _configST7796(); break;
				case DISPLAY_ST7789: _configST7789(); break;
				case DISPLAY_ILI9341:
				default: _configILI9341(); break;
			}
		} else if (id == 0x0000 && _lastRddid[0] == 0x00 && _lastRddid[1] == 0x00 && _lastRddid[2] == 0x00) {
			activeDisplay = DISPLAY_ILI9341;
			Serial.println("[LGFX] Detected ILI9341 by RDDID signature (00 00 00) (3.2\")");
			_configILI9341();
		} else {
			activeDisplay = DISPLAY_ILI9341;
			Serial.printf("[LGFX] Unknown display signature: ID=0x%04X RDDID=%02X %02X %02X\n",
			              id, _lastRddid[0], _lastRddid[1], _lastRddid[2]);
			Serial.println("[LGFX] Falling back to ILI9341 config (use FORCE_DISPLAY_ILI9488 for 4\" ILI948x panels)");
			_configILI9341();
		}
#endif

		// Cache only when probe is not ambiguous. This prevents bad probe reads
		// (all-zero/all-FF) from poisoning future boot detection.
		if (!probeAmbiguous) {
			g_lgfx_last_good_display = static_cast<int>(activeDisplay);
			_savePersistedDisplay(activeDisplay);
		} else {
			Serial.printf("[LGFX] Not updating cached display type due ambiguous probe (ID=0x%04X RDDID=%02X %02X %02X)\n",
			              id, _lastRddid[0], _lastRddid[1], _lastRddid[2]);
		}

		begin();
		// ILI9488/ST7796 have different native orientation — rotation=1 gives landscape
		// ILI9341/ST7789 use rotation=3 (hardware-confirmed for this PCB)
		if (activeDisplay == DISPLAY_ST7796 || activeDisplay == DISPLAY_ILI9488) {
			setRotation(1);
		} else {
			setRotation(LGFX_DEFAULT_ROTATION);
		}
		return true;
	}

private:
	int _loadPersistedDisplay() {
		Preferences p;
		if (!p.begin(LGFX_PREFS_NAMESPACE, true)) {
			return -1;
		}
		int v = p.getInt(LGFX_PREFS_KEY_DISPLAY, -1);
		p.end();
		return v;
	}

	void _savePersistedDisplay(DisplayVariant variant) {
		Preferences p;
		if (!p.begin(LGFX_PREFS_NAMESPACE, false)) {
			return;
		}
		int current = p.getInt(LGFX_PREFS_KEY_DISPLAY, -1);
		if (current != static_cast<int>(variant)) {
			p.putInt(LGFX_PREFS_KEY_DISPLAY, static_cast<int>(variant));
		}
		p.end();
	}

	// Returns a combined 16-bit ID: high byte from 0xD3 (IC ID, bytes 3+4), low byte from 0x04 (RDDID byte 3)
	// 0xD3 is more reliable for identifying ILI9488 and ILI9486; 0x04 works for ILI9341/ST7796/ST7789
	uint16_t _probeDisplayId() {
		SPIClass probeSpi(FSPI);
		uint8_t rddidSamples[3][4] = {{0}}; // includes 1 dummy + 3 ID bytes
		uint8_t idd3Samples[3][5] = {{0}};  // includes 1 dummy + 4 ID bytes

		pinMode(LGFX_PIN_CS, OUTPUT);
		digitalWrite(LGFX_PIN_CS, HIGH);
		pinMode(LGFX_PIN_DC, OUTPUT);
		digitalWrite(LGFX_PIN_DC, HIGH);
		pinMode(LGFX_PIN_RST, OUTPUT);

		digitalWrite(LGFX_PIN_RST, LOW);
		delay(8);
		digitalWrite(LGFX_PIN_RST, HIGH);
		delay(64);

		probeSpi.begin(LGFX_PIN_SCLK, LGFX_PIN_MISO, LGFX_PIN_MOSI, -1);
		// Use slower probe speed for reliable readback on panels with weak/slow MISO drive.
		probeSpi.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));

		auto majority3 = [](uint8_t a, uint8_t b, uint8_t c) -> uint8_t {
			if (a == b || a == c) return a;
			if (b == c) return b;
			return c; // fallback if all differ
		};

		for (int s = 0; s < 3; ++s) {
			// Read 0x04 RDDID. Many controllers insert a dummy byte first.
			digitalWrite(LGFX_PIN_CS, LOW);
			digitalWrite(LGFX_PIN_DC, LOW);
			probeSpi.transfer(0x04);
			digitalWrite(LGFX_PIN_DC, HIGH);
			for (int i = 0; i < 4; ++i) {
				rddidSamples[s][i] = probeSpi.transfer(0x00);
			}
			digitalWrite(LGFX_PIN_CS, HIGH);
			delay(1);

			// Read 0xD3 IC ID. Also expect a dummy byte first on SPI.
			digitalWrite(LGFX_PIN_CS, LOW);
			digitalWrite(LGFX_PIN_DC, LOW);
			probeSpi.transfer(0xD3);
			digitalWrite(LGFX_PIN_DC, HIGH);
			for (int i = 0; i < 5; ++i) {
				idd3Samples[s][i] = probeSpi.transfer(0x00);
			}
			digitalWrite(LGFX_PIN_CS, HIGH);
			delay(1);
		}

		probeSpi.endTransaction();
		probeSpi.end();

		// Parse bytes after dummy and apply per-byte majority across 3 reads.
		_lastRddid[0] = majority3(rddidSamples[0][1], rddidSamples[1][1], rddidSamples[2][1]);
		_lastRddid[1] = majority3(rddidSamples[0][2], rddidSamples[1][2], rddidSamples[2][2]);
		_lastRddid[2] = majority3(rddidSamples[0][3], rddidSamples[1][3], rddidSamples[2][3]);

		uint8_t d3b0 = majority3(idd3Samples[0][1], idd3Samples[1][1], idd3Samples[2][1]);
		uint8_t d3b1 = majority3(idd3Samples[0][2], idd3Samples[1][2], idd3Samples[2][2]);
		uint8_t d3b2 = majority3(idd3Samples[0][3], idd3Samples[1][3], idd3Samples[2][3]);
		uint8_t d3b3 = majority3(idd3Samples[0][4], idd3Samples[1][4], idd3Samples[2][4]);

		uint16_t icId = ((uint16_t)d3b2 << 8) | d3b3;
		Serial.printf("[LGFX] 0x04 RDDID bytes: %02X %02X %02X\n", _lastRddid[0], _lastRddid[1], _lastRddid[2]);
		Serial.printf("[LGFX] 0xD3 IC ID bytes: %02X %02X %02X %02X  → IC ID=0x%04X\n",
		              d3b0, d3b1, d3b2, d3b3, icId);
		return icId;
	}

	void _configILI9341() {
		auto cfg = _panel_ili9341.config();
		cfg.pin_cs       = LGFX_PIN_CS;
		cfg.pin_rst      = LGFX_PIN_RST;
		cfg.pin_busy     = -1;
		cfg.panel_width  = 240;
		cfg.panel_height = 320;
		cfg.readable     = true;
		cfg.invert       = false;
		cfg.rgb_order    = false;
		cfg.dlen_16bit   = false;
		cfg.bus_shared   = true;
		_panel_ili9341.config(cfg);
		_panel_ili9341.setBus(&_bus);
		_panel_ili9341.setLight(&_light);
		_panel_ili9341.setTouch(&_touch);
		setPanel(&_panel_ili9341);
	}

	void _configST7789() {
		auto cfg = _panel_st7789.config();
		cfg.pin_cs       = LGFX_PIN_CS;
		cfg.pin_rst      = LGFX_PIN_RST;
		cfg.pin_busy     = -1;
		cfg.panel_width  = 240;
		cfg.panel_height = 320;
		cfg.readable     = false;
		cfg.invert       = true;
		cfg.rgb_order    = false;
		cfg.dlen_16bit   = false;
		cfg.bus_shared   = true;
		_panel_st7789.config(cfg);
		_panel_st7789.setBus(&_bus);
		_panel_st7789.setLight(&_light);
		_panel_st7789.setTouch(&_touch);
		setPanel(&_panel_st7789);
	}

	void _configST7796() {
		auto cfg = _panel_st7796.config();
		cfg.pin_cs         = LGFX_PIN_CS;
		cfg.pin_rst        = LGFX_PIN_RST;
		cfg.pin_busy       = -1;
		cfg.panel_width    = 320;   // Native portrait width
		cfg.panel_height   = 480;   // Native portrait height
		cfg.offset_x       = 0;
		cfg.offset_y       = 0;
		cfg.offset_rotation = 0;    // Adjust 0-3 if landscape is still wrong after setRotation()
		cfg.readable       = true;
		cfg.invert         = false;
		cfg.rgb_order      = false;
		cfg.dlen_16bit     = false;
		cfg.bus_shared     = true;
		_panel_st7796.config(cfg);
		_panel_st7796.setBus(&_bus);
		_panel_st7796.setLight(&_light);
		_panel_st7796.setTouch(&_touch);
		setPanel(&_panel_st7796);
	}

	void _configILI9488() {
		auto cfg = _panel_ili9488.config();
		cfg.pin_cs          = LGFX_PIN_CS;
		cfg.pin_rst         = LGFX_PIN_RST;
		cfg.pin_busy        = -1;
		cfg.panel_width     = 320;   // Native portrait width
		cfg.panel_height    = 480;   // Native portrait height
		cfg.offset_x        = 0;
		cfg.offset_y        = 0;
		cfg.offset_rotation = 0;
		cfg.readable        = false; // ILI9488 SPI does not support read-back
		cfg.invert          = false;
		cfg.rgb_order       = false;
		cfg.dlen_16bit      = false;
		cfg.bus_shared      = true;
		_panel_ili9488.config(cfg);
		_panel_ili9488.setBus(&_bus);
		_panel_ili9488.setLight(&_light);
		_panel_ili9488.setTouch(&_touch);
		setPanel(&_panel_ili9488);
	}
};
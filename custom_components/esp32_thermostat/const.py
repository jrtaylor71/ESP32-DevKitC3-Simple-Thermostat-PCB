"""Constants for the ESP32 Simple Thermostat integration."""

from __future__ import annotations

from datetime import timedelta
from typing import Final

DOMAIN: Final = "esp32_thermostat"

CONF_TOKEN: Final = "token"

API_BASE_PATH: Final = "/api/v1"
DEFAULT_PORT: Final = 80

REQUEST_TIMEOUT: Final = 10
SSE_SOCK_READ_TIMEOUT: Final = 75
SSE_STALE_SECONDS: Final = 75
SSE_BACKOFF_INITIAL: Final = 5
SSE_BACKOFF_MAX: Final = 60

UPDATE_INTERVAL: Final = timedelta(seconds=30)
INFO_REV_RELOAD_DELAY: Final = 5

CARD_URL: Final = "/esp32_thermostat_static/esp32-thermostat-card.js"
CARD_FILENAME: Final = "esp32-thermostat-card.js"

DAYS: Final = (
    "sunday",
    "monday",
    "tuesday",
    "wednesday",
    "thursday",
    "friday",
    "saturday",
)

OVERRIDE_RESUME: Final = "resume"
OVERRIDE_TEMPORARY: Final = "temporary"
OVERRIDE_PERMANENT: Final = "permanent"
OVERRIDE_MODES: Final = (OVERRIDE_RESUME, OVERRIDE_TEMPORARY, OVERRIDE_PERMANENT)

PRESET_NONE: Final = "none"
PRESET_SCHEDULE: Final = "schedule"
PRESET_TEMPORARY_HOLD: Final = "temporary_hold"
PRESET_PERMANENT_HOLD: Final = "permanent_hold"

SERVICE_SET_SCHEDULE: Final = "set_schedule"
SERVICE_GET_SCHEDULE: Final = "get_schedule"
SERVICE_SET_OVERRIDE: Final = "set_override"

ATTR_DEVICE_ID: Final = "device_id"
ATTR_SCHEDULE_ENABLED: Final = "schedule_enabled"
ATTR_DAYS: Final = "days"
ATTR_MODE: Final = "mode"

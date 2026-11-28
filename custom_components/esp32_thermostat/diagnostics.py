# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Diagnostics for the ESP32 Simple Thermostat integration."""

from __future__ import annotations

from typing import Any

from homeassistant.components.diagnostics import async_redact_data
from homeassistant.core import HomeAssistant

from .const import CONF_TOKEN
from .coordinator import ThermostatConfigEntry

TO_REDACT = {CONF_TOKEN, "mac"}


async def async_get_config_entry_diagnostics(
    hass: HomeAssistant, entry: ThermostatConfigEntry
) -> dict[str, Any]:
    """Return diagnostics for a config entry."""
    coordinator = entry.runtime_data
    return {
        "entry": async_redact_data(dict(entry.data), TO_REDACT),
        "info": async_redact_data(coordinator.info, TO_REDACT),
        "state": coordinator.data,
        "event_stream": {
            "connected": coordinator.sse_connected,
            "last_update_success": coordinator.last_update_success,
        },
    }

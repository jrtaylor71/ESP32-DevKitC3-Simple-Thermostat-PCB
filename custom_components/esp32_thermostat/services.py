"""Service actions for the ESP32 Simple Thermostat integration."""

from __future__ import annotations

from typing import Any

import voluptuous as vol

from homeassistant.config_entries import ConfigEntryState
from homeassistant.core import (
    HomeAssistant,
    ServiceCall,
    ServiceResponse,
    SupportsResponse,
    callback,
)
from homeassistant.exceptions import ServiceValidationError
from homeassistant.helpers import config_validation as cv, device_registry as dr

from .const import (
    ATTR_DAYS,
    ATTR_DEVICE_ID,
    ATTR_MODE,
    ATTR_SCHEDULE_ENABLED,
    DAYS,
    DOMAIN,
    OVERRIDE_MODES,
    SERVICE_GET_SCHEDULE,
    SERVICE_SET_OVERRIDE,
    SERVICE_SET_SCHEDULE,
)
from .coordinator import ThermostatConfigEntry, ThermostatCoordinator

_DAY_KEYS = ("day", "index", "enabled", "day_period", "night_period")
_PERIOD_KEYS = ("hour", "minute", "heat", "cool", "auto", "active")

DEVICE_SCHEMA = vol.Schema({vol.Required(ATTR_DEVICE_ID): cv.string})

SET_SCHEDULE_SCHEMA = DEVICE_SCHEMA.extend(
    {
        vol.Optional(ATTR_SCHEDULE_ENABLED): cv.boolean,
        vol.Optional(ATTR_DAYS): vol.All(cv.ensure_list, [dict]),
    }
)

SET_OVERRIDE_SCHEMA = DEVICE_SCHEMA.extend(
    {vol.Required(ATTR_MODE): vol.In(OVERRIDE_MODES)}
)


def _coordinator_for_device(hass: HomeAssistant, device_id: str) -> ThermostatCoordinator:
    device = dr.async_get(hass).async_get(device_id)
    if device is None:
        raise ServiceValidationError(
            translation_domain=DOMAIN,
            translation_key="invalid_device_id",
            translation_placeholders={"device_id": device_id},
        )
    for entry_id in device.config_entries:
        entry: ThermostatConfigEntry | None = hass.config_entries.async_get_entry(
            entry_id
        )
        if entry is None or entry.domain != DOMAIN:
            continue
        if entry.state is not ConfigEntryState.LOADED:
            raise ServiceValidationError(
                translation_domain=DOMAIN,
                translation_key="device_not_loaded",
                translation_placeholders={"device_id": device_id},
            )
        return entry.runtime_data
    raise ServiceValidationError(
        translation_domain=DOMAIN,
        translation_key="invalid_device_id",
        translation_placeholders={"device_id": device_id},
    )


def _invalid_days(detail: str) -> ServiceValidationError:
    return ServiceValidationError(
        translation_domain=DOMAIN,
        translation_key="invalid_days",
        translation_placeholders={"detail": detail},
    )


def _clean_days(days: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """Reduce day entries to the fields accepted by PUT /schedule.

    Allows passing the `days` list returned by get_schedule straight back in.
    """
    cleaned: list[dict[str, Any]] = []
    for raw in days:
        day = {key: raw[key] for key in _DAY_KEYS if key in raw}
        if "day" in day:
            day["day"] = str(day["day"]).lower()
            if day["day"] not in DAYS:
                raise _invalid_days(f"unknown day '{day['day']}'")
            day.pop("index", None)
        elif "index" not in day:
            raise _invalid_days("each day needs 'day' or 'index'")
        for period in ("day_period", "night_period"):
            if period in day:
                if not isinstance(day[period], dict):
                    raise _invalid_days(f"'{period}' must be an object")
                day[period] = {
                    key: day[period][key] for key in _PERIOD_KEYS if key in day[period]
                }
        cleaned.append(day)
    return cleaned


@callback
def async_setup_services(hass: HomeAssistant) -> None:
    """Register integration services."""

    async def _set_schedule(call: ServiceCall) -> None:
        coordinator = _coordinator_for_device(hass, call.data[ATTR_DEVICE_ID])
        payload: dict[str, Any] = {}
        if ATTR_SCHEDULE_ENABLED in call.data:
            payload["enabled"] = call.data[ATTR_SCHEDULE_ENABLED]
        if ATTR_DAYS in call.data:
            payload["days"] = _clean_days(call.data[ATTR_DAYS])
        if not payload:
            raise ServiceValidationError(
                translation_domain=DOMAIN, translation_key="nothing_to_set"
            )
        await coordinator.async_put_schedule(payload)

    async def _get_schedule(call: ServiceCall) -> ServiceResponse:
        coordinator = _coordinator_for_device(hass, call.data[ATTR_DEVICE_ID])
        return await coordinator.async_get_schedule()

    async def _set_override(call: ServiceCall) -> None:
        coordinator = _coordinator_for_device(hass, call.data[ATTR_DEVICE_ID])
        await coordinator.async_send_command(schedule_override=call.data[ATTR_MODE])

    hass.services.async_register(
        DOMAIN, SERVICE_SET_SCHEDULE, _set_schedule, schema=SET_SCHEDULE_SCHEMA
    )
    hass.services.async_register(
        DOMAIN,
        SERVICE_GET_SCHEDULE,
        _get_schedule,
        schema=DEVICE_SCHEMA,
        supports_response=SupportsResponse.ONLY,
    )
    hass.services.async_register(
        DOMAIN, SERVICE_SET_OVERRIDE, _set_override, schema=SET_OVERRIDE_SCHEMA
    )

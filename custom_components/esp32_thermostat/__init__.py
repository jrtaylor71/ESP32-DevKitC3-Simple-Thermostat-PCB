"""The ESP32 Simple Thermostat integration."""

from __future__ import annotations

from homeassistant.const import CONF_HOST, Platform
from homeassistant.core import HomeAssistant
from homeassistant.exceptions import ConfigEntryAuthFailed, ConfigEntryNotReady
from homeassistant.helpers import config_validation as cv
from homeassistant.helpers.aiohttp_client import async_get_clientsession
from homeassistant.helpers.typing import ConfigType

from .api import ThermostatApiClient, ThermostatAuthError, ThermostatError
from .const import CONF_TOKEN, DOMAIN
from .coordinator import ThermostatConfigEntry, ThermostatCoordinator
from .services import async_setup_services

PLATFORMS: list[Platform] = [
    Platform.BINARY_SENSOR,
    Platform.BUTTON,
    Platform.CLIMATE,
    Platform.NUMBER,
    Platform.SENSOR,
    Platform.SWITCH,
]

CONFIG_SCHEMA = cv.config_entry_only_config_schema(DOMAIN)


async def async_setup(hass: HomeAssistant, config: ConfigType) -> bool:
    """Set up the integration (register services)."""
    async_setup_services(hass)
    return True


async def async_setup_entry(hass: HomeAssistant, entry: ThermostatConfigEntry) -> bool:
    """Set up a thermostat from a config entry."""
    api = ThermostatApiClient(
        entry.data[CONF_HOST], entry.data[CONF_TOKEN], async_get_clientsession(hass)
    )
    try:
        info = await api.async_get_info()
    except ThermostatAuthError as err:
        raise ConfigEntryAuthFailed(
            translation_domain=DOMAIN, translation_key="auth_failed"
        ) from err
    except ThermostatError as err:
        raise ConfigEntryNotReady(
            translation_domain=DOMAIN,
            translation_key="communication_error",
            translation_placeholders={"error": str(err)},
        ) from err

    if entry.unique_id and info.get("device_id") != entry.unique_id:
        raise ConfigEntryNotReady(
            translation_domain=DOMAIN,
            translation_key="wrong_device",
            translation_placeholders={
                "host": entry.data[CONF_HOST],
                "device_id": str(info.get("device_id")),
            },
        )

    coordinator = ThermostatCoordinator(hass, entry, api, info)
    await coordinator.async_config_entry_first_refresh()
    entry.runtime_data = coordinator

    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)
    coordinator.async_start_event_stream()
    return True


async def async_unload_entry(hass: HomeAssistant, entry: ThermostatConfigEntry) -> bool:
    """Unload a config entry."""
    await entry.runtime_data.async_stop_event_stream()
    return await hass.config_entries.async_unload_platforms(entry, PLATFORMS)

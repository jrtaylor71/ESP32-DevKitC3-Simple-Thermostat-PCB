"""Select platform for the ESP32 Simple Thermostat integration."""

from __future__ import annotations

from dataclasses import dataclass

from homeassistant.components.select import SelectEntity, SelectEntityDescription
from homeassistant.const import EntityCategory
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .coordinator import ThermostatConfigEntry, ThermostatCoordinator
from .entity import ThermostatEntity

PARALLEL_UPDATES = 1

TIME_ZONES: dict[str, str] = {
    "eastern": "EST5EDT,M3.2.0,M11.1.0",
    "central": "CST6CDT,M3.2.0,M11.1.0",
    "mountain": "MST7MDT,M3.2.0,M11.1.0",
    "pacific": "PST8PDT,M3.2.0,M11.1.0",
    "alaska": "AKST9AKDT,M3.2.0,M11.1.0",
    "hawaii": "HST10",
    "uk": "GMT0BST,M3.5.0,M10.5.0",
    "central_europe": "CET-1CEST,M3.5.0,M10.5.0",
    "japan": "JST-9",
    "australia_east": "AEST-10AEDT,M10.1.0,M4.1.0",
}


@dataclass(frozen=True, kw_only=True)
class ThermostatSelectEntityDescription(SelectEntityDescription):
    """Describes a select backed by a device setting.

    `values` maps HA option (translation key) -> value sent to/reported by the device.
    """

    values: dict[str, str]


def _describe(key: str, values: dict[str, str]) -> ThermostatSelectEntityDescription:
    return ThermostatSelectEntityDescription(
        key=key, translation_key=key, options=list(values), values=values
    )


SELECTS: tuple[ThermostatSelectEntityDescription, ...] = (
    _describe(
        "backup_heat_relay", {"pump": "pump", "heat2": "heat2", "cool2": "cool2"}
    ),
    _describe(
        "eu_humidity_relay", {"cool1": "cool1", "cool2": "cool2", "pump": "pump"}
    ),
    _describe("temperature_unit", {"fahrenheit": "F", "celsius": "C"}),
    _describe("region", {"us": "US", "eu": "EU"}),
    _describe("time_zone", TIME_ZONES),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ThermostatConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up select entities."""
    coordinator = entry.runtime_data
    async_add_entities(ThermostatSelect(coordinator, desc) for desc in SELECTS)


class ThermostatSelect(ThermostatEntity, SelectEntity):
    """A thermostat setting with a fixed set of values."""

    entity_description: ThermostatSelectEntityDescription
    _attr_entity_category = EntityCategory.CONFIG

    def __init__(
        self,
        coordinator: ThermostatCoordinator,
        description: ThermostatSelectEntityDescription,
    ) -> None:
        """Initialize the select entity."""
        self.entity_description = description
        super().__init__(coordinator, description.key)

    @property
    def current_option(self) -> str | None:
        """Return the option matching the device value (None if unknown)."""
        value = (self.state_data.get("settings") or {}).get(
            self.entity_description.key
        )
        for option, device_value in self.entity_description.values.items():
            if device_value == value:
                return option
        return None

    async def async_select_option(self, option: str) -> None:
        """Write the setting to the device."""
        await self.coordinator.async_send_settings(
            **{self.entity_description.key: self.entity_description.values[option]}
        )

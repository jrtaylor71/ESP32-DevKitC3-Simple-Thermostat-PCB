"""Number platform for the ESP32 Simple Thermostat integration."""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass

from homeassistant.components.number import (
    NumberDeviceClass,
    NumberEntity,
    NumberEntityDescription,
    NumberMode,
)
from homeassistant.const import PERCENTAGE, EntityCategory, UnitOfTime
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .coordinator import ThermostatConfigEntry, ThermostatCoordinator
from .entity import ThermostatEntity, device_temperature_unit

PARALLEL_UPDATES = 1


@dataclass(frozen=True, kw_only=True)
class ThermostatNumberEntityDescription(NumberEntityDescription):
    """Describes a thermostat setting."""

    exists_fn: Callable[[ThermostatCoordinator], bool] = lambda _: True
    integer: bool = False
    device_temperature: bool = False


def _stage2(coordinator: ThermostatCoordinator) -> bool:
    return coordinator.has_feature("stage2_heat", "stage2_cool")


NUMBERS: tuple[ThermostatNumberEntityDescription, ...] = (
    ThermostatNumberEntityDescription(
        key="temp_swing",
        translation_key="temp_swing",
        native_min_value=0.2,
        native_max_value=3.0,
        native_step=0.1,
        mode=NumberMode.BOX,
        device_temperature=True,
    ),
    ThermostatNumberEntityDescription(
        key="auto_temp_swing",
        translation_key="auto_temp_swing",
        native_min_value=0.2,
        native_max_value=5.0,
        native_step=0.1,
        mode=NumberMode.BOX,
        device_temperature=True,
    ),
    ThermostatNumberEntityDescription(
        key="stage1_min_runtime",
        translation_key="stage1_min_runtime",
        device_class=NumberDeviceClass.DURATION,
        native_unit_of_measurement=UnitOfTime.SECONDS,
        native_min_value=60,
        native_max_value=1800,
        native_step=30,
        mode=NumberMode.BOX,
        integer=True,
        exists_fn=_stage2,
    ),
    ThermostatNumberEntityDescription(
        key="stage2_temp_delta",
        translation_key="stage2_temp_delta",
        native_min_value=0.5,
        native_max_value=5.0,
        native_step=0.5,
        mode=NumberMode.BOX,
        device_temperature=True,
        exists_fn=_stage2,
    ),
    ThermostatNumberEntityDescription(
        key="fan_minutes_per_hour",
        translation_key="fan_minutes_per_hour",
        native_unit_of_measurement=UnitOfTime.MINUTES,
        native_min_value=0,
        native_max_value=60,
        # Firmware runs the fan cycle in 5-minute increments.
        native_step=5,
        mode=NumberMode.SLIDER,
        integer=True,
    ),
    ThermostatNumberEntityDescription(
        key="shower_duration",
        translation_key="shower_duration",
        native_unit_of_measurement=UnitOfTime.MINUTES,
        native_min_value=5,
        native_max_value=120,
        native_step=1,
        mode=NumberMode.BOX,
        integer=True,
        exists_fn=lambda c: c.has_feature("shower_mode"),
    ),
    ThermostatNumberEntityDescription(
        key="display_brightness",
        translation_key="display_brightness",
        native_min_value=30,
        native_max_value=255,
        native_step=1,
        mode=NumberMode.SLIDER,
        integer=True,
        entity_registry_enabled_default=False,
    ),
    ThermostatNumberEntityDescription(
        key="hydronic_temp_low",
        translation_key="hydronic_temp_low",
        device_class=NumberDeviceClass.TEMPERATURE,
        native_min_value=50,
        native_max_value=200,
        native_step=0.5,
        mode=NumberMode.BOX,
        device_temperature=True,
        exists_fn=lambda c: c.has_feature("hydronic_heat"),
    ),
    ThermostatNumberEntityDescription(
        key="hydronic_temp_high",
        translation_key="hydronic_temp_high",
        device_class=NumberDeviceClass.TEMPERATURE,
        native_min_value=50,
        native_max_value=200,
        native_step=0.5,
        mode=NumberMode.BOX,
        device_temperature=True,
        exists_fn=lambda c: c.has_feature("hydronic_heat"),
    ),
    ThermostatNumberEntityDescription(
        key="eu_humidity_setpoint",
        translation_key="eu_humidity_setpoint",
        device_class=NumberDeviceClass.HUMIDITY,
        native_unit_of_measurement=PERCENTAGE,
        native_min_value=30,
        native_max_value=90,
        native_step=1,
        mode=NumberMode.SLIDER,
        exists_fn=lambda c: c.has_feature("eu_humidity_control"),
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ThermostatConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up number entities."""
    coordinator = entry.runtime_data
    async_add_entities(
        ThermostatNumber(coordinator, description)
        for description in NUMBERS
        if description.exists_fn(coordinator)
    )


class ThermostatNumber(ThermostatEntity, NumberEntity):
    """A thermostat setting from /settings."""

    entity_description: ThermostatNumberEntityDescription
    _attr_entity_category = EntityCategory.CONFIG

    def __init__(
        self,
        coordinator: ThermostatCoordinator,
        description: ThermostatNumberEntityDescription,
    ) -> None:
        """Initialize the number entity."""
        self.entity_description = description
        super().__init__(coordinator, description.key)
        if description.device_temperature:
            # Swing/delta values are temperature differences; only absolute
            # temperatures get the TEMPERATURE device class (and conversion).
            self._attr_native_unit_of_measurement = device_temperature_unit(
                coordinator
            )

    @property
    def native_value(self) -> float | None:
        """Return the current setting value."""
        settings = self.state_data.get("settings") or {}
        return settings.get(self.entity_description.key)

    async def async_set_native_value(self, value: float) -> None:
        """Update the setting on the device."""
        payload: float | int = (
            int(round(value)) if self.entity_description.integer else value
        )
        await self.coordinator.async_send_settings(
            **{self.entity_description.key: payload}
        )

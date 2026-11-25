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
from homeassistant.const import (
    PERCENTAGE,
    EntityCategory,
    UnitOfTemperature,
    UnitOfTime,
)
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
    # (min, max) used instead of native_min/max_value when the device unit is °C.
    celsius_range: tuple[float, float] | None = None


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
        celsius_range=(10, 95),
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
        celsius_range=(10, 95),
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
    ThermostatNumberEntityDescription(
        key="backup_heat_delay_min",
        translation_key="backup_heat_delay_min",
        device_class=NumberDeviceClass.DURATION,
        native_unit_of_measurement=UnitOfTime.MINUTES,
        native_min_value=5,
        native_max_value=180,
        native_step=1,
        mode=NumberMode.BOX,
        integer=True,
    ),
    ThermostatNumberEntityDescription(
        key="backup_heat_min_rise",
        translation_key="backup_heat_min_rise",
        native_min_value=0.1,
        native_max_value=5.0,
        native_step=0.1,
        mode=NumberMode.BOX,
        device_temperature=True,
    ),
    ThermostatNumberEntityDescription(
        key="backup_heat_max_drop",
        translation_key="backup_heat_max_drop",
        native_min_value=0.1,
        native_max_value=10.0,
        native_step=0.1,
        mode=NumberMode.BOX,
        device_temperature=True,
    ),
    ThermostatNumberEntityDescription(
        key="eu_humidity_deadband",
        translation_key="eu_humidity_deadband",
        native_unit_of_measurement=PERCENTAGE,
        native_min_value=1,
        native_max_value=20,
        native_step=0.5,
        mode=NumberMode.BOX,
    ),
    ThermostatNumberEntityDescription(
        key="temp_offset",
        translation_key="temp_offset",
        # Temperature difference in the device unit (firmware stores ±10 °C).
        native_min_value=-18.0,
        native_max_value=18.0,
        native_step=0.1,
        mode=NumberMode.BOX,
        device_temperature=True,
        celsius_range=(-10.0, 10.0),
    ),
    ThermostatNumberEntityDescription(
        key="humidity_offset",
        translation_key="humidity_offset",
        native_unit_of_measurement=PERCENTAGE,
        native_min_value=-50.0,
        native_max_value=50.0,
        native_step=0.5,
        mode=NumberMode.BOX,
    ),
)


# Per-mode setpoints; the climate entity only exposes the active mode's target.
SETPOINT_KEYS = ("target_temp_heat", "target_temp_cool", "target_temp_auto")


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ThermostatConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up number entities."""
    coordinator = entry.runtime_data
    entities: list[NumberEntity] = [
        ThermostatNumber(coordinator, description)
        for description in NUMBERS
        if description.exists_fn(coordinator)
    ]
    entities.extend(
        ThermostatSetpoint(coordinator, key) for key in SETPOINT_KEYS
    )
    async_add_entities(entities)


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
        if (
            description.celsius_range is not None
            and device_temperature_unit(coordinator) == UnitOfTemperature.CELSIUS
        ):
            self._attr_native_min_value, self._attr_native_max_value = (
                description.celsius_range
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


class ThermostatSetpoint(ThermostatEntity, NumberEntity):
    """The heat, cool, or auto setpoint, editable regardless of the current mode."""

    _attr_device_class = NumberDeviceClass.TEMPERATURE
    _attr_mode = NumberMode.BOX

    def __init__(self, coordinator: ThermostatCoordinator, key: str) -> None:
        """Initialize the setpoint entity."""
        super().__init__(coordinator, key)
        self._key = key
        self._attr_translation_key = key
        info = coordinator.info
        self._attr_native_unit_of_measurement = device_temperature_unit(coordinator)
        self._attr_native_min_value = float(info.get("min_temp", 50))
        self._attr_native_max_value = float(info.get("max_temp", 95))
        self._attr_native_step = float(info.get("temp_step", 0.5))

    @property
    def native_value(self) -> float | None:
        """Return the setpoint."""
        value = self.state_data.get(self._key)
        return None if value is None else float(value)

    async def async_set_native_value(self, value: float) -> None:
        """Change the setpoint (starts a temporary override while the schedule runs)."""
        await self.coordinator.async_send_command(**{self._key: value})

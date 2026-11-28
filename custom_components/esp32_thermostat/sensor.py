# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Sensor platform for the ESP32 Simple Thermostat integration."""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass
from typing import Any

from homeassistant.components.sensor import (
    SensorDeviceClass,
    SensorEntity,
    SensorEntityDescription,
    SensorStateClass,
)
from homeassistant.const import (
    PERCENTAGE,
    SIGNAL_STRENGTH_DECIBELS_MILLIWATT,
    EntityCategory,
    UnitOfInformation,
    UnitOfPressure,
    UnitOfTime,
)
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .coordinator import ThermostatConfigEntry, ThermostatCoordinator
from .entity import ThermostatEntity, device_temperature_unit, nested

PARALLEL_UPDATES = 0


@dataclass(frozen=True, kw_only=True)
class ThermostatSensorEntityDescription(SensorEntityDescription):
    """Describes a thermostat sensor."""

    value_fn: Callable[[dict[str, Any]], Any]
    exists_fn: Callable[[ThermostatCoordinator], bool] = lambda _: True
    available_fn: Callable[[dict[str, Any]], bool] = lambda _: True
    device_temperature: bool = False


def _has_weather(data: dict[str, Any]) -> bool:
    return isinstance(data.get("weather"), dict)


SENSORS: tuple[ThermostatSensorEntityDescription, ...] = (
    ThermostatSensorEntityDescription(
        key="current_temperature",
        translation_key="current_temperature",
        device_class=SensorDeviceClass.TEMPERATURE,
        state_class=SensorStateClass.MEASUREMENT,
        device_temperature=True,
        value_fn=nested("current_temperature"),
    ),
    ThermostatSensorEntityDescription(
        key="humidity",
        translation_key="humidity",
        device_class=SensorDeviceClass.HUMIDITY,
        state_class=SensorStateClass.MEASUREMENT,
        native_unit_of_measurement=PERCENTAGE,
        value_fn=nested("current_humidity"),
        exists_fn=lambda c: c.has_feature("humidity"),
    ),
    ThermostatSensorEntityDescription(
        key="pressure",
        translation_key="pressure",
        device_class=SensorDeviceClass.ATMOSPHERIC_PRESSURE,
        state_class=SensorStateClass.MEASUREMENT,
        native_unit_of_measurement=UnitOfPressure.HPA,
        value_fn=nested("pressure_hpa"),
        exists_fn=lambda c: c.has_feature("pressure"),
    ),
    ThermostatSensorEntityDescription(
        key="air_quality",
        translation_key="air_quality",
        device_class=SensorDeviceClass.AQI,
        state_class=SensorStateClass.MEASUREMENT,
        value_fn=nested("air_quality"),
        exists_fn=lambda c: c.has_feature("air_quality"),
    ),
    ThermostatSensorEntityDescription(
        key="gas_resistance",
        translation_key="gas_resistance",
        state_class=SensorStateClass.MEASUREMENT,
        native_unit_of_measurement="kΩ",
        entity_registry_enabled_default=False,
        value_fn=nested("gas_resistance_kohm"),
        exists_fn=lambda c: c.has_feature("air_quality"),
    ),
    ThermostatSensorEntityDescription(
        key="hydronic_supply_temperature",
        translation_key="hydronic_supply_temperature",
        device_class=SensorDeviceClass.TEMPERATURE,
        state_class=SensorStateClass.MEASUREMENT,
        device_temperature=True,
        value_fn=nested("hydronic_supply_temperature"),
        exists_fn=lambda c: c.has_feature("hydronic_supply_sensor"),
    ),
    ThermostatSensorEntityDescription(
        key="hydronic_return_temperature",
        translation_key="hydronic_return_temperature",
        device_class=SensorDeviceClass.TEMPERATURE,
        state_class=SensorStateClass.MEASUREMENT,
        device_temperature=True,
        value_fn=nested("hydronic_return_temperature"),
        exists_fn=lambda c: c.has_feature("hydronic_return_sensor"),
    ),
    ThermostatSensorEntityDescription(
        key="outdoor_temperature",
        translation_key="outdoor_temperature",
        device_class=SensorDeviceClass.TEMPERATURE,
        state_class=SensorStateClass.MEASUREMENT,
        device_temperature=True,
        value_fn=nested("weather", "temperature"),
        exists_fn=lambda c: c.has_feature("weather"),
        available_fn=_has_weather,
    ),
    ThermostatSensorEntityDescription(
        key="outdoor_condition",
        translation_key="outdoor_condition",
        value_fn=nested("weather", "condition"),
        exists_fn=lambda c: c.has_feature("weather"),
        available_fn=_has_weather,
    ),
    ThermostatSensorEntityDescription(
        key="shower_remaining",
        translation_key="shower_remaining",
        device_class=SensorDeviceClass.DURATION,
        native_unit_of_measurement=UnitOfTime.MINUTES,
        value_fn=nested("shower", "remaining_min"),
        exists_fn=lambda c: c.has_feature("shower_mode"),
    ),
    ThermostatSensorEntityDescription(
        key="active_period",
        translation_key="active_period",
        device_class=SensorDeviceClass.ENUM,
        options=["day", "night", "manual", "override"],
        value_fn=nested("schedule", "active_period"),
    ),
    ThermostatSensorEntityDescription(
        key="hvac_action",
        translation_key="hvac_action",
        device_class=SensorDeviceClass.ENUM,
        options=["off", "idle", "heating", "cooling", "fan"],
        entity_category=EntityCategory.DIAGNOSTIC,
        entity_registry_enabled_default=False,
        value_fn=nested("action"),
    ),
    ThermostatSensorEntityDescription(
        key="wifi_rssi",
        translation_key="wifi_rssi",
        device_class=SensorDeviceClass.SIGNAL_STRENGTH,
        state_class=SensorStateClass.MEASUREMENT,
        native_unit_of_measurement=SIGNAL_STRENGTH_DECIBELS_MILLIWATT,
        entity_category=EntityCategory.DIAGNOSTIC,
        entity_registry_enabled_default=False,
        value_fn=nested("wifi_rssi"),
    ),
    ThermostatSensorEntityDescription(
        key="uptime",
        translation_key="uptime",
        device_class=SensorDeviceClass.DURATION,
        state_class=SensorStateClass.TOTAL_INCREASING,
        native_unit_of_measurement=UnitOfTime.SECONDS,
        entity_category=EntityCategory.DIAGNOSTIC,
        entity_registry_enabled_default=False,
        value_fn=nested("uptime_s"),
    ),
    ThermostatSensorEntityDescription(
        key="free_heap",
        translation_key="free_heap",
        device_class=SensorDeviceClass.DATA_SIZE,
        state_class=SensorStateClass.MEASUREMENT,
        native_unit_of_measurement=UnitOfInformation.BYTES,
        entity_category=EntityCategory.DIAGNOSTIC,
        entity_registry_enabled_default=False,
        value_fn=nested("free_heap"),
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ThermostatConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up sensors."""
    coordinator = entry.runtime_data
    async_add_entities(
        ThermostatSensor(coordinator, description)
        for description in SENSORS
        if description.exists_fn(coordinator)
    )


class ThermostatSensor(ThermostatEntity, SensorEntity):
    """A thermostat sensor."""

    entity_description: ThermostatSensorEntityDescription

    def __init__(
        self,
        coordinator: ThermostatCoordinator,
        description: ThermostatSensorEntityDescription,
    ) -> None:
        """Initialize the sensor."""
        self.entity_description = description
        super().__init__(coordinator, description.key)
        if description.device_temperature:
            self._attr_native_unit_of_measurement = device_temperature_unit(
                coordinator
            )

    @property
    def available(self) -> bool:
        """Return availability."""
        return super().available and self.entity_description.available_fn(
            self.state_data
        )

    @property
    def native_value(self) -> Any:
        """Return the sensor value."""
        value = self.entity_description.value_fn(self.state_data)
        if (
            self.entity_description.device_class == SensorDeviceClass.ENUM
            and value not in (self.entity_description.options or [])
        ):
            return None
        return value

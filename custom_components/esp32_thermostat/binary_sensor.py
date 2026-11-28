# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Binary sensor platform for the ESP32 Simple Thermostat integration."""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass
from typing import Any

from homeassistant.components.binary_sensor import (
    BinarySensorDeviceClass,
    BinarySensorEntity,
    BinarySensorEntityDescription,
)
from homeassistant.const import EntityCategory
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .coordinator import ThermostatConfigEntry, ThermostatCoordinator
from .entity import ThermostatEntity, nested

PARALLEL_UPDATES = 0


@dataclass(frozen=True, kw_only=True)
class ThermostatBinarySensorEntityDescription(BinarySensorEntityDescription):
    """Describes a thermostat binary sensor."""

    value_fn: Callable[[dict[str, Any]], Any]
    exists_fn: Callable[[ThermostatCoordinator], bool] = lambda _: True


RELAYS = ("heat1", "heat2", "cool1", "cool2", "fan", "pump")

BINARY_SENSORS: tuple[ThermostatBinarySensorEntityDescription, ...] = (
    ThermostatBinarySensorEntityDescription(
        key="motion",
        translation_key="motion",
        device_class=BinarySensorDeviceClass.OCCUPANCY,
        value_fn=nested("motion"),
        exists_fn=lambda c: c.has_feature("motion_sensor"),
    ),
    ThermostatBinarySensorEntityDescription(
        key="schedule_override",
        translation_key="schedule_override",
        value_fn=nested("schedule", "override"),
    ),
    ThermostatBinarySensorEntityDescription(
        key="backup_heat_active",
        translation_key="backup_heat_active",
        device_class=BinarySensorDeviceClass.RUNNING,
        value_fn=nested("backup_heat_active"),
        exists_fn=lambda c: c.has_feature("backup_heat"),
    ),
    ThermostatBinarySensorEntityDescription(
        key="hydronic_lockout",
        translation_key="hydronic_lockout",
        device_class=BinarySensorDeviceClass.PROBLEM,
        value_fn=nested("hydronic_lockout"),
        exists_fn=lambda c: c.has_feature("hydronic_heat"),
    ),
    *(
        ThermostatBinarySensorEntityDescription(
            key=f"relay_{relay}",
            translation_key=f"relay_{relay}",
            device_class=BinarySensorDeviceClass.RUNNING,
            entity_category=EntityCategory.DIAGNOSTIC,
            entity_registry_enabled_default=False,
            value_fn=nested("relays", relay),
        )
        for relay in RELAYS
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ThermostatConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up binary sensors."""
    coordinator = entry.runtime_data
    async_add_entities(
        ThermostatBinarySensor(coordinator, description)
        for description in BINARY_SENSORS
        if description.exists_fn(coordinator)
    )


class ThermostatBinarySensor(ThermostatEntity, BinarySensorEntity):
    """A thermostat binary sensor."""

    entity_description: ThermostatBinarySensorEntityDescription

    def __init__(
        self,
        coordinator: ThermostatCoordinator,
        description: ThermostatBinarySensorEntityDescription,
    ) -> None:
        """Initialize the binary sensor."""
        self.entity_description = description
        super().__init__(coordinator, description.key)

    @property
    def is_on(self) -> bool | None:
        """Return the state."""
        value = self.entity_description.value_fn(self.state_data)
        return None if value is None else bool(value)

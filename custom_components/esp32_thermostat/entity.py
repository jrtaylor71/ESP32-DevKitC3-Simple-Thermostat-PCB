"""Base entity for the ESP32 Simple Thermostat integration."""

from __future__ import annotations

from collections.abc import Callable
from typing import Any

from homeassistant.const import UnitOfTemperature

from homeassistant.helpers.device_registry import CONNECTION_NETWORK_MAC, DeviceInfo
from homeassistant.helpers.update_coordinator import CoordinatorEntity

from .const import DOMAIN
from .coordinator import ThermostatCoordinator


def nested(*keys: str) -> Callable[[dict[str, Any]], Any]:
    """Return a getter for a (possibly nested) key path in the state dict."""

    def getter(data: dict[str, Any]) -> Any:
        value: Any = data
        for key in keys:
            if not isinstance(value, dict):
                return None
            value = value.get(key)
        return value

    return getter


def device_temperature_unit(coordinator: ThermostatCoordinator) -> UnitOfTemperature:
    """Return the device's configured temperature unit."""
    if str(coordinator.info.get("temperature_unit", "F")).upper() == "C":
        return UnitOfTemperature.CELSIUS
    return UnitOfTemperature.FAHRENHEIT


class ThermostatEntity(CoordinatorEntity[ThermostatCoordinator]):
    """Common base for all thermostat entities."""

    _attr_has_entity_name = True

    def __init__(self, coordinator: ThermostatCoordinator, key: str) -> None:
        """Initialize the entity."""
        super().__init__(coordinator)
        info = coordinator.info
        self._attr_unique_id = f"{coordinator.device_id}_{key}"
        connections = set()
        if mac := info.get("mac"):
            connections.add((CONNECTION_NETWORK_MAC, str(mac).lower()))
        self._attr_device_info = DeviceInfo(
            identifiers={(DOMAIN, coordinator.device_id)},
            connections=connections,
            name=info.get("hostname"),
            manufacturer=info.get("manufacturer"),
            model=info.get("model"),
            sw_version=info.get("firmware_version"),
            configuration_url=f"http://{coordinator.api.host}",
        )

    @property
    def state_data(self) -> dict[str, Any]:
        """Return the current device state (never None)."""
        return self.coordinator.data or {}

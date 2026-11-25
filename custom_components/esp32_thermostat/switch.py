"""Switch platform for the ESP32 Simple Thermostat integration."""

from __future__ import annotations

from collections.abc import Awaitable, Callable
from dataclasses import dataclass
from typing import Any

from homeassistant.components.switch import SwitchEntity, SwitchEntityDescription
from homeassistant.const import EntityCategory
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import DAYS
from .coordinator import ThermostatConfigEntry, ThermostatCoordinator
from .entity import ThermostatEntity, nested

PARALLEL_UPDATES = 1


@dataclass(frozen=True, kw_only=True)
class ThermostatSwitchEntityDescription(SwitchEntityDescription):
    """Describes a thermostat switch."""

    value_fn: Callable[[dict[str, Any]], Any]
    set_fn: Callable[[ThermostatCoordinator, bool], Awaitable[None]]
    exists_fn: Callable[[ThermostatCoordinator], bool] = lambda _: True


def _day_enabled(index: int) -> Callable[[dict[str, Any]], Any]:
    def getter(data: dict[str, Any]) -> Any:
        days = nested("schedule", "days_enabled")(data)
        if isinstance(days, list) and index < len(days):
            return days[index]
        return None

    return getter


def _set_day(day: str) -> Callable[[ThermostatCoordinator, bool], Awaitable[None]]:
    def setter(coordinator: ThermostatCoordinator, value: bool) -> Awaitable[None]:
        return coordinator.async_put_schedule(
            {"days": [{"day": day, "enabled": value}]}
        )

    return setter


def _setting(key: str) -> ThermostatSwitchEntityDescription:
    """Describe a boolean device setting written through POST /settings."""
    return ThermostatSwitchEntityDescription(
        key=key,
        translation_key=key,
        entity_category=EntityCategory.CONFIG,
        value_fn=nested("settings", key),
        set_fn=lambda c, v: c.async_send_settings(**{key: v}),
    )


SETTING_SWITCHES = (
    "stage2_heat_enabled",
    "stage2_cool_enabled",
    "heat_pump_enabled",
    "fan_with_heat",
    "fan_with_cool",
    "backup_heat_enabled",
    "hydronic_heat_enabled",
    "eu_humidity_enabled",
    "shower_mode_enabled",
)

SWITCHES: tuple[ThermostatSwitchEntityDescription, ...] = (
    ThermostatSwitchEntityDescription(
        key="schedule_enabled",
        translation_key="schedule_enabled",
        value_fn=nested("schedule", "enabled"),
        set_fn=lambda c, v: c.async_send_command(schedule_enabled=v),
    ),
    ThermostatSwitchEntityDescription(
        key="shower_mode",
        translation_key="shower_mode",
        value_fn=nested("shower", "active"),
        set_fn=lambda c, v: c.async_send_command(shower_mode=v),
        exists_fn=lambda c: c.has_feature("shower_mode"),
    ),
    *(
        ThermostatSwitchEntityDescription(
            key=f"schedule_{day}",
            translation_key=f"schedule_{day}",
            entity_category=EntityCategory.CONFIG,
            value_fn=_day_enabled(index),
            set_fn=_set_day(day),
        )
        for index, day in enumerate(DAYS)
    ),
    *(_setting(key) for key in SETTING_SWITCHES),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ThermostatConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up switches."""
    coordinator = entry.runtime_data
    async_add_entities(
        ThermostatSwitch(coordinator, description)
        for description in SWITCHES
        if description.exists_fn(coordinator)
    )


class ThermostatSwitch(ThermostatEntity, SwitchEntity):
    """A thermostat switch."""

    entity_description: ThermostatSwitchEntityDescription

    def __init__(
        self,
        coordinator: ThermostatCoordinator,
        description: ThermostatSwitchEntityDescription,
    ) -> None:
        """Initialize the switch."""
        self.entity_description = description
        super().__init__(coordinator, description.key)

    @property
    def is_on(self) -> bool | None:
        """Return the state."""
        value = self.entity_description.value_fn(self.state_data)
        return None if value is None else bool(value)

    async def async_turn_on(self, **kwargs: Any) -> None:
        """Turn on."""
        await self.entity_description.set_fn(self.coordinator, True)

    async def async_turn_off(self, **kwargs: Any) -> None:
        """Turn off."""
        await self.entity_description.set_fn(self.coordinator, False)

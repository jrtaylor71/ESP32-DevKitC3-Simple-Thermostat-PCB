"""Button platform for the ESP32 Simple Thermostat integration."""

from __future__ import annotations

from collections.abc import Awaitable, Callable
from dataclasses import dataclass
from typing import Any

from homeassistant.components.button import (
    ButtonDeviceClass,
    ButtonEntity,
    ButtonEntityDescription,
)
from homeassistant.const import EntityCategory
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import OVERRIDE_RESUME
from .coordinator import ThermostatConfigEntry, ThermostatCoordinator
from .entity import ThermostatEntity, nested

PARALLEL_UPDATES = 1


@dataclass(frozen=True, kw_only=True)
class ThermostatButtonEntityDescription(ButtonEntityDescription):
    """Describes a thermostat button."""

    press_fn: Callable[[ThermostatCoordinator], Awaitable[None]]
    exists_fn: Callable[[ThermostatCoordinator], bool] = lambda _: True
    available_fn: Callable[[dict[str, Any]], bool] = lambda _: True


BUTTONS: tuple[ThermostatButtonEntityDescription, ...] = (
    ThermostatButtonEntityDescription(
        key="reboot",
        translation_key="reboot",
        device_class=ButtonDeviceClass.RESTART,
        entity_category=EntityCategory.CONFIG,
        press_fn=lambda c: c.async_send_action("reboot"),
    ),
    ThermostatButtonEntityDescription(
        key="refresh_weather",
        translation_key="refresh_weather",
        press_fn=lambda c: c.async_send_action("refresh_weather"),
        exists_fn=lambda c: c.has_feature("weather"),
    ),
    ThermostatButtonEntityDescription(
        key="cancel_override",
        translation_key="cancel_override",
        press_fn=lambda c: c.async_send_command(schedule_override=OVERRIDE_RESUME),
        available_fn=lambda data: bool(nested("schedule", "enabled")(data)),
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ThermostatConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up buttons."""
    coordinator = entry.runtime_data
    async_add_entities(
        ThermostatButton(coordinator, description)
        for description in BUTTONS
        if description.exists_fn(coordinator)
    )


class ThermostatButton(ThermostatEntity, ButtonEntity):
    """A thermostat button."""

    entity_description: ThermostatButtonEntityDescription

    def __init__(
        self,
        coordinator: ThermostatCoordinator,
        description: ThermostatButtonEntityDescription,
    ) -> None:
        """Initialize the button."""
        self.entity_description = description
        super().__init__(coordinator, description.key)

    @property
    def available(self) -> bool:
        """Return availability."""
        return super().available and self.entity_description.available_fn(
            self.state_data
        )

    async def async_press(self) -> None:
        """Press the button."""
        await self.entity_description.press_fn(self.coordinator)

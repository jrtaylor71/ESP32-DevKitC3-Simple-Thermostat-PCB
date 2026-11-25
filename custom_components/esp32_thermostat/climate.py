"""Climate platform for the ESP32 Simple Thermostat integration."""

from __future__ import annotations

from typing import Any

from homeassistant.components.climate import (
    ATTR_HVAC_MODE,
    ClimateEntity,
    ClimateEntityFeature,
    HVACAction,
    HVACMode,
)
from homeassistant.const import ATTR_TEMPERATURE, PRECISION_TENTHS
from homeassistant.core import HomeAssistant, callback
from homeassistant.exceptions import ServiceValidationError
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import (
    DOMAIN,
    OVERRIDE_PERMANENT,
    OVERRIDE_RESUME,
    OVERRIDE_TEMPORARY,
    PRESET_NONE,
    PRESET_PERMANENT_HOLD,
    PRESET_SCHEDULE,
    PRESET_TEMPORARY_HOLD,
)
from .coordinator import ThermostatConfigEntry, ThermostatCoordinator
from .entity import ThermostatEntity, device_temperature_unit

PARALLEL_UPDATES = 0

DEVICE_TO_HVAC_MODE: dict[str, HVACMode] = {
    "off": HVACMode.OFF,
    "heat": HVACMode.HEAT,
    "cool": HVACMode.COOL,
    "auto": HVACMode.AUTO,
}
HVAC_MODE_TO_DEVICE = {value: key for key, value in DEVICE_TO_HVAC_MODE.items()}

DEVICE_TO_HVAC_ACTION: dict[str, HVACAction] = {
    "off": HVACAction.OFF,
    "idle": HVACAction.IDLE,
    "heating": HVACAction.HEATING,
    "cooling": HVACAction.COOLING,
    "fan": HVACAction.FAN,
}

FAN_MODES = ["auto", "on", "cycle"]

PRESET_COMMANDS: dict[str, dict[str, Any]] = {
    PRESET_NONE: {"schedule_enabled": False},
    PRESET_SCHEDULE: {"schedule_enabled": True, "schedule_override": OVERRIDE_RESUME},
    PRESET_TEMPORARY_HOLD: {
        "schedule_enabled": True,
        "schedule_override": OVERRIDE_TEMPORARY,
    },
    PRESET_PERMANENT_HOLD: {
        "schedule_enabled": True,
        "schedule_override": OVERRIDE_PERMANENT,
    },
}


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ThermostatConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the climate entity."""
    async_add_entities([ThermostatClimate(entry.runtime_data)])


class ThermostatClimate(ThermostatEntity, ClimateEntity):
    """The thermostat itself."""

    _attr_name = None
    _attr_translation_key = "thermostat"
    _attr_hvac_modes = list(DEVICE_TO_HVAC_MODE.values())
    _attr_fan_modes = FAN_MODES
    _attr_preset_modes = list(PRESET_COMMANDS)
    _attr_precision = PRECISION_TENTHS
    _attr_supported_features = (
        ClimateEntityFeature.TARGET_TEMPERATURE
        | ClimateEntityFeature.FAN_MODE
        | ClimateEntityFeature.PRESET_MODE
        | ClimateEntityFeature.TURN_ON
        | ClimateEntityFeature.TURN_OFF
    )
    _enable_turn_on_off_backwards_compatibility = False
    _unrecorded_attributes = frozenset({"override_remaining_min"})

    def __init__(self, coordinator: ThermostatCoordinator) -> None:
        """Initialize the climate entity."""
        super().__init__(coordinator, "climate")
        info = coordinator.info
        self._attr_temperature_unit = device_temperature_unit(coordinator)
        if (min_temp := info.get("min_temp")) is not None:
            self._attr_min_temp = float(min_temp)
        if (max_temp := info.get("max_temp")) is not None:
            self._attr_max_temp = float(max_temp)
        if (step := info.get("temp_step")) is not None:
            self._attr_target_temperature_step = float(step)
        self._last_on_mode: str | None = None
        self._remember_mode()

    def _remember_mode(self) -> None:
        mode = self.state_data.get("mode")
        if mode in DEVICE_TO_HVAC_MODE and mode != "off":
            self._last_on_mode = mode

    @callback
    def _handle_coordinator_update(self) -> None:
        self._remember_mode()
        super()._handle_coordinator_update()

    @property
    def hvac_mode(self) -> HVACMode | None:
        """Return the current HVAC mode."""
        return DEVICE_TO_HVAC_MODE.get(self.state_data.get("mode"))

    @property
    def hvac_action(self) -> HVACAction | None:
        """Return the current HVAC action."""
        return DEVICE_TO_HVAC_ACTION.get(self.state_data.get("action"))

    @property
    def current_temperature(self) -> float | None:
        """Return the current temperature."""
        return self.state_data.get("current_temperature")

    @property
    def current_humidity(self) -> float | None:
        """Return the current humidity."""
        return self.state_data.get("current_humidity")

    @property
    def target_temperature(self) -> float | None:
        """Return the setpoint for the current mode."""
        return self.state_data.get("target_temperature")

    @property
    def fan_mode(self) -> str | None:
        """Return the fan mode."""
        return self.state_data.get("fan_mode")

    @property
    def preset_mode(self) -> str | None:
        """Return the preset derived from the schedule state."""
        schedule = self.state_data.get("schedule") or {}
        if not schedule.get("enabled"):
            return PRESET_NONE
        override_type = schedule.get("override_type")
        if override_type == OVERRIDE_TEMPORARY:
            return PRESET_TEMPORARY_HOLD
        if override_type == OVERRIDE_PERMANENT:
            return PRESET_PERMANENT_HOLD
        return PRESET_SCHEDULE

    @property
    def extra_state_attributes(self) -> dict[str, Any]:
        """Return stage and schedule details (used by the Lovelace card)."""
        data = self.state_data
        schedule = data.get("schedule") or {}
        return {
            "stage1_active": data.get("stage1_active"),
            "stage2_active": data.get("stage2_active"),
            "backup_heat_active": data.get("backup_heat_active"),
            "schedule_enabled": schedule.get("enabled"),
            "active_period": schedule.get("active_period"),
            "override_type": schedule.get("override_type"),
            "override_remaining_min": schedule.get("override_remaining_min"),
        }

    async def async_set_temperature(self, **kwargs: Any) -> None:
        """Set the target temperature (optionally with an HVAC mode)."""
        fields: dict[str, Any] = {}
        mode = self.state_data.get("mode")
        if (hvac_mode := kwargs.get(ATTR_HVAC_MODE)) is not None:
            mode = HVAC_MODE_TO_DEVICE[HVACMode(hvac_mode)]
            fields["mode"] = mode
        if (temperature := kwargs.get(ATTR_TEMPERATURE)) is not None:
            if mode == "off":
                raise ServiceValidationError(
                    translation_domain=DOMAIN, translation_key="temperature_mode_off"
                )
            fields["target_temperature"] = float(temperature)
        if fields:
            await self.coordinator.async_send_command(**fields)

    async def async_set_hvac_mode(self, hvac_mode: HVACMode) -> None:
        """Set the HVAC mode."""
        await self.coordinator.async_send_command(mode=HVAC_MODE_TO_DEVICE[hvac_mode])

    async def async_set_fan_mode(self, fan_mode: str) -> None:
        """Set the fan mode."""
        await self.coordinator.async_send_command(fan_mode=fan_mode)

    async def async_set_preset_mode(self, preset_mode: str) -> None:
        """Set the schedule/hold preset."""
        await self.coordinator.async_send_command(**PRESET_COMMANDS[preset_mode])

    async def async_turn_on(self) -> None:
        """Turn on using the last non-off mode (default auto)."""
        await self.coordinator.async_send_command(mode=self._last_on_mode or "auto")

    async def async_turn_off(self) -> None:
        """Turn off."""
        await self.coordinator.async_send_command(mode="off")

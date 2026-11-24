"""Data coordinator for the ESP32 Simple Thermostat integration."""

from __future__ import annotations

import asyncio
from collections.abc import Awaitable, Callable
import copy
import logging
import time
from typing import Any, TypeVar

from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.exceptions import ConfigEntryAuthFailed, HomeAssistantError
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator, UpdateFailed

from .api import (
    ThermostatApiClient,
    ThermostatAuthError,
    ThermostatCommandError,
    ThermostatError,
)
from .const import (
    DAYS,
    DOMAIN,
    SSE_BACKOFF_INITIAL,
    SSE_BACKOFF_MAX,
    SSE_STALE_SECONDS,
    UPDATE_INTERVAL,
)

_LOGGER = logging.getLogger(__name__)

_T = TypeVar("_T")

type ThermostatConfigEntry = ConfigEntry[ThermostatCoordinator]


class ThermostatCoordinator(DataUpdateCoordinator[dict[str, Any]]):
    """Keeps thermostat state up to date via SSE with polling fallback."""

    config_entry: ThermostatConfigEntry

    def __init__(
        self,
        hass: HomeAssistant,
        entry: ThermostatConfigEntry,
        api: ThermostatApiClient,
        info: dict[str, Any],
    ) -> None:
        """Initialize the coordinator."""
        super().__init__(
            hass,
            _LOGGER,
            config_entry=entry,
            name=f"{DOMAIN} {info.get('hostname', api.host)}",
            update_interval=UPDATE_INTERVAL,
        )
        self.api = api
        self.info = info
        self.sse_connected = False
        self.last_event: float | None = None
        self._sse_task: asyncio.Task[None] | None = None

    @property
    def device_id(self) -> str:
        """Return the device id (lowercase MAC without colons)."""
        return str(self.info["device_id"])

    @property
    def features(self) -> dict[str, Any]:
        """Return the feature flags from /info."""
        return self.info.get("features") or {}

    def has_feature(self, *names: str) -> bool:
        """Return True if any of the named features is enabled."""
        return any(bool(self.features.get(name)) for name in names)

    def _sse_fresh(self) -> bool:
        return (
            self.sse_connected
            and self.last_event is not None
            and time.monotonic() - self.last_event <= SSE_STALE_SECONDS
        )

    async def _async_update_data(self) -> dict[str, Any]:
        """Poll /state only if the event stream is not delivering."""
        if self.data is not None and self._sse_fresh():
            return self.data
        try:
            return await self.api.async_get_state()
        except ThermostatAuthError as err:
            raise ConfigEntryAuthFailed(str(err)) from err
        except ThermostatError as err:
            raise UpdateFailed(f"Error fetching thermostat state: {err}") from err

    # ---------------------------------------------------------------- SSE
    def async_start_event_stream(self) -> None:
        """Start the background SSE listener."""
        if self._sse_task is None or self._sse_task.done():
            self._sse_task = self.config_entry.async_create_background_task(
                self.hass,
                self._async_event_loop(),
                f"{DOMAIN}_events_{self.device_id}",
            )

    async def async_stop_event_stream(self) -> None:
        """Stop the background SSE listener."""
        task, self._sse_task = self._sse_task, None
        self.sse_connected = False
        if task is not None and not task.done():
            task.cancel()
            try:
                await task
            except asyncio.CancelledError:
                pass

    async def _async_event_loop(self) -> None:
        backoff = SSE_BACKOFF_INITIAL
        while True:
            try:
                async for event in self.api.async_events():
                    self.last_event = time.monotonic()
                    if event.event == "state":
                        self.sse_connected = True
                        backoff = SSE_BACKOFF_INITIAL
                        self.async_set_updated_data(event.data)
                    elif event.event == "update":
                        if self.data is None:
                            continue
                        self.async_set_updated_data({**self.data, **event.data})
                _LOGGER.debug("Event stream from %s closed", self.api.host)
            except ThermostatAuthError as err:
                self.sse_connected = False
                _LOGGER.warning(
                    "Event stream from %s rejected credentials: %s", self.api.host, err
                )
                self.config_entry.async_start_reauth(self.hass)
                return
            except ThermostatError as err:
                _LOGGER.debug("Event stream from %s failed: %s", self.api.host, err)
            except asyncio.CancelledError:
                self.sse_connected = False
                raise
            except Exception:
                _LOGGER.exception("Unexpected error in event stream from %s", self.api.host)
            self.sse_connected = False
            await asyncio.sleep(backoff)
            backoff = min(backoff * 2, SSE_BACKOFF_MAX)

    # ----------------------------------------------------------- Commands
    async def _async_call(self, func: Callable[[], Awaitable[_T]]) -> _T:
        try:
            return await func()
        except ThermostatCommandError as err:
            raise HomeAssistantError(
                translation_domain=DOMAIN,
                translation_key="command_rejected",
                translation_placeholders={
                    "field": err.field or "-",
                    "message": err.message,
                },
            ) from err
        except ThermostatAuthError as err:
            self.config_entry.async_start_reauth(self.hass)
            raise HomeAssistantError(
                translation_domain=DOMAIN,
                translation_key="auth_failed",
            ) from err
        except ThermostatError as err:
            raise HomeAssistantError(
                translation_domain=DOMAIN,
                translation_key="communication_error",
                translation_placeholders={"error": str(err)},
            ) from err

    async def _async_after_write(self) -> None:
        # The device pushes changes over SSE; poll only if the stream is down.
        if not self._sse_fresh():
            await self.async_request_refresh()

    async def async_send_command(self, **fields: Any) -> None:
        """Send POST /command."""
        await self._async_call(lambda: self.api.async_command(**fields))
        await self._async_after_write()

    async def async_send_settings(self, **fields: Any) -> None:
        """Send POST /settings."""
        await self._async_call(lambda: self.api.async_settings(**fields))
        await self._async_after_write()

    async def async_send_action(self, name: str) -> None:
        """Send POST /action."""
        await self._async_call(lambda: self.api.async_action(name))

    async def async_get_schedule(self) -> dict[str, Any]:
        """Return GET /schedule."""
        return await self._async_call(self.api.async_get_schedule)

    async def async_put_schedule(self, payload: dict[str, Any]) -> None:
        """Send PUT /schedule and optimistically update local schedule state."""
        await self._async_call(lambda: self.api.async_put_schedule(payload))
        if self.data is None:
            return
        data = copy.deepcopy(self.data)
        schedule = data.get("schedule")
        if not isinstance(schedule, dict):
            schedule = data["schedule"] = {}
        if "enabled" in payload:
            schedule["enabled"] = bool(payload["enabled"])
        days_enabled = list(schedule.get("days_enabled") or [True] * len(DAYS))
        for day in payload.get("days") or []:
            if "enabled" not in day:
                continue
            index = day.get("index")
            if day.get("day") in DAYS:
                index = DAYS.index(day["day"])
            if isinstance(index, int) and 0 <= index < len(days_enabled):
                days_enabled[index] = bool(day["enabled"])
        schedule["days_enabled"] = days_enabled
        self.async_set_updated_data(data)

"""Async client for the ESP32 Simple Thermostat local API (v1)."""

from __future__ import annotations

import asyncio
from collections.abc import AsyncIterator
from dataclasses import dataclass
import json
from typing import Any

import aiohttp

from .const import API_BASE_PATH, REQUEST_TIMEOUT, SSE_SOCK_READ_TIMEOUT


class ThermostatError(Exception):
    """Base error for the thermostat API."""


class ThermostatConnectionError(ThermostatError):
    """The device could not be reached or returned an unexpected response."""


class ThermostatAuthError(ThermostatError):
    """The token was rejected (401)."""


class ThermostatApiDisabledError(ThermostatAuthError):
    """The local API is disabled in the device settings (403 api_disabled)."""


class ThermostatCommandError(ThermostatError):
    """The device rejected a request (400 invalid / 409 conflict)."""

    def __init__(
        self, status: int, error: str | None, field: str | None, message: str | None
    ) -> None:
        """Initialize the error."""
        self.status = status
        self.error = error
        self.field = field
        self.message = message or error or f"HTTP {status}"
        super().__init__(
            f"{self.field}: {self.message}" if self.field else self.message
        )


@dataclass(slots=True, frozen=True)
class ThermostatEvent:
    """A single Server-Sent Event."""

    event: str
    data: dict[str, Any]


def _format_host(host: str) -> str:
    host = host.strip().rstrip("/")
    for prefix in ("http://", "https://"):
        if host.startswith(prefix):
            host = host[len(prefix) :]
    # Bare IPv6 literal
    if host.count(":") > 1 and not host.startswith("["):
        host = f"[{host}]"
    return host


class ThermostatApiClient:
    """Client for the thermostat local HTTP API."""

    def __init__(self, host: str, token: str, session: aiohttp.ClientSession) -> None:
        """Initialize the client."""
        self._host = host
        self._token = token
        self._session = session
        self._base_url = f"http://{_format_host(host)}{API_BASE_PATH}"

    @property
    def host(self) -> str:
        """Return the configured host."""
        return self._host

    @property
    def _headers(self) -> dict[str, str]:
        return {
            "Authorization": f"Bearer {self._token}",
            "Accept": "application/json",
        }

    @staticmethod
    async def _read_json(resp: aiohttp.ClientResponse) -> Any:
        try:
            return await resp.json(content_type=None)
        except (ValueError, aiohttp.ContentTypeError):
            return None

    @classmethod
    async def _raise_for_status(cls, resp: aiohttp.ClientResponse) -> None:
        if resp.status < 400:
            return
        body = await cls._read_json(resp)
        if not isinstance(body, dict):
            body = {}
        error = body.get("error")
        if resp.status == 401:
            raise ThermostatAuthError(error or "unauthorized")
        if resp.status == 403:
            if error == "api_disabled":
                raise ThermostatApiDisabledError(error)
            raise ThermostatAuthError(error or "forbidden")
        if resp.status in (400, 409):
            raise ThermostatCommandError(
                resp.status, error, body.get("field"), body.get("message")
            )
        raise ThermostatConnectionError(
            f"Unexpected HTTP {resp.status} from device: {error or resp.reason}"
        )

    async def _request(
        self, method: str, path: str, payload: dict[str, Any] | None = None
    ) -> Any:
        try:
            async with self._session.request(
                method,
                f"{self._base_url}{path}",
                headers=self._headers,
                json=payload,
                timeout=aiohttp.ClientTimeout(total=REQUEST_TIMEOUT),
            ) as resp:
                await self._raise_for_status(resp)
                return await self._read_json(resp)
        except ThermostatError:
            raise
        except (TimeoutError, asyncio.TimeoutError) as err:
            raise ThermostatConnectionError(
                f"Timeout talking to {self._host}"
            ) from err
        except aiohttp.ClientError as err:
            raise ThermostatConnectionError(
                f"Error talking to {self._host}: {err}"
            ) from err

    async def _get_dict(self, path: str) -> dict[str, Any]:
        data = await self._request("GET", path)
        if not isinstance(data, dict):
            raise ThermostatConnectionError(f"Invalid response for {path}")
        return data

    async def async_get_info(self) -> dict[str, Any]:
        """Return GET /info."""
        return await self._get_dict("/info")

    async def async_get_state(self) -> dict[str, Any]:
        """Return GET /state."""
        return await self._get_dict("/state")

    async def async_get_schedule(self) -> dict[str, Any]:
        """Return GET /schedule."""
        return await self._get_dict("/schedule")

    async def async_put_schedule(self, payload: dict[str, Any]) -> Any:
        """PUT /schedule (partial update)."""
        return await self._request("PUT", "/schedule", payload)

    async def async_command(self, **fields: Any) -> Any:
        """POST /command."""
        return await self._request("POST", "/command", fields)

    async def async_settings(self, **fields: Any) -> Any:
        """POST /settings."""
        return await self._request("POST", "/settings", fields)

    async def async_action(self, name: str) -> Any:
        """POST /action."""
        return await self._request("POST", "/action", {"action": name})

    async def async_events(self) -> AsyncIterator[ThermostatEvent]:
        """Connect to /events and yield parsed Server-Sent Events.

        The generator ends when the server closes the stream. Read inactivity
        longer than the socket read timeout raises ThermostatConnectionError.
        """
        # The firmware only routes to the SSE handler with this Accept header.
        headers = {
            **self._headers,
            "Accept": "text/event-stream",
            "Cache-Control": "no-cache",
        }
        try:
            async with self._session.get(
                f"{self._base_url}/events",
                headers=headers,
                timeout=aiohttp.ClientTimeout(
                    total=None,
                    connect=REQUEST_TIMEOUT,
                    sock_connect=REQUEST_TIMEOUT,
                    sock_read=SSE_SOCK_READ_TIMEOUT,
                ),
            ) as resp:
                await self._raise_for_status(resp)
                parser = SSEParser()
                async for raw in resp.content:
                    event = parser.feed_line(raw.decode("utf-8", errors="replace"))
                    if event is not None:
                        yield event
        except ThermostatError:
            raise
        except (TimeoutError, asyncio.TimeoutError) as err:
            raise ThermostatConnectionError(
                f"Event stream from {self._host} timed out"
            ) from err
        except aiohttp.ClientError as err:
            raise ThermostatConnectionError(
                f"Event stream error from {self._host}: {err}"
            ) from err


class SSEParser:
    """Incremental line-based Server-Sent Events parser."""

    def __init__(self) -> None:
        """Initialize the parser."""
        self._event = ""
        self._data: list[str] = []

    def feed_line(self, line: str) -> ThermostatEvent | None:
        """Feed one line; return an event when a blank line dispatches one."""
        line = line.rstrip("\r\n")
        if not line:
            return self._dispatch()
        if line.startswith(":"):
            return None
        field, sep, value = line.partition(":")
        if sep and value.startswith(" "):
            value = value[1:]
        if field == "event":
            self._event = value
        elif field == "data":
            self._data.append(value)
        return None

    def _dispatch(self) -> ThermostatEvent | None:
        event = self._event or "message"
        raw = "\n".join(self._data)
        has_data = bool(self._data)
        self._event = ""
        self._data = []
        if not has_data:
            return None
        try:
            data = json.loads(raw) if raw.strip() else {}
        except ValueError:
            return None
        if not isinstance(data, dict):
            data = {"value": data}
        return ThermostatEvent(event, data)

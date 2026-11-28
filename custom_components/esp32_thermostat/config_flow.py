# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Config flow for the ESP32 Simple Thermostat integration."""

from __future__ import annotations

from collections.abc import Mapping
import logging
from typing import TYPE_CHECKING, Any

import voluptuous as vol

from homeassistant.config_entries import ConfigFlow, ConfigFlowResult
from homeassistant.const import CONF_HOST
from homeassistant.helpers.aiohttp_client import async_get_clientsession

if TYPE_CHECKING:
    from homeassistant.helpers.service_info.zeroconf import ZeroconfServiceInfo

from .api import (
    ThermostatApiClient,
    ThermostatApiDisabledError,
    ThermostatAuthError,
    ThermostatConnectionError,
)
from .const import CONF_TOKEN, DOMAIN

_LOGGER = logging.getLogger(__name__)

TOKEN_SCHEMA = vol.Schema({vol.Required(CONF_TOKEN): str})


class ThermostatConfigFlow(ConfigFlow, domain=DOMAIN):
    """Handle a config flow for the thermostat."""

    VERSION = 1

    def __init__(self) -> None:
        """Initialize the flow."""
        self._host: str | None = None
        self._name: str | None = None

    async def _async_validate(
        self, host: str, token: str
    ) -> tuple[dict[str, Any] | None, dict[str, str]]:
        api = ThermostatApiClient(
            host, token.strip(), async_get_clientsession(self.hass)
        )
        try:
            info = await api.async_get_info()
        except ThermostatApiDisabledError:
            return None, {"base": "api_disabled"}
        except ThermostatAuthError:
            return None, {"base": "invalid_auth"}
        except ThermostatConnectionError:
            return None, {"base": "cannot_connect"}
        except Exception:
            _LOGGER.exception("Unexpected error validating thermostat")
            return None, {"base": "unknown"}
        if not info.get("device_id"):
            return None, {"base": "unknown"}
        return info, {}

    async def async_step_user(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """Handle manual setup."""
        errors: dict[str, str] = {}
        if user_input is not None:
            host = user_input[CONF_HOST].strip()
            token = user_input[CONF_TOKEN].strip()
            info, errors = await self._async_validate(host, token)
            if info is not None:
                await self.async_set_unique_id(str(info["device_id"]))
                self._abort_if_unique_id_configured(updates={CONF_HOST: host})
                return self.async_create_entry(
                    title=str(info.get("hostname") or host),
                    data={CONF_HOST: host, CONF_TOKEN: token},
                )
        return self.async_show_form(
            step_id="user",
            data_schema=self.add_suggested_values_to_schema(
                vol.Schema(
                    {vol.Required(CONF_HOST): str, vol.Required(CONF_TOKEN): str}
                ),
                user_input,
            ),
            errors=errors,
        )

    async def async_step_zeroconf(
        self, discovery_info: ZeroconfServiceInfo
    ) -> ConfigFlowResult:
        """Handle zeroconf discovery."""
        if discovery_info.ip_address.version == 6:
            return self.async_abort(reason="not_ipv4_address")
        properties = discovery_info.properties
        device_id = properties.get("id")
        if not device_id:
            return self.async_abort(reason="invalid_discovery_info")
        if str(properties.get("api", "1")) != "1":
            return self.async_abort(reason="unsupported_api_version")

        host = discovery_info.host
        await self.async_set_unique_id(str(device_id).lower())
        self._abort_if_unique_id_configured(updates={CONF_HOST: host})

        self._host = host
        self._name = str(properties.get("name") or discovery_info.hostname or host)
        self._name = self._name.removesuffix(".").removesuffix(".local")
        self.context["title_placeholders"] = {"name": self._name}
        return await self.async_step_zeroconf_confirm()

    async def async_step_zeroconf_confirm(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """Ask for the token of a discovered thermostat."""
        assert self._host is not None
        errors: dict[str, str] = {}
        if user_input is not None:
            token = user_input[CONF_TOKEN].strip()
            info, errors = await self._async_validate(self._host, token)
            if info is not None:
                if str(info["device_id"]) != self.unique_id:
                    return self.async_abort(reason="wrong_device")
                return self.async_create_entry(
                    title=str(info.get("hostname") or self._name),
                    data={CONF_HOST: self._host, CONF_TOKEN: token},
                )
        return self.async_show_form(
            step_id="zeroconf_confirm",
            data_schema=TOKEN_SCHEMA,
            description_placeholders={"name": self._name or "", "host": self._host},
            errors=errors,
        )

    async def async_step_reauth(
        self, entry_data: Mapping[str, Any]
    ) -> ConfigFlowResult:
        """Handle a rejected token."""
        return await self.async_step_reauth_confirm()

    async def async_step_reauth_confirm(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """Ask for a new token."""
        entry = self._get_reauth_entry()
        errors: dict[str, str] = {}
        if user_input is not None:
            token = user_input[CONF_TOKEN].strip()
            info, errors = await self._async_validate(entry.data[CONF_HOST], token)
            if info is not None:
                if str(info["device_id"]) != entry.unique_id:
                    return self.async_abort(reason="wrong_device")
                return self.async_update_reload_and_abort(
                    entry, data_updates={CONF_TOKEN: token}
                )
        return self.async_show_form(
            step_id="reauth_confirm",
            data_schema=TOKEN_SCHEMA,
            description_placeholders={"name": entry.title},
            errors=errors,
        )

    async def async_step_reconfigure(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """Change the host of an existing entry."""
        entry = self._get_reconfigure_entry()
        errors: dict[str, str] = {}
        if user_input is not None:
            host = user_input[CONF_HOST].strip()
            info, errors = await self._async_validate(host, entry.data[CONF_TOKEN])
            if info is not None:
                if str(info["device_id"]) != entry.unique_id:
                    return self.async_abort(reason="wrong_device")
                return self.async_update_reload_and_abort(
                    entry, data_updates={CONF_HOST: host}
                )
        return self.async_show_form(
            step_id="reconfigure",
            data_schema=self.add_suggested_values_to_schema(
                vol.Schema({vol.Required(CONF_HOST): str}),
                user_input or {CONF_HOST: entry.data[CONF_HOST]},
            ),
            description_placeholders={"name": entry.title},
            errors=errors,
        )

# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Serve and register the bundled Lovelace card."""

from __future__ import annotations

import logging
from collections.abc import Mapping
from pathlib import Path
from typing import Any
from urllib.parse import urlsplit

from homeassistant.components.http import StaticPathConfig
from homeassistant.const import CONF_TYPE, CONF_URL
from homeassistant.core import HomeAssistant
from homeassistant.loader import async_get_integration

from .const import CARD_FILENAME, CARD_URL, DOMAIN

_LOGGER = logging.getLogger(__name__)

DATA_CARD_REGISTERED = f"{DOMAIN}_card_registered"
CARD_RESOURCE_TYPE = "module"


def _is_card_resource(item: Mapping[str, Any]) -> bool:
    """Return whether a Lovelace resource points at this card."""
    try:
        return urlsplit(item.get(CONF_URL, "")).path == CARD_URL
    except ValueError:
        return False


async def async_register_card(hass: HomeAssistant) -> None:
    """Serve and register the card JS as a Lovelace resource."""
    if hass.data.get(DATA_CARD_REGISTERED):
        return

    path = Path(__file__).parent / "frontend" / CARD_FILENAME
    try:
        await hass.http.async_register_static_paths(
            [StaticPathConfig(CARD_URL, str(path), cache_headers=False)]
        )
    except RuntimeError:
        # Route already registered (e.g. integration set up again in the same run).
        _LOGGER.debug("Static path %s already registered", CARD_URL)

    lovelace = hass.data.get("lovelace")
    if lovelace is None:
        raise RuntimeError(
            "Lovelace must be loaded before registering the thermostat card"
        )

    if isinstance(lovelace, dict):
        resource_mode = lovelace.get("resource_mode", lovelace.get("mode"))
        resources = lovelace.get("resources")
    else:
        resource_mode = getattr(lovelace, "resource_mode", None)
        resources = getattr(lovelace, "resources", None)

    if resource_mode != "storage" or resources is None:
        _LOGGER.warning(
            "Cannot automatically register the thermostat card when Lovelace "
            "resources use YAML mode; add %s as a JavaScript module in YAML",
            CARD_URL,
        )
        return

    integration = await async_get_integration(hass, DOMAIN)
    resource_url = f"{CARD_URL}?v={integration.version}"

    # Ensure storage resources are loaded before inspecting the collection.
    await resources.async_get_info()
    existing = [
        item
        for item in resources.async_items()
        if _is_card_resource(item)
    ]

    if existing:
        for item in existing:
            updates: dict[str, str] = {}
            if item.get(CONF_URL) != resource_url:
                updates[CONF_URL] = resource_url
            if item.get(CONF_TYPE) != CARD_RESOURCE_TYPE:
                updates["res_type"] = CARD_RESOURCE_TYPE
            if updates:
                await resources.async_update_item(item["id"], updates)
        _LOGGER.debug("Updated existing Lovelace resource for %s", DOMAIN)
    else:
        await resources.async_create_item(
            {"res_type": CARD_RESOURCE_TYPE, CONF_URL: resource_url}
        )
        _LOGGER.info("Registered Lovelace resource for %s", DOMAIN)

    hass.data[DATA_CARD_REGISTERED] = True

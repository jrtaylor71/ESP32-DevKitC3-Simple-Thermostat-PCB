"""Serve and register the bundled Lovelace card."""

from __future__ import annotations

import logging
from pathlib import Path

from homeassistant.components.frontend import add_extra_js_url
from homeassistant.components.http import StaticPathConfig
from homeassistant.core import HomeAssistant
from homeassistant.loader import async_get_integration

from .const import CARD_FILENAME, CARD_URL, DOMAIN

_LOGGER = logging.getLogger(__name__)

DATA_CARD_REGISTERED = f"{DOMAIN}_card_registered"


async def async_register_card(hass: HomeAssistant) -> None:
    """Serve the card JS and add it to the frontend (once per HA run)."""
    if hass.data.get(DATA_CARD_REGISTERED):
        return
    hass.data[DATA_CARD_REGISTERED] = True
    path = Path(__file__).parent / "frontend" / CARD_FILENAME
    integration = await async_get_integration(hass, DOMAIN)
    try:
        await hass.http.async_register_static_paths(
            [StaticPathConfig(CARD_URL, str(path), cache_headers=False)]
        )
    except RuntimeError:
        # Route already registered (e.g. integration set up again in the same run).
        _LOGGER.debug("Static path %s already registered", CARD_URL)
    add_extra_js_url(hass, f"{CARD_URL}?v={integration.version}")

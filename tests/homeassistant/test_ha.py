import asyncio
import pytest
from homeassistant import config_entries
from homeassistant.data_entry_flow import FlowResultType
from homeassistant.exceptions import HomeAssistantError
from homeassistant.helpers import device_registry as dr, entity_registry as er
from pytest_homeassistant_custom_component.common import MockConfigEntry

from tests.homeassistant import fake_device as fd
from custom_components.esp32_thermostat.const import DOMAIN


@pytest.fixture
def card_reg(hass):
    """Fake HTTP and Lovelace resource registration."""
    from unittest.mock import AsyncMock, MagicMock

    class FakeResources:
        def __init__(self):
            self.items = []

        async def async_get_info(self):
            return {"resources": len(self.items)}

        def async_items(self):
            return self.items

        async def async_create_item(self, data):
            item = {
                "id": str(len(self.items)),
                "type": data["res_type"],
                "url": data["url"],
            }
            self.items.append(item)
            return item

        async def async_update_item(self, item_id, updates):
            item = next(item for item in self.items if item["id"] == item_id)
            updates = dict(updates)
            if "res_type" in updates:
                updates["type"] = updates.pop("res_type")
            item.update(updates)
            return item

    hass.config.components.update({"http", "frontend", "lovelace"})
    hass.http = MagicMock()
    hass.http.async_register_static_paths = AsyncMock()
    resources = FakeResources()
    hass.data["lovelace"] = {"mode": "storage", "resources": resources}
    yield hass.http.async_register_static_paths, resources


@pytest.fixture
async def dev(hass, socket_enabled, card_reg):
    from homeassistant.util.unit_system import US_CUSTOMARY_SYSTEM
    hass.config.units = US_CUSTOMARY_SYSTEM
    app, runner, host = await fd.start(ping_interval=0.5)
    yield app, host
    await runner.cleanup()


async def _setup(hass, host):
    entry = MockConfigEntry(domain=DOMAIN, unique_id="a1b2c3d4e5f6", title="Shop-Thermostat",
                            data={"host": host, "token": fd.TOKEN})
    entry.add_to_hass(hass)
    assert await hass.config_entries.async_setup(entry.entry_id)
    await hass.async_block_till_done()
    await _wait(lambda: entry.runtime_data.sse_connected)
    return entry


async def _wait(cond, timeout=3.0):
    for _ in range(int(timeout / 0.05)):
        if cond():
            return
        await asyncio.sleep(0.05)
    raise AssertionError("condition not met")


async def test_setup_and_control(hass, dev):
    app, host = dev
    entry = MockConfigEntry(domain=DOMAIN, unique_id="a1b2c3d4e5f6", title="Shop-Thermostat",
                            data={"host": host, "token": fd.TOKEN})
    entry.add_to_hass(hass)
    assert await hass.config_entries.async_setup(entry.entry_id)
    await hass.async_block_till_done()

    st = hass.states.get("climate.shop_thermostat")
    assert st.state == "heat"
    assert st.attributes["temperature"] == 72.0
    assert st.attributes["current_temperature"] == 71.3
    assert st.attributes["current_humidity"] == 45.2
    assert st.attributes["hvac_action"] == "heating"
    assert st.attributes["preset_mode"] == "schedule"
    assert st.attributes["fan_modes"] == ["auto", "on", "cycle"]
    assert st.attributes["hvac_modes"] == ["off", "heat", "cool", "auto"]
    assert st.attributes["min_temp"] == 50 and st.attributes["max_temp"] == 95
    assert st.attributes["target_temp_step"] == 0.5

    # SSE connected and live updates merged
    await _wait(lambda: entry.runtime_data.sse_connected)
    assert app["sse_headers"][-1]["Accept"] == "text/event-stream"
    app["push"]({"current_temperature": 73.0})
    await _wait(lambda: hass.states.get("climate.shop_thermostat").attributes["current_temperature"] == 73.0)
    assert hass.states.get("climate.shop_thermostat").attributes["current_humidity"] == 45.2

    # Entities created per feature flags
    ents = {e.entity_id for e in er.async_entries_for_config_entry(er.async_get(hass), entry.entry_id)}
    assert "sensor.shop_thermostat_pressure" in ents
    assert "switch.shop_thermostat_shower_mode" in ents
    assert "sensor.shop_thermostat_air_quality_index" not in ents
    assert "number.shop_thermostat_stage_1_minimum_runtime" not in ents
    assert hass.states.get("sensor.shop_thermostat_active_schedule_period").state == "day"
    num = hass.states.get("number.shop_thermostat_fan_minutes_per_hour")
    assert num.state == "15" and num.attributes["step"] == 5 and num.attributes["max"] == 60

    # set_temperature with hvac_mode -> single command
    await hass.services.async_call("climate", "set_temperature",
        {"entity_id": "climate.shop_thermostat", "temperature": 75, "hvac_mode": "cool"}, blocking=True)
    assert ("command", {"mode": "cool", "target_temperature": 75.0}) in app["requests"]
    await _wait(lambda: hass.states.get("climate.shop_thermostat").state == "cool")
    assert hass.states.get("climate.shop_thermostat").attributes["temperature"] == 75

    await hass.services.async_call("climate", "set_preset_mode",
        {"entity_id": "climate.shop_thermostat", "preset_mode": "permanent_hold"}, blocking=True)
    assert app["requests"][-1] == ("command", {"schedule_enabled": True, "schedule_override": "permanent"})
    await _wait(lambda: hass.states.get("climate.shop_thermostat").attributes["preset_mode"] == "permanent_hold")

    await hass.services.async_call("climate", "turn_off", {"entity_id": "climate.shop_thermostat"}, blocking=True)
    await _wait(lambda: hass.states.get("climate.shop_thermostat").state == "off")
    await hass.services.async_call("climate", "turn_on", {"entity_id": "climate.shop_thermostat"}, blocking=True)
    assert app["requests"][-1] == ("command", {"mode": "cool"})

    await hass.services.async_call("number", "set_value",
        {"entity_id": "number.shop_thermostat_fan_minutes_per_hour", "value": 20}, blocking=True)
    assert app["requests"][-1] == ("settings", {"fan_minutes_per_hour": 20})

    # Per-mode setpoints are always exposed and sent as commands
    for eid, value in (("number.shop_thermostat_heat_setpoint", "72.0"),
                       ("number.shop_thermostat_cool_setpoint", "75.0"),
                       ("number.shop_thermostat_auto_setpoint", "74.0")):
        state = hass.states.get(eid)
        assert state.state == value and state.attributes["unit_of_measurement"] == "°F", eid
    await hass.services.async_call("number", "set_value",
        {"entity_id": "number.shop_thermostat_cool_setpoint", "value": 78}, blocking=True)
    assert app["requests"][-1] == ("command", {"target_temp_cool": 78.0})

    await hass.services.async_call("switch", "turn_off",
        {"entity_id": "switch.shop_thermostat_schedule_monday"}, blocking=True)
    assert app["requests"][-1] == ("schedule", {"days": [{"day": "monday", "enabled": False}]})
    assert hass.states.get("switch.shop_thermostat_schedule_monday").state == "off"

    # Integration services
    device = dr.async_get(hass).async_get_device(identifiers={(DOMAIN, "a1b2c3d4e5f6")})
    assert device.name == "Shop-Thermostat" and device.sw_version == "2.0.000"
    sched = await hass.services.async_call(DOMAIN, "get_schedule", {"device_id": device.id},
                                           blocking=True, return_response=True)
    assert len(sched["days"]) == 7
    await hass.services.async_call(DOMAIN, "set_schedule",
        {"device_id": device.id, "schedule_enabled": True, "days": sched["days"]}, blocking=True)
    assert app["requests"][-1][1]["days"][0]["day"] == "sunday"
    assert "index" not in app["requests"][-1][1]["days"][0]

    # Command error -> translated HomeAssistantError
    await hass.services.async_call("switch", "turn_off",
        {"entity_id": "switch.shop_thermostat_schedule"}, blocking=True)
    await _wait(lambda: hass.states.get("button.shop_thermostat_resume_schedule").state == "unavailable")
    with pytest.raises(HomeAssistantError) as err:
        await hass.services.async_call(DOMAIN, "set_override",
            {"device_id": device.id, "mode": "temporary"}, blocking=True)
    assert err.value.translation_key == "command_rejected"
    assert err.value.translation_placeholders["field"] == "schedule_override"
    assert "schedule disabled" in str(err.value)

    with pytest.raises(HomeAssistantError):
        await hass.services.async_call(DOMAIN, "get_schedule", {"device_id": "nope"},
                                       blocking=True, return_response=True)

    # Diagnostics redact the token
    from custom_components.esp32_thermostat.diagnostics import async_get_config_entry_diagnostics
    diag = await async_get_config_entry_diagnostics(hass, entry)
    assert diag["entry"]["token"] == "**REDACTED**" and diag["info"]["mac"] == "**REDACTED**"

    # Connection loss -> entities unavailable after poll
    coord = entry.runtime_data
    await coord.async_stop_event_stream()
    app["api_config"]["enabled"] = False  # 403 -> auth failed -> reauth flow
    await coord.async_refresh()
    await hass.async_block_till_done()
    assert hass.states.get("climate.shop_thermostat").state == "unavailable"
    flows = hass.config_entries.flow.async_progress()
    assert any(f["context"]["source"] == "reauth" for f in flows)

    assert await hass.config_entries.async_unload(entry.entry_id)
    await hass.async_block_till_done()


async def test_config_flow_user(hass, dev):
    app, host = dev
    result = await hass.config_entries.flow.async_init(DOMAIN, context={"source": config_entries.SOURCE_USER})
    assert result["type"] is FlowResultType.FORM
    result = await hass.config_entries.flow.async_configure(result["flow_id"], {"host": host, "token": "bad"})
    assert result["errors"] == {"base": "invalid_auth"}
    app["api_config"]["enabled"] = False
    result = await hass.config_entries.flow.async_configure(result["flow_id"], {"host": host, "token": fd.TOKEN})
    assert result["errors"] == {"base": "api_disabled"}
    app["api_config"]["enabled"] = True
    result = await hass.config_entries.flow.async_configure(result["flow_id"], {"host": "127.0.0.1:1", "token": fd.TOKEN})
    assert result["errors"] == {"base": "cannot_connect"}
    result = await hass.config_entries.flow.async_configure(result["flow_id"], {"host": host, "token": fd.TOKEN})
    assert result["type"] is FlowResultType.CREATE_ENTRY
    assert result["title"] == "Shop-Thermostat"
    assert result["result"].unique_id == "a1b2c3d4e5f6"
    await hass.async_block_till_done()
    entry = result["result"]

    # reconfigure + reauth
    result = await hass.config_entries.flow.async_init(DOMAIN, context={"source": config_entries.SOURCE_RECONFIGURE, "entry_id": entry.entry_id})
    assert result["step_id"] == "reconfigure"
    result = await hass.config_entries.flow.async_configure(result["flow_id"], {"host": host})
    assert result["reason"] == "reconfigure_successful"
    await hass.async_block_till_done()
    result = await hass.config_entries.flow.async_init(DOMAIN, context={"source": config_entries.SOURCE_REAUTH, "entry_id": entry.entry_id}, data=dict(entry.data))
    assert result["step_id"] == "reauth_confirm"
    result = await hass.config_entries.flow.async_configure(result["flow_id"], {"token": fd.TOKEN})
    assert result["reason"] == "reauth_successful"
    await hass.async_block_till_done()
    assert await hass.config_entries.async_unload(entry.entry_id)
    await hass.async_block_till_done()


async def test_zeroconf(hass, dev):
    from ipaddress import ip_address
    from homeassistant.components.zeroconf import ZeroconfServiceInfo
    app, host = dev
    ip, port = host.split(":")
    info = ZeroconfServiceInfo(ip_address=ip_address(ip), ip_addresses=[ip_address(ip)], port=int(port),
        hostname="Shop-Thermostat.local.", type="_esp32thermostat._tcp.local.",
        name="Shop-Thermostat._esp32thermostat._tcp.local.",
        properties={"id": "a1b2c3d4e5f6", "name": "Shop-Thermostat", "api": "1", "unit": "F"})
    result = await hass.config_entries.flow.async_init(DOMAIN, context={"source": config_entries.SOURCE_ZEROCONF}, data=info)
    assert result["step_id"] == "zeroconf_confirm"
    assert result["description_placeholders"]["name"] == "Shop-Thermostat"
    # discovery uses host (no port); point it at the fake server port for the test
    flow = hass.config_entries.flow._progress[result["flow_id"]]
    flow._host = host
    result = await hass.config_entries.flow.async_configure(result["flow_id"], {"token": fd.TOKEN})
    assert result["type"] is FlowResultType.CREATE_ENTRY
    await hass.async_block_till_done()
    # rediscovery with new IP updates host and aborts
    result = await hass.config_entries.flow.async_init(DOMAIN, context={"source": config_entries.SOURCE_ZEROCONF}, data=info)
    assert result["reason"] == "already_configured"
    entry = hass.config_entries.async_entries(DOMAIN)[0]
    assert entry.data["host"] == ip
    await hass.config_entries.async_unload(entry.entry_id)
    await hass.async_block_till_done()


async def test_settings_entities(hass, dev):
    app, host = dev
    entry = await _setup(hass, host)
    ent_reg = er.async_get(hass)
    ents = {e.entity_id: e for e in er.async_entries_for_config_entry(ent_reg, entry.entry_id)}
    expected = [
        "switch.shop_thermostat_stage_2_heating", "switch.shop_thermostat_stage_2_cooling",
        "switch.shop_thermostat_heat_pump", "switch.shop_thermostat_fan_with_heat",
        "switch.shop_thermostat_fan_with_cool", "switch.shop_thermostat_backup_heat",
        "switch.shop_thermostat_hydronic_heat", "switch.shop_thermostat_eu_humidity_control",
        "switch.shop_thermostat_shower_mode_feature",
        "select.shop_thermostat_backup_heat_relay", "select.shop_thermostat_eu_humidity_relay",
        "select.shop_thermostat_temperature_unit", "select.shop_thermostat_region",
        "select.shop_thermostat_time_zone",
        "number.shop_thermostat_backup_heat_delay", "number.shop_thermostat_backup_heat_minimum_rise",
        "number.shop_thermostat_backup_heat_maximum_drop", "number.shop_thermostat_eu_humidity_deadband",
        "number.shop_thermostat_temperature_offset", "number.shop_thermostat_humidity_offset",
    ]
    for eid in expected:
        assert eid in ents, eid
        assert ents[eid].entity_category == "config", eid
    assert "sensor.shop_thermostat_outdoor_temperature" not in ents

    st = lambda eid: hass.states.get(eid)
    assert st("switch.shop_thermostat_fan_with_cool").state == "on"
    assert st("switch.shop_thermostat_stage_2_heating").state == "off"
    assert st("select.shop_thermostat_backup_heat_relay").state == "pump"
    assert st("select.shop_thermostat_backup_heat_relay").attributes["options"] == ["pump", "heat2", "cool2"]
    assert st("select.shop_thermostat_temperature_unit").state == "fahrenheit"
    assert st("select.shop_thermostat_region").state == "us"
    assert st("select.shop_thermostat_time_zone").state == "central"
    assert st("number.shop_thermostat_backup_heat_delay").state == "30"
    # temperature differences (including the offset) stay in the device unit
    assert st("number.shop_thermostat_backup_heat_minimum_rise").attributes["unit_of_measurement"] == "°F"
    off = st("number.shop_thermostat_temperature_offset")
    assert off.attributes["unit_of_measurement"] == "°F"
    assert (off.attributes["min"], off.attributes["max"], off.attributes["step"]) == (-18, 18, 0.1)
    hum = st("number.shop_thermostat_humidity_offset")
    assert (hum.attributes["min"], hum.attributes["max"], hum.attributes["step"]) == (-50, 50, 0.5)

    async def call(domain, service, eid, **data):
        await hass.services.async_call(domain, service, {"entity_id": eid, **data}, blocking=True)

    await call("switch", "turn_on", "switch.shop_thermostat_stage_2_heating")
    assert app["requests"][-1] == ("settings", {"stage2_heat_enabled": True})
    await _wait(lambda: st("switch.shop_thermostat_stage_2_heating").state == "on")

    # 409 conflict surfaces as translated error, state unchanged
    with pytest.raises(HomeAssistantError) as err:
        await call("switch", "turn_on", "switch.shop_thermostat_heat_pump")
    assert app["requests"][-1] == ("settings", {"heat_pump_enabled": True})
    assert err.value.translation_key == "command_rejected"
    assert err.value.translation_placeholders["field"] == "heat_pump_enabled"
    assert st("switch.shop_thermostat_heat_pump").state == "off"

    await call("switch", "turn_off", "switch.shop_thermostat_shower_mode_feature")
    assert app["requests"][-1] == ("settings", {"shower_mode_enabled": False})

    await call("select", "select_option", "select.shop_thermostat_backup_heat_relay", option="cool2")
    assert app["requests"][-1] == ("settings", {"backup_heat_relay": "cool2"})
    await call("switch", "turn_on", "switch.shop_thermostat_backup_heat")
    await call("select", "select_option", "select.shop_thermostat_eu_humidity_relay", option="cool2")
    await call("switch", "turn_on", "switch.shop_thermostat_eu_humidity_control")
    with pytest.raises(HomeAssistantError) as err:
        await call("switch", "turn_on", "switch.shop_thermostat_stage_2_cooling")
    assert err.value.translation_placeholders["field"] == "stage2_cool_enabled"

    await call("select", "select_option", "select.shop_thermostat_time_zone", option="pacific")
    assert app["requests"][-1] == ("settings", {"time_zone": "PST8PDT,M3.2.0,M11.1.0"})
    await _wait(lambda: st("select.shop_thermostat_time_zone").state == "pacific")
    await call("select", "select_option", "select.shop_thermostat_temperature_unit", option="celsius")
    assert app["requests"][-1] == ("settings", {"temperature_unit": "C"})
    await call("select", "select_option", "select.shop_thermostat_region", option="eu")
    assert app["requests"][-1] == ("settings", {"region": "EU"})

    # unknown device TZ -> no current option, no crash
    app["push"]({"settings": {**app["state"]["settings"], "time_zone": "IST-5:30"}})
    await _wait(lambda: st("select.shop_thermostat_time_zone").state == "unknown")

    await call("number", "set_value", "number.shop_thermostat_backup_heat_delay", value=45)
    assert app["requests"][-1] == ("settings", {"backup_heat_delay_min": 45})
    assert isinstance(app["requests"][-1][1]["backup_heat_delay_min"], int)
    await call("number", "set_value", "number.shop_thermostat_backup_heat_maximum_drop", value=7.5)
    assert app["requests"][-1] == ("settings", {"backup_heat_max_drop": 7.5})
    offset = st("number.shop_thermostat_temperature_offset").attributes
    assert offset["unit_of_measurement"] == "°F" and (offset["min"], offset["max"]) == (-18.0, 18.0)
    await call("number", "set_value", "number.shop_thermostat_temperature_offset", value=-7.2)
    assert app["requests"][-1] == ("settings", {"temp_offset": -7.2})
    await call("number", "set_value", "number.shop_thermostat_humidity_offset", value=2.5)
    assert app["requests"][-1] == ("settings", {"humidity_offset": 2.5})
    await call("number", "set_value", "number.shop_thermostat_eu_humidity_deadband", value=3.5)
    assert app["requests"][-1] == ("settings", {"eu_humidity_deadband": 3.5})
    await _wait(lambda: st("number.shop_thermostat_eu_humidity_deadband").state == "3.5")

    # climate exposes stage/schedule details for the card
    attrs = st("climate.shop_thermostat").attributes
    assert attrs["stage1_active"] is True and attrs["stage2_active"] is False
    assert attrs["active_period"] == "day" and attrs["override_type"] == "none"
    assert attrs["schedule_enabled"] is True and "override_remaining_min" in attrs

    assert await hass.config_entries.async_unload(entry.entry_id)
    await hass.async_block_till_done()


async def test_hydronic_celsius_range(hass, socket_enabled, card_reg):
    from homeassistant.util.unit_system import METRIC_SYSTEM
    hass.config.units = METRIC_SYSTEM
    app, runner, host = await fd.start(
        ping_interval=0.5,
        info={"temperature_unit": "C", "min_temp": 10.0, "max_temp": 35.0,
              "features": {"hydronic_heat": True}},
        state={"current_temperature": 21.0, "target_temperature": 22.0},
    )
    try:
        entry = await _setup(hass, host)
        for eid in ("number.shop_thermostat_hydronic_low_temperature",
                    "number.shop_thermostat_hydronic_high_temperature"):
            attrs = hass.states.get(eid).attributes
            assert (attrs["min"], attrs["max"]) == (10, 95), eid
        rise = hass.states.get("number.shop_thermostat_backup_heat_minimum_rise")
        assert rise.attributes["unit_of_measurement"] == "°C"
        offset = hass.states.get("number.shop_thermostat_temperature_offset").attributes
        assert offset["unit_of_measurement"] == "°C" and (offset["min"], offset["max"]) == (-10.0, 10.0)
        heat = hass.states.get("number.shop_thermostat_heat_setpoint").attributes
        assert heat["unit_of_measurement"] == "°C" and (heat["min"], heat["max"]) == (10.0, 35.0)
        assert await hass.config_entries.async_unload(entry.entry_id)
        await hass.async_block_till_done()
    finally:
        await runner.cleanup()


async def test_weather_sensors(hass, socket_enabled, card_reg):
    from homeassistant.util.unit_system import US_CUSTOMARY_SYSTEM
    hass.config.units = US_CUSTOMARY_SYSTEM
    app, runner, host = await fd.start(ping_interval=0.5, info={"features": {"weather": True}})
    try:
        entry = await _setup(hass, host)
        temp, cond = "sensor.shop_thermostat_outdoor_temperature", "sensor.shop_thermostat_outdoor_condition"
        assert hass.states.get(temp).state == "unavailable"
        assert hass.states.get(cond).state == "unavailable"
        app["push"]({"weather": {"temperature": 41.0, "temp_high": 48.0, "temp_low": 33.0, "humidity": 70,
                                 "condition": "Clouds", "description": "overcast clouds", "icon": "04d",
                                 "wind_speed": 5.2}})
        await _wait(lambda: hass.states.get(cond).state == "Clouds")
        assert hass.states.get(temp).state == "41.0"
        assert hass.states.get(temp).attributes["unit_of_measurement"] == "°F"
        app["push"]({"weather": None})
        await _wait(lambda: hass.states.get(temp).state == "unavailable")
        assert hass.states.get(cond).state == "unavailable"
        assert await hass.config_entries.async_unload(entry.entry_id)
        await hass.async_block_till_done()
    finally:
        await runner.cleanup()


async def test_info_rev_reload(hass, dev):
    from unittest.mock import patch
    app, host = dev
    with patch("custom_components.esp32_thermostat.coordinator.INFO_REV_RELOAD_DELAY", 0.3):
        entry = await _setup(hass, host)
        first = entry.runtime_data
        assert first.info_rev == 1
        reloads = []
        orig = hass.config_entries.async_schedule_reload
        def track(entry_id):
            reloads.append(entry_id)
            orig(entry_id)
        with patch.object(hass.config_entries, "async_schedule_reload", side_effect=track):
            # unchanged / other updates -> no reload
            app["push"]({"current_temperature": 70.0, "info_rev": 1})
            await asyncio.sleep(0.5)
            assert reloads == []
            # device-side config change (enable weather) -> one debounced reload
            app["info"]["features"]["weather"] = True
            app["push"]({"info_rev": 2})
            await asyncio.sleep(0.05)
            app["push"]({"info_rev": 3})
            app["state"]["info_rev"] = 3
            await _wait(lambda: reloads == [entry.entry_id])
            await hass.async_block_till_done()
            await _wait(lambda: entry.runtime_data is not first and entry.runtime_data.sse_connected)
            assert entry.runtime_data.info_rev == 3
            assert hass.states.get("sensor.shop_thermostat_outdoor_temperature") is not None
            await asyncio.sleep(0.8)
            assert reloads == [entry.entry_id]
    assert await hass.config_entries.async_unload(entry.entry_id)
    await hass.async_block_till_done()


async def test_card_registered_once(hass, dev, card_reg):
    from custom_components.esp32_thermostat.card import async_register_card
    from custom_components.esp32_thermostat.const import CARD_URL
    static, resources = card_reg
    app, host = dev
    entry = await _setup(hass, host)
    assert await hass.config_entries.async_reload(entry.entry_id)
    await hass.async_block_till_done()
    await async_register_card(hass)
    assert static.await_count == 1
    (cfg,), = static.await_args.args
    assert cfg.url_path == CARD_URL and cfg.cache_headers is False
    assert cfg.path.endswith("frontend/esp32-thermostat-card.js")
    import os
    assert os.path.isfile(cfg.path)
    assert resources.items == [
        {
            "id": "0",
            "type": "module",
            "url": CARD_URL + "?v=2.0.0",
        }
    ]
    assert await hass.config_entries.async_unload(entry.entry_id)
    await hass.async_block_till_done()


async def test_card_updates_existing_lovelace_resource(hass, card_reg):
    from custom_components.esp32_thermostat.card import async_register_card
    from custom_components.esp32_thermostat.const import CARD_URL

    _, resources = card_reg
    resources.items.append(
        {"id": "existing", "type": "js", "url": CARD_URL + "?v=old"}
    )
    await async_register_card(hass)
    assert resources.items == [
        {
            "id": "existing",
            "type": "module",
            "url": CARD_URL + "?v=2.0.0",
        }
    ]

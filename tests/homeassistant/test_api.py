import asyncio
import aiohttp, pytest
from tests.homeassistant import fake_device as fd
from custom_components.esp32_thermostat.api import (
    SSEParser, ThermostatApiClient, ThermostatApiDisabledError, ThermostatAuthError,
    ThermostatCommandError, ThermostatConnectionError)


@pytest.fixture(autouse=True)
def _sockets(socket_enabled):
    yield


@pytest.fixture
async def dev():
    app, runner, host = await fd.start(ping_interval=0.2, initial_update={"current_temperature": 72.5})
    async with aiohttp.ClientSession() as s:
        yield app, ThermostatApiClient(host, fd.TOKEN, s), ThermostatApiClient(host, "bad", s), s, host
    await runner.cleanup()


async def test_info_state(dev):
    app, api, *_ = dev
    assert (await api.async_get_info())["device_id"] == "a1b2c3d4e5f6"
    assert (await api.async_get_state())["mode"] == "heat"
    assert len((await api.async_get_schedule())["days"]) == 7


async def test_sse_state_update_ping(dev):
    app, api, *_ = dev
    events, data = [], None
    async for ev in api.async_events():
        events.append(ev.event)
        if ev.event == "state": data = dict(ev.data)
        elif ev.event == "update": data.update(ev.data)
        if ev.event == "ping": break
    assert events[:3] == ["state", "update", "ping"]
    assert data["current_temperature"] == 72.5 and data["mode"] == "heat"
    h = app["sse_headers"][-1]
    assert h["Accept"] == "text/event-stream" and h["Cache-Control"] == "no-cache"
    assert h["Authorization"] == f"Bearer {fd.TOKEN}"


async def test_sse_requires_accept_header(dev):
    app, api, bad, s, host = dev
    async with s.get(f"http://{host}/api/v1/events", headers={"Authorization": f"Bearer {fd.TOKEN}"}) as r:
        assert r.status == 404


async def test_command_errors(dev):
    app, api, *_ = dev
    with pytest.raises(ThermostatCommandError) as e:
        await api.async_command(target_temperature=120)
    assert e.value.status == 400 and e.value.field == "target_temperature" and e.value.message == "out of range"
    with pytest.raises(ThermostatCommandError) as e:
        await api.async_command(schedule_enabled=False, schedule_override="temporary")
    assert e.value.status == 409 and e.value.field == "schedule_override"
    with pytest.raises(ThermostatCommandError):
        await api.async_settings(fan_minutes_per_hour=61)
    await api.async_command(mode="cool", target_temperature=75)
    assert app["state"]["target_temp_cool"] == 75 and app["state"]["target_temperature"] == 75
    await api.async_put_schedule({"days": [{"day": "monday", "enabled": False}]})
    assert app["state"]["schedule"]["days_enabled"][1] is False
    await api.async_action("reboot")
    with pytest.raises(ThermostatCommandError):
        await api.async_action("refresh_weather")


async def test_auth_errors(dev):
    app, api, bad, *_ = dev
    with pytest.raises(ThermostatAuthError) as e:
        await bad.async_get_info()
    assert not isinstance(e.value, ThermostatApiDisabledError)
    with pytest.raises(ThermostatAuthError):
        async for _ in bad.async_events():
            pass
    app["api_config"]["enabled"] = False
    with pytest.raises(ThermostatApiDisabledError):
        await api.async_get_state()


async def test_connection_error():
    async with aiohttp.ClientSession() as s:
        with pytest.raises(ThermostatConnectionError):
            await ThermostatApiClient("127.0.0.1:1", "x", s).async_get_info()


def test_parser_multiline_and_comments():
    p = SSEParser()
    out = [p.feed_line(l) for l in [": hi", "event: update", 'data: {"a":', "data: 1}", "", "data: {}", ""]]
    evs = [e for e in out if e]
    assert evs[0].event == "update" and evs[0].data == {"a": 1}
    assert evs[1].event == "message" and evs[1].data == {}

# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
"""Throwaway fake ESP32 thermostat implementing API v1."""
import asyncio, copy, json
from aiohttp import web

TOKEN = "0123456789abcdef0123456789abcdef"
INFO = {
    "api_version": 1, "device_id": "a1b2c3d4e5f6", "mac": "A1:B2:C3:D4:E5:F6",
    "hostname": "Shop-Thermostat", "manufacturer": "TDC", "model": "ESP32-S3 Simple Thermostat",
    "firmware_version": "2.0.001", "build": "Nov 24 2026 10:00:00", "temperature_unit": "F",
    "min_temp": 50.0, "max_temp": 95.0, "temp_step": 0.5, "region": "US", "mqtt_enabled": False,
    "features": {"sensor": "BME280", "humidity": True, "pressure": True, "air_quality": False,
        "hydronic_heat": False, "hydronic_supply_sensor": False, "hydronic_return_sensor": False,
        "stage2_heat": False, "stage2_cool": False, "heat_pump": False, "backup_heat": False,
        "motion_sensor": False, "shower_mode": True, "eu_humidity_control": False, "weather": False},
}
STATE = {
    "current_temperature": 71.3, "current_humidity": 45.2, "pressure_hpa": 1013.2, "air_quality": None,
    "gas_resistance_kohm": None, "hydronic_supply_temperature": None, "hydronic_return_temperature": None,
    "mode": "heat", "fan_mode": "auto", "action": "heating", "target_temperature": 72.0,
    "target_temp_heat": 72.0, "target_temp_cool": 76.0, "target_temp_auto": 74.0,
    "stage1_active": True, "stage2_active": False, "backup_heat_active": False, "hydronic_lockout": False,
    "eu_humidity_demand": False,
    "relays": {"heat1": True, "heat2": False, "cool1": False, "cool2": False, "fan": True, "pump": False},
    "schedule": {"enabled": True, "override": False, "override_type": "none", "override_remaining_min": None,
                 "active_period": "day", "days_enabled": [True] * 7},
    "shower": {"active": False, "remaining_min": 0}, "motion": None,
    "settings": {"temp_swing": 1.0, "auto_temp_swing": 1.5, "stage1_min_runtime": 300, "stage2_temp_delta": 2.0,
                 "fan_minutes_per_hour": 15, "shower_duration": 30, "display_brightness": 130,
                 "hydronic_temp_low": 110.0, "hydronic_temp_high": 130.0, "eu_humidity_setpoint": 60.0,
                 "stage2_heat_enabled": False, "stage2_cool_enabled": False, "heat_pump_enabled": False,
                 "fan_with_heat": False, "fan_with_cool": True, "backup_heat_enabled": False,
                 "backup_heat_relay": "pump", "backup_heat_delay_min": 30, "backup_heat_min_rise": 1.0,
                 "backup_heat_max_drop": 2.0, "hydronic_heat_enabled": False, "eu_humidity_enabled": False,
                 "eu_humidity_relay": "cool1", "eu_humidity_deadband": 5.0, "shower_mode_enabled": True,
                 "temp_offset": 0.0, "humidity_offset": 0.0, "temperature_unit": "F", "region": "US",
                 "time_zone": "CST6CDT,M3.2.0,M11.1.0"},
    "weather": None, "info_rev": 1,
    "wifi_rssi": -55, "uptime_s": 12345, "free_heap": 120000,
}
DAYS = ["sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"]
SETTINGS_RANGES = {"temp_swing": (0.2, 3.0), "auto_temp_swing": (0.2, 5.0), "stage1_min_runtime": (60, 1800),
    "stage2_temp_delta": (0.5, 5.0), "fan_minutes_per_hour": (0, 60), "shower_duration": (5, 120),
    "display_brightness": (30, 255), "hydronic_temp_low": (50, 200), "hydronic_temp_high": (50, 200),
    "eu_humidity_setpoint": (30, 90), "backup_heat_delay_min": (5, 180), "backup_heat_min_rise": (0.1, 5.0),
    "backup_heat_max_drop": (0.1, 10.0), "eu_humidity_deadband": (1, 20), "temp_offset": (-18.0, 18.0),
    "humidity_offset": (-50.0, 50.0)}
INT_SETTINGS = {"stage1_min_runtime", "fan_minutes_per_hour", "shower_duration", "display_brightness",
                "backup_heat_delay_min"}
BOOL_SETTINGS = {"stage2_heat_enabled", "stage2_cool_enabled", "heat_pump_enabled", "fan_with_heat",
                 "fan_with_cool", "backup_heat_enabled", "hydronic_heat_enabled", "eu_humidity_enabled",
                 "shower_mode_enabled"}
ENUM_SETTINGS = {"backup_heat_relay": ("pump", "heat2", "cool2"), "eu_humidity_relay": ("cool1", "cool2", "pump"),
                 "temperature_unit": ("F", "C"), "region": ("US", "EU")}


def _conflict(s: dict) -> str | None:
    """Return a description of a settings conflict (addendum rules) or None."""
    if s["stage2_heat_enabled"] and s["heat_pump_enabled"]:
        return "stage2 heat and heat pump"
    if s["backup_heat_enabled"] and s["backup_heat_relay"] == "heat2" and (
            s["stage2_heat_enabled"] or s["heat_pump_enabled"]):
        return "backup heat relay heat2 in use"
    if s["backup_heat_enabled"] and s["backup_heat_relay"] == "cool2" and s["stage2_cool_enabled"]:
        return "backup heat relay cool2 in use"
    if s["eu_humidity_enabled"] and s["eu_humidity_relay"] == "cool2" and s["stage2_cool_enabled"]:
        return "eu humidity relay cool2 in use"
    return None


def make_app(ping_interval: float = 30.0, initial_update: dict | None = None) -> web.Application:
    app = web.Application()
    app["info"] = copy.deepcopy(INFO)
    app["state"] = copy.deepcopy(STATE)
    app["schedule"] = {"enabled": True, "days": [
        {"index": i, "day": d, "enabled": True,
         "day_period": {"hour": 6, "minute": 0, "heat": 72.0, "cool": 76.0, "auto": 74.0, "active": True},
         "night_period": {"hour": 22, "minute": 0, "heat": 68.0, "cool": 78.0, "auto": 73.0, "active": True}}
        for i, d in enumerate(DAYS)]}
    app["api_config"] = {"enabled": True}
    app["clients"] = []
    app["requests"] = []
    app["sse_headers"] = []

    def err(status, error, field=None, message=None):
        body = {"error": error}
        if field: body["field"] = field
        if message: body["message"] = message
        return web.json_response(body, status=status)

    @web.middleware
    async def auth(request, handler):
        if request.headers.get("Authorization") != f"Bearer {TOKEN}":
            return err(401, "unauthorized")
        if not app["api_config"]["enabled"]:
            return err(403, "api_disabled")
        return await handler(request)
    app.middlewares.append(auth)

    def push(changes: dict):
        app["state"].update(changes)
        for q in app["clients"]:
            q.put_nowait(("update", changes))
    app["push"] = push

    async def info(request): return web.json_response(app["info"])
    async def state(request): return web.json_response(app["state"])

    async def events(request):
        app["sse_headers"].append(dict(request.headers))
        if request.headers.get("Accept") != "text/event-stream":
            return err(404, "not_found")
        if len(app["clients"]) >= 2:
            return err(503, "too_many_clients")
        resp = web.StreamResponse(headers={"Content-Type": "text/event-stream", "Cache-Control": "no-cache"})
        await resp.prepare(request)
        q: asyncio.Queue = asyncio.Queue()
        app["clients"].append(q)
        try:
            # full state with multi-line data to exercise the parser
            body = json.dumps(app["state"], indent=1)
            await resp.write(("event: state\n" + "".join(f"data: {l}\n" for l in body.splitlines()) + "\n").encode())
            if initial_update:
                q.put_nowait(("update", initial_update))
            while True:
                try:
                    ev, data = await asyncio.wait_for(q.get(), ping_interval)
                except asyncio.TimeoutError:
                    ev, data = "ping", {}
                await resp.write(f": comment\nevent: {ev}\ndata: {json.dumps(data)}\n\n".encode())
        finally:
            app["clients"].remove(q)
        return resp

    async def command(request):
        body = await request.json()
        app["requests"].append(("command", body))
        allowed = {"mode", "fan_mode", "target_temperature", "target_temp_heat", "target_temp_cool",
                   "target_temp_auto", "shower_mode", "schedule_enabled", "schedule_override"}
        for k in body:
            if k not in allowed: return err(400, "invalid", k, "unknown field")
        st = copy.deepcopy(app["state"])
        if "schedule_enabled" in body: st["schedule"]["enabled"] = bool(body["schedule_enabled"])
        if "schedule_override" in body:
            if not st["schedule"]["enabled"]:
                return err(409, "conflict", "schedule_override", "schedule disabled")
            o = body["schedule_override"]
            if o not in ("resume", "temporary", "permanent"):
                return err(400, "invalid", "schedule_override", "bad value")
            st["schedule"]["override"] = o != "resume"
            st["schedule"]["override_type"] = "none" if o == "resume" else o
        if "mode" in body:
            if body["mode"] not in ("off", "heat", "cool", "auto"):
                return err(400, "invalid", "mode", "bad mode")
            st["mode"] = body["mode"]
        if "target_temperature" in body:
            t = body["target_temperature"]
            if st["mode"] == "off": return err(400, "invalid", "target_temperature", "mode is off")
            if not 50 <= t <= 95: return err(400, "invalid", "target_temperature", "out of range")
            st[f"target_temp_{st['mode']}"] = t
        st["target_temperature"] = None if st["mode"] == "off" else st[f"target_temp_{st['mode']}"]
        if "fan_mode" in body: st["fan_mode"] = body["fan_mode"]
        if "shower_mode" in body:
            if not app["info"]["features"]["shower_mode"]: return err(409, "conflict", "shower_mode", "no shower")
            st["shower"] = {"active": bool(body["shower_mode"]), "remaining_min": 30 if body["shower_mode"] else 0}
        changes = {k: v for k, v in st.items() if v != app["state"].get(k)}
        push(changes)
        return web.json_response({"status": "accepted"})

    async def settings(request):
        body = await request.json()
        app["requests"].append(("settings", body))
        cur = app["state"]["settings"]
        unit = body.get("temperature_unit", cur["temperature_unit"])
        for k, v in body.items():
            if k in BOOL_SETTINGS:
                if not isinstance(v, bool): return err(400, "invalid", k, "must be bool")
            elif k in ENUM_SETTINGS:
                if v not in ENUM_SETTINGS[k]: return err(400, "invalid", k, "bad value")
            elif k == "time_zone":
                if not isinstance(v, str) or not 1 <= len(v) <= 63: return err(400, "invalid", k, "bad tz")
            elif k in SETTINGS_RANGES:
                if isinstance(v, bool) or not isinstance(v, (int, float)): return err(400, "invalid", k, "type")
                if k in INT_SETTINGS and not isinstance(v, int): return err(400, "invalid", k, "integer")
                lo, hi = SETTINGS_RANGES[k]
                if k.startswith("hydronic_temp") and unit == "C": lo, hi = 10, 95
                if not lo <= v <= hi: return err(400, "invalid", k, f"must be {lo}-{hi}")
            else:
                return err(400, "invalid", k, "unknown field")
        s = {**cur, **body}
        if (c := _conflict(s)) is not None:
            field = next((k for k in body if _conflict({**cur, k: body[k]})), next(iter(body)))
            return err(409, "conflict", field, c)
        push({"settings": s})
        return web.json_response({"status": "accepted"})

    async def get_schedule(request): return web.json_response(app["schedule"])

    async def put_schedule(request):
        body = await request.json()
        app["requests"].append(("schedule", body))
        if "enabled" in body: app["schedule"]["enabled"] = body["enabled"]
        for d in body.get("days", []):
            idx = DAYS.index(d["day"]) if "day" in d else d.get("index")
            if idx is None or not 0 <= idx <= 6: return err(400, "invalid", "days", "bad day")
            tgt = app["schedule"]["days"][idx]
            for k in ("enabled",):
                if k in d: tgt[k] = d[k]
            for p in ("day_period", "night_period"):
                if p in d: tgt[p].update(d[p])
        sch = {**app["state"]["schedule"], "enabled": app["schedule"]["enabled"],
               "days_enabled": [d["enabled"] for d in app["schedule"]["days"]]}
        push({"schedule": sch})
        return web.json_response({"status": "accepted"})

    async def action(request):
        body = await request.json()
        app["requests"].append(("action", body))
        if body.get("action") == "refresh_weather" and not app["info"]["features"]["weather"]:
            return err(409, "conflict", "action", "weather disabled")
        if body.get("action") not in ("reboot", "refresh_weather"):
            return err(400, "invalid", "action", "unknown")
        return web.json_response({"status": "accepted"})

    app.router.add_get("/api/v1/info", info)
    app.router.add_get("/api/v1/state", state)
    app.router.add_get("/api/v1/events", events)
    app.router.add_post("/api/v1/command", command)
    app.router.add_post("/api/v1/settings", settings)
    app.router.add_get("/api/v1/schedule", get_schedule)
    app.router.add_put("/api/v1/schedule", put_schedule)
    app.router.add_post("/api/v1/action", action)
    return app


async def start(port: int = 0, info: dict | None = None, state: dict | None = None, **kw):
    app = make_app(**kw)
    if info:
        app["info"] = {**app["info"], **info, "features": {**app["info"]["features"], **info.get("features", {})}}
    if state:
        app["state"].update(state)
    runner = web.AppRunner(app, shutdown_timeout=0.5)
    await runner.setup()
    site = web.TCPSite(runner, "127.0.0.1", port)
    await site.start()
    port = site._server.sockets[0].getsockname()[1]
    return app, runner, f"127.0.0.1:{port}"

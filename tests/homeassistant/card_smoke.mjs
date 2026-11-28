// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Minimal DOM-stub smoke test for the bundled Lovelace card (no jsdom).
import assert from "node:assert/strict";
import { fileURLToPath, pathToFileURL } from "node:url";

const CARD = fileURLToPath(new URL("../../custom_components/esp32_thermostat/frontend/esp32-thermostat-card.js", import.meta.url));

class FakeEl {
  constructor(tag = "div") {
    this.tagName = tag.toUpperCase();
    this.dataset = {};
    this.style = {};
    this.attrs = {};
    this.children = [];
    this.listeners = {};
    this.hidden = false;
    this.textContent = "";
    this.value = "";
    this.checked = false;
    this.disabled = false;
    this._byId = {};
    const cls = new Set();
    this.classList = {
      toggle: (c, on) => { (on ?? !cls.has(c)) ? cls.add(c) : cls.delete(c); return cls.has(c); },
      add: (c) => cls.add(c), remove: (c) => cls.delete(c), contains: (c) => cls.has(c),
    };
  }
  set innerHTML(v) { this._html = v; this.children = []; }
  get innerHTML() { return this._html || ""; }
  get lastElementChild() { return this._last || (this._last = new FakeEl("span")); }
  setAttribute(k, v) { this.attrs[k] = String(v); }
  getAttribute(k) { return this.attrs[k]; }
  append(...n) { this.children.push(...n); }
  appendChild(n) { this.children.push(n); return n; }
  prepend(n) { this.children.unshift(n); }
  remove() {}
  focus() {}
  addEventListener(t, fn) { (this.listeners[t] ||= []).push(fn); }
  dispatchEvent(ev) { (this.listeners[ev.type] || []).forEach((f) => f(ev)); return true; }
  getElementById(id) { return this._byId[id] || (this._byId[id] = new FakeEl()); }
  querySelector(sel) { return this._byId[sel] || (this._byId[sel] = new FakeEl()); }
  querySelectorAll() { return []; }
  closest() { return null; }
  getBoundingClientRect() { return { left: 0, top: 0, width: 200, height: 200 }; }
  setPointerCapture() {}
  releasePointerCapture() {}
}

class HTMLElement extends FakeEl {
  attachShadow() { this.shadowRoot = new FakeEl("shadow"); return this.shadowRoot; }
}
const registry = new Map();
globalThis.HTMLElement = HTMLElement;
globalThis.customElements = {
  define: (n, c) => registry.set(n, c), get: (n) => registry.get(n), whenDefined: () => new Promise(() => {}),
};
globalThis.document = { createElement: (t) => (registry.has(t) ? new (registry.get(t))() : new FakeEl(t)) };
globalThis.window = globalThis;
globalThis.CustomEvent = class { constructor(type, init) { this.type = type; Object.assign(this, init); } };
const origInfo = console.info; console.info = () => {};

await import(pathToFileURL(CARD).href);
console.info = origInfo;

const Card = registry.get("esp32-thermostat-card");
assert.ok(Card, "card registered");
assert.ok(registry.get("esp32-thermostat-card-editor"), "editor registered");
assert.equal(window.customCards.filter((c) => c.type === "esp32-thermostat-card").length, 1);

const calls = [];
const climateAttrs = {
  friendly_name: "Shop", hvac_modes: ["off", "heat", "cool", "auto"], fan_modes: ["auto", "on", "cycle"],
  min_temp: 50, max_temp: 95, target_temp_step: 0.5, current_temperature: 71.3, temperature: 72,
  hvac_action: "heating", fan_mode: "auto", stage1_active: true, stage2_active: false, backup_heat_active: false,
  schedule_enabled: true, active_period: "day", override_type: "temporary", override_remaining_min: 90,
};
const st = (state, attributes = {}) => ({ state, attributes });
const mkHass = (climate) => ({
  config: { unit_system: { temperature: "°F" } },
  states: {
    "climate.shop": climate,
    "sensor.shop_humidity": st("45.2", { unit_of_measurement: "%" }),
    "sensor.shop_outdoor_temperature": st("41.0", { unit_of_measurement: "°F" }),
    "sensor.shop_outdoor_condition": st("Clouds"),
    "switch.shop_shower_mode": st("off"),
    "switch.shop_schedule": st("on"),
    "number.shop_cool_setpoint": st("76.0", { min: 50, max: 95, step: 0.5, unit_of_measurement: "°F" }),
  },
  entities: {
    "climate.shop": { entity_id: "climate.shop", device_id: "dev1", platform: "esp32_thermostat", translation_key: "thermostat" },
    "sensor.shop_humidity": { entity_id: "sensor.shop_humidity", device_id: "dev1", platform: "esp32_thermostat", translation_key: "humidity" },
    "sensor.shop_outdoor_temperature": { entity_id: "sensor.shop_outdoor_temperature", device_id: "dev1", platform: "esp32_thermostat", translation_key: "outdoor_temperature" },
    "sensor.shop_outdoor_condition": { entity_id: "sensor.shop_outdoor_condition", device_id: "dev1", platform: "esp32_thermostat", translation_key: "outdoor_condition" },
    "switch.shop_shower_mode": { entity_id: "switch.shop_shower_mode", device_id: "dev1", platform: "esp32_thermostat", translation_key: "shower_mode" },
    "switch.shop_schedule": { entity_id: "switch.shop_schedule", device_id: "dev1", platform: "esp32_thermostat", translation_key: "schedule_enabled" },
    "number.shop_cool_setpoint": { entity_id: "number.shop_cool_setpoint", device_id: "dev1", platform: "esp32_thermostat", translation_key: "target_temp_cool" },
    "sensor.other_humidity": { entity_id: "sensor.other_humidity", device_id: "dev2", platform: "esp32_thermostat", translation_key: "humidity" },
  },
  callService: async (...a) => { calls.push(a); },
  callWS: async (msg) => {
    calls.push(["ws", msg]);
    return { response: { enabled: true, days: [0, 1, 2, 3, 4, 5, 6].map((i) => ({ index: i, day: ["sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"][i], enabled: true,
      day_period: { hour: 6, minute: 0, heat: 72, cool: 76, auto: 74, active: true },
      night_period: { hour: 22, minute: 0, heat: 68, cool: 78, auto: 73, active: true } })) } };
  },
});

assert.deepEqual(Card.getStubConfig(mkHass(st("heat", climateAttrs))), { entity: "climate.shop" });
assert.throws(() => new Card().setConfig({}));

const card = new Card();
card.setConfig({ entity: "climate.shop" });
card.connectedCallback();
const hass = mkHass(st("heat", climateAttrs));
card.hass = hass;
assert.equal(card._roles.humidity, "sensor.shop_humidity");
assert.equal(card._roles.shower, "switch.shop_shower_mode");
assert.equal(card._roles.outdoor_condition, "sensor.shop_outdoor_condition");
assert.equal(card._el.target.classList.contains("pending"), false);
assert.equal(card._card.dataset.action, "heating");
assert.match(card._el["sched-text"].textContent, /Temporary hold \(90 min left\)/);
assert.equal(card._el["sched-resume"].hidden, false);
assert.equal(card._el["chip-weather"].lastElementChild.textContent, "41 °F · Clouds");
assert.equal(card._el["shower-row"].hidden, false);

// Same state objects -> no re-render
card._el.title.textContent = "sentinel";
card.hass = { ...hass };
assert.equal(card._el.title.textContent, "sentinel");

// +/- with debounce
card._nudge(1); card._nudge(1);
assert.equal(card._pending, 73);
await new Promise((r) => setTimeout(r, 1100));
assert.deepEqual(calls.at(-1), ["climate", "set_temperature", { entity_id: "climate.shop", temperature: 73 }]);
// device confirms -> pending cleared
card.hass = { ...hass, states: { ...hass.states, "climate.shop": st("heat", { ...climateAttrs, temperature: 73 }) } };
assert.equal(card._pending, null);

// Per-mode setpoint +/- (debounced number.set_value on the discovered entity)
assert.equal(card._roles.sp_cool, "number.shop_cool_setpoint");
assert.equal(card._roles.sp_heat, undefined);
assert.equal(card._el["sched-edit-row"].hidden, false);
card._nudgeSetpoint("sp_cool", 1); card._nudgeSetpoint("sp_cool", 1);
assert.equal(card._spPending.sp_cool, 77);
await new Promise((r) => setTimeout(r, 1100));
assert.deepEqual(calls.at(-1), ["number", "set_value", { entity_id: "number.shop_cool_setpoint", value: 77 }]);

// Drag on the dial: pointer at top-centre = middle of the range
card._dragStart({ clientX: 100, clientY: 20, pointerId: 1, preventDefault() {} }, card._el.dial);
assert.equal(card._pending, 72.5);
card._dragEnd({ pointerId: 1 }, card._el.dial);

// Schedule editor load + save
await card._openEditor();
assert.equal(calls.find((c) => c[0] === "ws")[1].service, "get_schedule");
assert.equal(calls.find((c) => c[0] === "ws")[1].return_response, true);

// Unavailable / missing entity must not throw
card.hass = { ...hass, states: { ...hass.states, "climate.shop": st("unavailable") } };
assert.equal(card._el.warn.hidden, false);
card.hass = { ...hass, states: {} , entities: {} };
const off = new Card();
off.setConfig({ entity: "climate.shop", show_weather: false, show_schedule: false });
off.hass = { ...hass, states: { ...hass.states, "climate.shop": st("off", { ...climateAttrs, temperature: null }) } };
assert.equal(off._el.dial.classList.contains("disabled"), true);
assert.equal(off._el.schedule.hidden, true);

// Fallback discovery via unique_id when translation_key is missing (older HA)
const old = new Card();
old.setConfig({ entity: "climate.shop" });
const oldEntities = Object.fromEntries(Object.entries(hass.entities).map(([k, v]) => [k, { ...v, translation_key: undefined }]));
old.hass = { ...hass, entities: oldEntities, callWS: async () => ({
  "climate.shop": { unique_id: "a1b2_climate" }, "sensor.shop_humidity": { unique_id: "a1b2_humidity" } }) };
await new Promise((r) => setTimeout(r, 10));
assert.equal(old._roles.humidity, "sensor.shop_humidity");

// Editor
const editor = Card.getConfigElement();
editor.setConfig({ entity: "climate.shop" });
editor.hass = hass;
editor.connectedCallback();

console.log("card smoke OK");
process.exit(0);

/*
 * ESP32 Simple Thermostat card for Home Assistant.
 * Bundled with the esp32_thermostat integration; no build step, no external imports.
 */
const CARD_TYPE = "esp32-thermostat-card";
const EDITOR_TYPE = "esp32-thermostat-card-editor";
const DOMAIN = "esp32_thermostat";
const SET_TEMP_DEBOUNCE_MS = 1000;
const PENDING_TIMEOUT_MS = 6000;
const COMPACT_WIDTH = 400;

// role -> [entity domain, translation_key (== unique_id suffix)]
const ROLES = {
  humidity: ["sensor", "humidity"],
  pressure: ["sensor", "pressure"],
  air_quality: ["sensor", "air_quality"],
  outdoor_temperature: ["sensor", "outdoor_temperature"],
  outdoor_condition: ["sensor", "outdoor_condition"],
  supply: ["sensor", "hydronic_supply_temperature"],
  return: ["sensor", "hydronic_return_temperature"],
  active_period: ["sensor", "active_period"],
  shower_remaining: ["sensor", "shower_remaining"],
  shower: ["switch", "shower_mode"],
  schedule: ["switch", "schedule_enabled"],
  resume: ["button", "cancel_override"],
  backup_heat: ["binary_sensor", "backup_heat_active"],
  sp_heat: ["number", "target_temp_heat"],
  sp_cool: ["number", "target_temp_cool"],
  sp_auto: ["number", "target_temp_auto"],
};
const SETPOINTS = [
  ["sp_heat", "heat", "Heat", "mdi:fire"],
  ["sp_cool", "cool", "Cool", "mdi:snowflake"],
  ["sp_auto", "auto", "Auto", "mdi:thermostat-auto"],
];

const DAY_NAMES = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"];
const MODE_LABELS = { off: "Off", heat: "Heat", cool: "Cool", auto: "Auto", heat_cool: "Auto" };
const MODE_ICONS = { off: "mdi:power", heat: "mdi:fire", cool: "mdi:snowflake", auto: "mdi:thermostat-auto", heat_cool: "mdi:thermostat-auto" };
const FAN_LABELS = { auto: "Auto", on: "On", cycle: "Cycle" };
const FAN_ICONS = { auto: "mdi:fan-auto", on: "mdi:fan", cycle: "mdi:fan-clock" };
const ACTION_LABELS = { off: "Off", idle: "Idle", heating: "Heating", cooling: "Cooling", fan: "Fan", drying: "Drying", preheating: "Preheating" };
const PERIOD_LABELS = { day: "Day", night: "Night", manual: "Manual", override: "Override" };

const fireEvent = (node, type, detail) => {
  node.dispatchEvent(new CustomEvent(type, { detail, bubbles: true, composed: true }));
};

const isUnavailable = (stateObj) =>
  !stateObj || stateObj.state === "unavailable" || stateObj.state === "unknown";

const decimals = (step) => {
  const s = String(step);
  return s.includes(".") ? s.split(".")[1].length : 0;
};

const fmt = (value, digits = 1) =>
  value === null || value === undefined || Number.isNaN(Number(value)) ? "–" : Number(value).toFixed(digits);

const polar = (cx, cy, r, deg) => {
  const rad = (deg * Math.PI) / 180;
  return [cx + r * Math.cos(rad), cy + r * Math.sin(rad)];
};

const arcPath = (cx, cy, r, startDeg, endDeg) => {
  if (endDeg - startDeg < 0.01) return "";
  const [x1, y1] = polar(cx, cy, r, startDeg);
  const [x2, y2] = polar(cx, cy, r, endDeg);
  const large = endDeg - startDeg > 180 ? 1 : 0;
  return `M ${x1.toFixed(2)} ${y1.toFixed(2)} A ${r} ${r} 0 ${large} 1 ${x2.toFixed(2)} ${y2.toFixed(2)}`;
};

const DIAL_START = 135;
const DIAL_SWEEP = 270;

/** Resolve the integration's other entities of the climate's device. */
function discoverEntities(hass, climateId, uniqueIds) {
  const result = {};
  const entities = hass && hass.entities;
  if (!entities || !climateId) return result;
  const climate = entities[climateId];
  if (!climate || !climate.device_id) return result;
  const prefix = uniqueIds && uniqueIds[climateId] ? uniqueIds[climateId].replace(/_climate$/, "_") : null;
  for (const entry of Object.values(entities)) {
    if (!entry || entry.device_id !== climate.device_id || entry.entity_id === climateId) continue;
    if (entry.platform && entry.platform !== DOMAIN) continue;
    const domain = entry.entity_id.split(".")[0];
    let key = entry.translation_key;
    if (!key && prefix && uniqueIds[entry.entity_id] && uniqueIds[entry.entity_id].startsWith(prefix)) {
      key = uniqueIds[entry.entity_id].slice(prefix.length);
    }
    if (!key) continue;
    for (const [role, [roleDomain, roleKey]] of Object.entries(ROLES)) {
      if (roleDomain === domain && roleKey === key && !result[role]) result[role] = entry.entity_id;
    }
  }
  return result;
}

const STYLE = `
  :host { display: block; }
  ha-card { padding: 16px; box-sizing: border-box; height: 100%; color: var(--primary-text-color);
    background: var(--ha-card-background, var(--card-background-color, #fff)); overflow: hidden;
    --heat: var(--state-climate-heat-color, #ff8100); --cool: var(--state-climate-cool-color, #2b9af9);
    --fan: var(--state-climate-fan_only-color, #8a8a8a); --idle: var(--state-climate-idle-color, var(--success-color, #43a047));
    --off: var(--state-climate-off-color, var(--disabled-text-color, #9e9e9e)); --accent: var(--off); }
  ha-card[data-action="heating"], ha-card[data-action="preheating"] { --accent: var(--heat); }
  ha-card[data-action="cooling"] { --accent: var(--cool); }
  ha-card[data-action="fan"] { --accent: var(--fan); }
  ha-card[data-action="idle"] { --accent: var(--idle); }
  [hidden] { display: none !important; }
  .header { display: flex; justify-content: space-between; align-items: center; gap: 8px; }
  .title { font-size: 1.2em; font-weight: 500; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
  .warn { color: var(--error-color, #db4437); font-size: .9em; }
  .main { display: flex; flex-direction: column; align-items: center; }
  .dial-wrap { position: relative; width: 100%; max-width: 300px; margin: 4px auto 0; touch-action: none; user-select: none; -webkit-user-select: none; }
  svg { width: 100%; height: auto; display: block; }
  .track { fill: none; stroke: var(--divider-color, rgba(127,127,127,.25)); stroke-width: 14; stroke-linecap: round; }
  .value-arc { fill: none; stroke: var(--accent); stroke-width: 14; stroke-linecap: round; transition: stroke .3s; }
  .cur-dot { fill: var(--primary-text-color); opacity: .7; }
  .handle { fill: var(--card-background-color, #fff); stroke: var(--accent); stroke-width: 4; cursor: grab; }
  .dial-wrap.disabled .handle, .dial-wrap.disabled .value-arc { display: none; }
  .center { position: absolute; inset: 18% 15% 22%; display: flex; flex-direction: column; align-items: center; justify-content: center; pointer-events: none; text-align: center; }
  .action-text { font-size: .95em; color: var(--accent); font-weight: 500; min-height: 1.2em; }
  .target { font-size: 3.2em; font-weight: 400; line-height: 1.1; }
  .target.pending { opacity: .65; font-style: italic; }
  .target .unit { font-size: .4em; vertical-align: super; }
  .current { font-size: .95em; color: var(--secondary-text-color); }
  .stepper { display: flex; justify-content: center; gap: 48px; margin-top: -28px; position: relative; }
  button { font: inherit; color: inherit; cursor: pointer; border: 1px solid var(--divider-color, rgba(127,127,127,.3));
    background: var(--secondary-background-color, rgba(127,127,127,.08)); border-radius: 12px; padding: 6px 10px;
    display: inline-flex; align-items: center; gap: 4px; justify-content: center; min-height: 36px; }
  button:disabled { opacity: .45; cursor: default; }
  button.round { width: 44px; height: 44px; border-radius: 50%; font-size: 1.5em; padding: 0; }
  button.active { background: var(--accent); color: var(--text-primary-color, #fff); border-color: transparent; }
  .modes button.active[data-mode="heat"] { background: var(--heat); }
  .modes button.active[data-mode="cool"] { background: var(--cool); }
  .modes button.active[data-mode="auto"], .modes button.active[data-mode="heat_cool"] { background: var(--state-climate-auto-color, var(--primary-color, #03a9f4)); }
  .modes button.active[data-mode="off"] { background: var(--off); }
  .fans button.active, .shower.active { background: var(--primary-color, #03a9f4); }
  .row { display: flex; flex-wrap: wrap; gap: 8px; justify-content: center; margin-top: 12px; }
  .row.modes button, .row.fans button { flex: 1 1 0; min-width: 60px; }
  .label { font-size: .8em; color: var(--secondary-text-color); width: 100%; text-align: center; margin-bottom: -4px; }
  .status { display: flex; flex-wrap: wrap; gap: 6px 16px; justify-content: center; font-size: .9em; color: var(--secondary-text-color); margin-top: 12px; }
  .status b { color: var(--primary-text-color); font-weight: 500; }
  .schedule { display: flex; flex-wrap: wrap; align-items: center; gap: 8px; margin-top: 12px; padding: 8px 10px; border-radius: 12px;
    background: var(--secondary-background-color, rgba(127,127,127,.08)); font-size: .9em; }
  .schedule .grow { flex: 1 1 auto; }
  .setpoints { display: grid; grid-template-columns: repeat(auto-fit, minmax(96px, 1fr)); gap: 8px; margin-top: 12px; }
  .sp { display: flex; flex-direction: column; align-items: center; gap: 2px; padding: 6px 4px; border-radius: 12px;
    border: 1px solid var(--divider-color, rgba(127,127,127,.3)); }
  .sp.current { border-color: transparent; background: var(--secondary-background-color, rgba(127,127,127,.12)); }
  .sp[data-mode="heat"] ha-icon { color: var(--heat); }
  .sp[data-mode="cool"] ha-icon { color: var(--cool); }
  .sp .sp-name { font-size: .8em; color: var(--secondary-text-color); display: flex; align-items: center; gap: 2px; }
  .sp .sp-ctl { display: flex; align-items: center; gap: 6px; }
  .sp .sp-val { font-size: 1.15em; min-width: 3.2em; text-align: center; }
  .sp .sp-val.pending { opacity: .65; font-style: italic; }
  .sp button { width: 30px; height: 30px; min-height: 30px; border-radius: 50%; padding: 0; }
  .sched-edit-row { margin-top: 8px; }
  .sched-edit-row button { width: 100%; }
  .dialog .hint { font-size: .85em; color: var(--secondary-text-color); padding: 8px 16px 0; }
  .chips { display: flex; flex-wrap: wrap; gap: 6px; justify-content: center; margin-top: 12px; }
  .chip { display: inline-flex; align-items: center; gap: 4px; padding: 4px 10px; border-radius: 16px; font-size: .85em;
    border: 1px solid var(--divider-color, rgba(127,127,127,.3)); }
  ha-icon { --mdc-icon-size: 18px; }
  .compact .target { font-size: 2.5em; }
  .compact .stepper { gap: 24px; }
  .compact .row.modes button span, .compact .row.fans button span { display: none; }
  /* schedule editor */
  .overlay { position: fixed; inset: 0; background: rgba(0,0,0,.5); z-index: 10; display: flex; align-items: center; justify-content: center; }
  .dialog { background: var(--card-background-color, #fff); color: var(--primary-text-color); border-radius: 16px; width: min(760px, 96vw);
    max-height: 90vh; display: flex; flex-direction: column; box-shadow: 0 8px 32px rgba(0,0,0,.4); }
  .dialog header, .dialog footer { padding: 12px 16px; display: flex; gap: 8px; align-items: center; }
  .dialog header { justify-content: space-between; font-size: 1.15em; font-weight: 500; border-bottom: 1px solid var(--divider-color, #ddd); }
  .dialog footer { justify-content: flex-end; border-top: 1px solid var(--divider-color, #ddd); flex-wrap: wrap; }
  .dialog .body { overflow: auto; padding: 8px 16px; }
  .day { border-bottom: 1px solid var(--divider-color, #eee); padding: 8px 0; }
  .day-head { display: flex; align-items: center; gap: 8px; font-weight: 500; }
  .day-head .grow { flex: 1; }
  .period { display: grid; grid-template-columns: 60px 96px repeat(3, minmax(60px, 1fr)) 70px; gap: 6px; align-items: center; margin-top: 6px; font-size: .9em; }
  .period input[type=number], .period input[type=time] { width: 100%; box-sizing: border-box; padding: 4px; font: inherit; border-radius: 6px;
    border: 1px solid var(--divider-color, #ccc); background: var(--card-background-color, #fff); color: var(--primary-text-color); }
  .period input.invalid { border-color: var(--error-color, #db4437); outline: 1px solid var(--error-color, #db4437); }
  .period .hdr { font-size: .75em; color: var(--secondary-text-color); }
  .dlg-error { color: var(--error-color, #db4437); flex: 1 1 100%; }
  .primary { background: var(--primary-color, #03a9f4); color: var(--text-primary-color, #fff); border-color: transparent; }
  @media (max-width: 500px) { .period { grid-template-columns: 1fr 1fr 1fr; } .period .hdr { display: none; } }
`;

class Esp32ThermostatCard extends HTMLElement {
  constructor() {
    super();
    this._config = null;
    this._hass = null;
    this._built = false;
    this._roles = {};
    this._uniqueIds = null;
    this._entitiesRef = null;
    this._watchRefs = [];
    this._pending = null;
    this._pendingTimer = null;
    this._pendingClear = null;
    this._dragging = false;
    this._listSig = "";
    this._dialog = null;
    this.attachShadow({ mode: "open" });
  }

  static getConfigElement() {
    return document.createElement(EDITOR_TYPE);
  }

  static getStubConfig(hass) {
    const states = (hass && hass.states) || {};
    const climates = Object.keys(states).filter((id) => id.startsWith("climate."));
    const own = climates.find((id) => hass.entities && hass.entities[id] && hass.entities[id].platform === DOMAIN);
    return { entity: own || climates[0] || "" };
  }

  setConfig(config) {
    if (!config || typeof config !== "object") throw new Error("Invalid configuration");
    if (!config.entity || !String(config.entity).startsWith("climate.")) {
      throw new Error("Please define a climate entity ('entity: climate.xxx')");
    }
    this._config = {
      show_status: true,
      show_setpoints: true,
      show_schedule: true,
      show_sensors: true,
      show_weather: true,
      ...config,
    };
    this._entitiesRef = null;
    this._watchRefs = [];
    if (this._hass) this._refresh(true);
  }

  set hass(hass) {
    this._hass = hass;
    if (this._config) this._refresh(false);
  }

  get hass() {
    return this._hass;
  }

  getCardSize() {
    return 8;
  }

  getGridOptions() {
    return { columns: 12, min_columns: 6, rows: "auto" };
  }

  connectedCallback() {
    if (typeof ResizeObserver !== "undefined" && !this._ro) {
      this._ro = new ResizeObserver((entries) => {
        const width = entries[0] && entries[0].contentRect ? entries[0].contentRect.width : 0;
        if (this._card) this._card.classList.toggle("compact", width > 0 && width < COMPACT_WIDTH);
      });
    }
    if (this._ro) this._ro.observe(this);
  }

  disconnectedCallback() {
    if (this._ro) this._ro.disconnect();
  }

  // ------------------------------------------------------------ helpers
  _state(role) {
    const id = role === "climate" ? this._config.entity : this._roles[role];
    return id && this._hass ? this._hass.states[id] : undefined;
  }

  _deviceId() {
    const entry = this._hass && this._hass.entities && this._hass.entities[this._config.entity];
    return entry ? entry.device_id : undefined;
  }

  _watchedIds() {
    return [this._config.entity, ...Object.values(this._roles)];
  }

  _discover() {
    const hass = this._hass;
    this._roles = discoverEntities(hass, this._config.entity, this._uniqueIds);
    const entry = hass.entities && hass.entities[this._config.entity];
    const needsFallback = entry && entry.device_id &&
      Object.values(hass.entities).some((e) => e.device_id === entry.device_id && !e.translation_key && e.entity_id !== this._config.entity);
    if (needsFallback && this._uniqueIds === null && typeof hass.callWS === "function") {
      // Older HA without translation_key in hass.entities: fetch unique_ids instead.
      this._uniqueIds = {};
      const ids = Object.values(hass.entities).filter((e) => e.device_id === entry.device_id).map((e) => e.entity_id);
      hass.callWS({ type: "config/entity_registry/get_entries", entity_ids: ids })
        .then((res) => {
          const map = {};
          for (const [id, e] of Object.entries(res || {})) if (e && e.unique_id) map[id] = e.unique_id;
          this._uniqueIds = map;
          this._entitiesRef = null;
          if (this._hass) this._refresh(true);
        })
        .catch(() => {});
    }
  }

  _refresh(force) {
    try {
      const hass = this._hass;
      if (!hass || !this._config) return;
      if (hass.entities !== this._entitiesRef) {
        this._entitiesRef = hass.entities;
        this._discover();
        force = true;
      }
      const refs = this._watchedIds().map((id) => hass.states[id]);
      if (!force && refs.length === this._watchRefs.length && refs.every((r, i) => r === this._watchRefs[i])) return;
      this._watchRefs = refs;
      if (!this._built) this._build();
      this._update();
    } catch (err) {
      // Never break the dashboard; log for debugging.
      console.error(`${CARD_TYPE}:`, err);
    }
  }

  _call(domain, service, data) {
    if (!this._hass) return Promise.resolve();
    return Promise.resolve(this._hass.callService(domain, service, data)).catch((err) => this._toast(err));
  }

  _toast(err) {
    const message = (err && (err.message || err.error)) || String(err);
    fireEvent(this, "hass-notification", { message });
  }

  // ------------------------------------------------------------- build
  _build() {
    const root = this.shadowRoot;
    root.innerHTML = `
      <style>${STYLE}</style>
      <ha-card>
        <div class="header"><div class="title" id="title"></div><div class="warn" id="warn" hidden></div></div>
        <div class="main">
          <div class="dial-wrap" id="dial">
            <svg viewBox="0 0 200 200" role="slider" aria-label="Target temperature">
              <path class="track" id="track"></path>
              <path class="value-arc" id="arc"></path>
              <circle class="cur-dot" id="cur" r="4"></circle>
              <circle class="handle" id="handle" r="11"></circle>
            </svg>
            <div class="center">
              <div class="action-text" id="action"></div>
              <div class="target" id="target"></div>
              <div class="current" id="current"></div>
            </div>
          </div>
          <div class="stepper">
            <button class="round" id="minus" aria-label="Decrease">−</button>
            <button class="round" id="plus" aria-label="Increase">+</button>
          </div>
        </div>
        <div class="row modes" id="modes"></div>
        <div class="row fans" id="fans"></div>
        <div class="row" id="shower-row"><button class="shower" id="shower"><ha-icon icon="mdi:shower-head"></ha-icon><span id="shower-text">Shower</span></button></div>
        <div class="setpoints" id="setpoints"></div>
        <div class="status" id="status">
          <span id="st-action"></span><span id="st-stage"></span><span id="st-supply"></span><span id="st-return"></span>
        </div>
        <div class="schedule" id="schedule">
          <ha-icon icon="mdi:calendar-clock"></ha-icon>
          <span class="grow" id="sched-text"></span>
          <button id="sched-toggle"></button>
          <button id="sched-resume"><ha-icon icon="mdi:calendar-sync"></ha-icon>Resume</button>
        </div>
        <div class="sched-edit-row" id="sched-edit-row">
          <button id="sched-edit"><ha-icon icon="mdi:calendar-edit"></ha-icon><span>Edit weekly schedule</span></button>
        </div>
        <div class="chips" id="chips">
          <span class="chip" id="chip-humidity"><ha-icon icon="mdi:water-percent"></ha-icon><span></span></span>
          <span class="chip" id="chip-aq"><ha-icon icon="mdi:air-filter"></ha-icon><span></span></span>
          <span class="chip" id="chip-pressure"><ha-icon icon="mdi:gauge"></ha-icon><span></span></span>
          <span class="chip" id="chip-weather"><ha-icon icon="mdi:weather-partly-cloudy"></ha-icon><span></span></span>
        </div>
      </ha-card>`;
    const $ = (id) => root.getElementById(id);
    this._card = root.querySelector("ha-card");
    this._el = {};
    for (const id of ["title", "warn", "dial", "track", "arc", "cur", "handle", "action", "target", "current", "minus", "plus",
      "modes", "fans", "shower-row", "shower", "shower-text", "status", "st-action", "st-stage", "st-supply", "st-return",
      "schedule", "sched-text", "sched-toggle", "sched-resume", "sched-edit", "sched-edit-row", "setpoints", "chips", "chip-humidity", "chip-aq",
      "chip-pressure", "chip-weather"]) {
      this._el[id] = $(id);
    }
    this._el.track.setAttribute("d", arcPath(100, 100, 80, DIAL_START, DIAL_START + DIAL_SWEEP));
    this._el.minus.addEventListener("click", () => this._nudge(-1));
    this._el.plus.addEventListener("click", () => this._nudge(1));
    this._el.shower.addEventListener("click", () => this._toggleShower());
    this._el["sched-toggle"].addEventListener("click", () => this._toggleSchedule());
    this._el["sched-resume"].addEventListener("click", () => this._resume());
    this._el["sched-edit"].addEventListener("click", () => this._openEditor());
    this._el.setpoints.addEventListener("click", (ev) => {
      const btn = ev.target.closest && ev.target.closest("button[data-sp]");
      if (btn && !btn.disabled) this._nudgeSetpoint(btn.dataset.sp, Number(btn.dataset.dir));
    });
    this._spPending = {};
    this._spTimers = {};
    this._spSig = "";
    this._el.modes.addEventListener("click", (ev) => {
      const btn = ev.target.closest && ev.target.closest("button[data-mode]");
      if (btn && !btn.disabled) this._call("climate", "set_hvac_mode", { entity_id: this._config.entity, hvac_mode: btn.dataset.mode });
    });
    this._el.fans.addEventListener("click", (ev) => {
      const btn = ev.target.closest && ev.target.closest("button[data-fan]");
      if (btn && !btn.disabled) this._call("climate", "set_fan_mode", { entity_id: this._config.entity, fan_mode: btn.dataset.fan });
    });
    const svg = root.querySelector("svg");
    svg.addEventListener("pointerdown", (ev) => this._dragStart(ev, svg));
    svg.addEventListener("pointermove", (ev) => this._dragMove(ev, svg));
    svg.addEventListener("pointerup", (ev) => this._dragEnd(ev, svg));
    svg.addEventListener("pointercancel", (ev) => this._dragEnd(ev, svg));
    this._listSig = "";
    this._built = true;
  }

  // ------------------------------------------------------------ update
  _limits(attrs) {
    const min = Number(attrs.min_temp ?? 7);
    const max = Number(attrs.max_temp ?? 35);
    const step = Number(attrs.target_temp_step) || 0.5;
    return { min, max: max > min ? max : min + 1, step };
  }

  _unit() {
    const cfg = this._hass && this._hass.config;
    return (cfg && cfg.unit_system && cfg.unit_system.temperature) || "°";
  }

  _update() {
    const el = this._el;
    const cfg = this._config;
    const climate = this._state("climate");
    const unavailable = isUnavailable(climate);
    const attrs = (climate && climate.attributes) || {};
    const mode = climate ? climate.state : "unavailable";
    const action = unavailable ? "off" : attrs.hvac_action || (mode === "off" ? "off" : "idle");
    this._card.dataset.action = action;

    el.title.textContent = cfg.name || attrs.friendly_name || cfg.entity;
    el.warn.hidden = !unavailable;
    el.warn.textContent = climate ? "Unavailable" : "Entity not found";

    const { step } = this._limits(attrs);
    const digits = decimals(step);
    const unit = this._unit();
    el.current.textContent = unavailable ? "" : `Currently ${fmt(attrs.current_temperature, 1)} ${unit}`;
    el.action.textContent = unavailable ? "" : ACTION_LABELS[action] || action;

    // Clear pending once the device reports it (or never will).
    if (this._pending !== null && !this._pendingTimer && attrs.temperature !== undefined &&
        Math.abs(Number(attrs.temperature) - this._pending) < step / 2) {
      this._clearPending();
    }
    const dialEnabled = !unavailable && mode !== "off" && attrs.temperature !== null && attrs.temperature !== undefined;
    el.dial.classList.toggle("disabled", !dialEnabled);
    el.minus.disabled = el.plus.disabled = !dialEnabled;
    if (!this._dragging) this._renderDial(dialEnabled ? (this._pending ?? Number(attrs.temperature)) : null, attrs, digits, unit, mode, unavailable);

    // mode / fan buttons (rebuilt only if the lists change)
    const modes = unavailable ? [] : attrs.hvac_modes || [];
    const fans = unavailable ? [] : attrs.fan_modes || [];
    const sig = JSON.stringify([modes, fans]);
    if (sig !== this._listSig) {
      this._listSig = sig;
      el.modes.innerHTML = modes.map((m) =>
        `<button data-mode="${m}" title="${MODE_LABELS[m] || m}"><ha-icon icon="${MODE_ICONS[m] || "mdi:thermostat"}"></ha-icon><span>${MODE_LABELS[m] || m}</span></button>`).join("");
      el.fans.innerHTML = fans.length ? `<div class="label">Fan</div>` + fans.map((f) =>
        `<button data-fan="${f}" title="Fan ${FAN_LABELS[f] || f}"><ha-icon icon="${FAN_ICONS[f] || "mdi:fan"}"></ha-icon><span>${FAN_LABELS[f] || f}</span></button>`).join("") : "";
    }
    for (const btn of el.modes.querySelectorAll("button")) btn.classList.toggle("active", btn.dataset.mode === mode);
    for (const btn of el.fans.querySelectorAll("button")) btn.classList.toggle("active", btn.dataset.fan === attrs.fan_mode);
    el.modes.hidden = modes.length === 0;
    el.fans.hidden = fans.length === 0;

    // shower
    const shower = this._state("shower");
    el["shower-row"].hidden = isUnavailable(shower);
    if (!isUnavailable(shower)) {
      const on = shower.state === "on";
      el.shower.classList.toggle("active", on);
      const remaining = this._state("shower_remaining");
      el["shower-text"].textContent = on
        ? `Shower on${!isUnavailable(remaining) ? ` · ${remaining.state} min` : ""}`
        : "Shower mode";
    }

    // status row
    el.status.hidden = !cfg.show_status || unavailable;
    if (!el.status.hidden) {
      el["st-action"].innerHTML = "";
      el["st-action"].append("Status: ", this._bold(ACTION_LABELS[action] || action));
      const backup = this._state("backup_heat");
      let stage = "";
      if (attrs.backup_heat_active || (backup && backup.state === "on")) stage = "Backup heat";
      else if (attrs.stage2_active) stage = "Stage 2";
      else if (attrs.stage1_active) stage = "Stage 1";
      el["st-stage"].hidden = !stage;
      el["st-stage"].innerHTML = "";
      if (stage) el["st-stage"].append("Stage: ", this._bold(stage));
      this._sensorText(el["st-supply"], "supply", "Supply");
      this._sensorText(el["st-return"], "return", "Return");
    }

    // schedule strip
    const schedSwitch = this._state("schedule");
    const schedEnabled = attrs.schedule_enabled ?? (schedSwitch ? schedSwitch.state === "on" : undefined);
    el.schedule.hidden = !cfg.show_schedule || unavailable;
    if (!el.schedule.hidden) {
      const periodState = this._state("active_period");
      const period = attrs.active_period ?? (isUnavailable(periodState) ? undefined : periodState.state);
      const overrideType = attrs.override_type ??
        (attrs.preset_mode === "temporary_hold" ? "temporary" : attrs.preset_mode === "permanent_hold" ? "permanent" : "none");
      let text;
      if (schedEnabled === false) text = "Schedule off";
      else {
        text = `Schedule · ${PERIOD_LABELS[period] || period || "–"}`;
        if (overrideType === "temporary") {
          const rem = attrs.override_remaining_min;
          text += ` · Temporary hold${rem !== null && rem !== undefined ? ` (${rem} min left)` : ""}`;
        } else if (overrideType === "permanent") text += " · Permanent hold";
      }
      el["sched-text"].textContent = text;
      el["sched-toggle"].textContent = schedEnabled === false ? "Enable" : "Disable";
      el["sched-toggle"].disabled = schedEnabled === undefined;
      el["sched-resume"].hidden = !(schedEnabled && overrideType && overrideType !== "none");
    }
    el["sched-edit-row"].hidden = !cfg.show_schedule || unavailable || !this._deviceId();

    this._renderSetpoints(mode, unavailable);

    // chips
    el.chips.hidden = !(cfg.show_sensors || cfg.show_weather);
    const humidity = this._state("humidity");
    const humValue = !isUnavailable(humidity) ? `${fmt(humidity.state, 0)} %`
      : attrs.current_humidity !== undefined && attrs.current_humidity !== null && !unavailable ? `${fmt(attrs.current_humidity, 0)} %` : null;
    this._chip("chip-humidity", cfg.show_sensors && humValue);
    this._chip("chip-aq", cfg.show_sensors && this._stateText("air_quality", 0, "AQI "));
    this._chip("chip-pressure", cfg.show_sensors && this._stateText("pressure", 0));
    const ot = this._state("outdoor_temperature");
    const oc = this._state("outdoor_condition");
    let weather = null;
    if (!isUnavailable(ot) || !isUnavailable(oc)) {
      weather = [!isUnavailable(ot) ? `${fmt(ot.state, 0)} ${ot.attributes.unit_of_measurement || ""}`.trim() : "",
        !isUnavailable(oc) ? oc.state : ""].filter(Boolean).join(" · ");
    }
    this._chip("chip-weather", cfg.show_weather && weather);
    el.chips.hidden = el.chips.hidden || [...el.chips.children].every((c) => c.hidden);
  }

  _bold(text) {
    const b = document.createElement("b");
    b.textContent = text;
    return b;
  }

  _stateText(role, digits, prefix = "") {
    const s = this._state(role);
    if (isUnavailable(s)) return null;
    const num = Number(s.state);
    const val = Number.isNaN(num) ? s.state : fmt(num, digits);
    return `${prefix}${val} ${s.attributes.unit_of_measurement || ""}`.trim();
  }

  _sensorText(node, role, label) {
    const text = this._stateText(role, 1);
    node.hidden = !text;
    node.innerHTML = "";
    if (text) node.append(`${label}: `, this._bold(text));
  }

  _chip(id, text) {
    const chip = this._el[id];
    chip.hidden = !text;
    if (text) chip.lastElementChild.textContent = text;
  }

  _renderDial(value, attrs, digits, unit, mode, unavailable) {
    const el = this._el;
    const { min, max } = this._limits(attrs);
    const frac = (v) => Math.min(1, Math.max(0, (v - min) / (max - min)));
    const cur = Number(attrs.current_temperature);
    if (!unavailable && !Number.isNaN(cur) && attrs.current_temperature !== null) {
      const [cx, cy] = polar(100, 100, 80, DIAL_START + frac(cur) * DIAL_SWEEP);
      el.cur.setAttribute("cx", cx.toFixed(2));
      el.cur.setAttribute("cy", cy.toFixed(2));
      el.cur.style.display = "";
    } else {
      el.cur.style.display = "none";
    }
    if (value === null || Number.isNaN(value)) {
      el.arc.setAttribute("d", "");
      el.target.classList.remove("pending");
      el.target.textContent = unavailable ? "–" : mode === "off" ? "Off" : "–";
      return;
    }
    const end = DIAL_START + frac(value) * DIAL_SWEEP;
    el.arc.setAttribute("d", arcPath(100, 100, 80, DIAL_START, end));
    const [hx, hy] = polar(100, 100, 80, end);
    el.handle.setAttribute("cx", hx.toFixed(2));
    el.handle.setAttribute("cy", hy.toFixed(2));
    el.target.classList.toggle("pending", this._pending !== null);
    el.target.innerHTML = "";
    el.target.append(fmt(value, digits));
    const u = document.createElement("span");
    u.className = "unit";
    u.textContent = unit;
    el.target.append(u);
  }

  // ------------------------------------------------------- target temp
  _setPending(value) {
    const attrs = (this._state("climate") || {}).attributes || {};
    const { min, max, step } = this._limits(attrs);
    let v = Math.round(value / step) * step;
    v = Math.min(max, Math.max(min, v));
    v = Number(v.toFixed(decimals(step)));
    this._pending = v;
    clearTimeout(this._pendingTimer);
    clearTimeout(this._pendingClear);
    this._pendingTimer = setTimeout(() => this._commit(), SET_TEMP_DEBOUNCE_MS);
    this._renderDial(v, attrs, decimals(step), this._unit(), (this._state("climate") || {}).state, false);
  }

  _commit() {
    this._pendingTimer = null;
    const value = this._pending;
    if (value === null) return;
    Promise.resolve(this._hass.callService("climate", "set_temperature", { entity_id: this._config.entity, temperature: value }))
      .then(() => {
        // Keep showing the pending value until the device confirms it (or give up).
        this._pendingClear = setTimeout(() => this._clearPending(true), PENDING_TIMEOUT_MS);
        this._refresh(true);
      })
      .catch((err) => {
        this._toast(err);
        if (this._pending === value && !this._pendingTimer) this._clearPending(true);
      });
  }

  _clearPending(rerender) {
    clearTimeout(this._pendingClear);
    this._pending = null;
    if (rerender) this._refresh(true);
  }

  _nudge(dir) {
    const climate = this._state("climate");
    if (isUnavailable(climate)) return;
    const attrs = climate.attributes;
    const { step } = this._limits(attrs);
    const base = this._pending ?? Number(attrs.temperature);
    if (Number.isNaN(base)) return;
    this._setPending(base + dir * step);
  }

  _valueFromPointer(ev, svg) {
    const rect = svg.getBoundingClientRect();
    if (!rect.width) return null;
    const x = ((ev.clientX - rect.left) / rect.width) * 200 - 100;
    const y = ((ev.clientY - rect.top) / rect.height) * 200 - 100;
    let rel = ((Math.atan2(y, x) * 180) / Math.PI - DIAL_START + 720) % 360;
    if (rel > DIAL_SWEEP) rel = rel > DIAL_SWEEP + (360 - DIAL_SWEEP) / 2 ? 0 : DIAL_SWEEP;
    const { min, max } = this._limits(this._state("climate").attributes);
    return min + (rel / DIAL_SWEEP) * (max - min);
  }

  _dragStart(ev, svg) {
    if (this._el.dial.classList.contains("disabled")) return;
    this._dragging = true;
    try { svg.setPointerCapture(ev.pointerId); } catch (_e) { /* ignore */ }
    ev.preventDefault();
    this._dragMove(ev, svg);
  }

  _dragMove(ev, svg) {
    if (!this._dragging) return;
    const v = this._valueFromPointer(ev, svg);
    if (v !== null) this._setPending(v);
  }

  _dragEnd(ev, svg) {
    if (!this._dragging) return;
    this._dragging = false;
    try { svg.releasePointerCapture(ev.pointerId); } catch (_e) { /* ignore */ }
    this._refresh(true);
  }

  // ------------------------------------------------------- other actions
  _toggleShower() {
    const id = this._roles.shower;
    const s = this._state("shower");
    if (!id || isUnavailable(s)) return;
    this._call("switch", s.state === "on" ? "turn_off" : "turn_on", { entity_id: id });
  }

  // --------------------------------------------------- per-mode setpoints
  _renderSetpoints(mode, unavailable) {
    const el = this._el.setpoints;
    const shown = SETPOINTS.filter(([role]) => !isUnavailable(this._state(role)));
    el.hidden = !this._config.show_setpoints || unavailable || shown.length === 0;
    if (el.hidden) return;
    const sig = shown.map(([role]) => role).join(",");
    if (sig !== this._spSig) {
      this._spSig = sig;
      el.innerHTML = shown.map(([role, m, label, icon]) => `
        <div class="sp" data-role="${role}" data-mode="${m}">
          <span class="sp-name"><ha-icon icon="${icon}"></ha-icon>${label}</span>
          <span class="sp-ctl">
            <button data-sp="${role}" data-dir="-1" aria-label="Lower ${label} setpoint">−</button>
            <span class="sp-val"></span>
            <button data-sp="${role}" data-dir="1" aria-label="Raise ${label} setpoint">+</button>
          </span>
        </div>`).join("");
    }
    const activeMode = mode === "heat_cool" ? "auto" : mode;
    for (const box of el.querySelectorAll(".sp")) {
      const role = box.dataset.role;
      const st = this._state(role);
      const pending = this._spPending[role];
      if (pending !== undefined && !this._spTimers[role] && Math.abs(Number(st.state) - pending) < 0.01) delete this._spPending[role];
      const value = this._spPending[role] ?? Number(st.state);
      const unit = st.attributes.unit_of_measurement || "";
      const val = box.querySelector(".sp-val");
      val.textContent = `${fmt(value, decimals(Number(st.attributes.step) || 0.5))}${unit}`;
      val.classList.toggle("pending", this._spPending[role] !== undefined);
      box.classList.toggle("current", box.dataset.mode === activeMode);
      const [minus, plus] = box.querySelectorAll("button");
      minus.disabled = value <= Number(st.attributes.min);
      plus.disabled = value >= Number(st.attributes.max);
    }
  }

  _nudgeSetpoint(role, dir) {
    const st = this._state(role);
    if (isUnavailable(st)) return;
    const step = Number(st.attributes.step) || 0.5;
    const base = this._spPending[role] ?? Number(st.state);
    const next = Math.min(Number(st.attributes.max), Math.max(Number(st.attributes.min), Math.round((base + dir * step) / step) * step));
    this._spPending[role] = next;
    clearTimeout(this._spTimers[role]);
    this._spTimers[role] = setTimeout(() => {
      this._spTimers[role] = null;
      this._call("number", "set_value", { entity_id: this._roles[role], value: this._spPending[role] });
      // Drop the pending value if the device never confirms it.
      setTimeout(() => { if (!this._spTimers[role]) { delete this._spPending[role]; this._refresh(true); } }, PENDING_TIMEOUT_MS);
    }, SET_TEMP_DEBOUNCE_MS);
    this._refresh(true);
  }

  _toggleSchedule() {
    const attrs = (this._state("climate") || {}).attributes || {};
    const sw = this._state("schedule");
    const enabled = attrs.schedule_enabled ?? (sw ? sw.state === "on" : false);
    if (this._roles.schedule) {
      this._call("switch", enabled ? "turn_off" : "turn_on", { entity_id: this._roles.schedule });
    } else {
      this._call("climate", "set_preset_mode", { entity_id: this._config.entity, preset_mode: enabled ? "none" : "schedule" });
    }
  }

  _resume() {
    if (this._roles.resume && !isUnavailable(this._state("resume"))) {
      this._call("button", "press", { entity_id: this._roles.resume });
    } else if (this._deviceId()) {
      this._call(DOMAIN, "set_override", { device_id: this._deviceId(), mode: "resume" });
    }
  }

  // ----------------------------------------------------- schedule editor
  async _openEditor() {
    const deviceId = this._deviceId();
    if (!deviceId || !this._hass) return;
    this._closeEditor();
    const overlay = document.createElement("div");
    overlay.className = "overlay";
    overlay.innerHTML = `
      <div class="dialog" role="dialog" aria-modal="true" aria-label="Weekly schedule">
        <header><span>Weekly schedule</span><label><input type="checkbox" id="dlg-enabled"> Schedule enabled</label></header>
        <div class="hint">Each day has a Day and a Night period. The start time applies to every mode; Heat, Cool and Auto each have their own temperature for that period.</div>
        <div class="body" id="dlg-body">Loading…</div>
        <footer><span class="dlg-error" id="dlg-error" hidden></span>
          <button id="dlg-cancel">Cancel</button><button class="primary" id="dlg-save" disabled>Save</button></footer>
      </div>`;
    overlay.addEventListener("click", (ev) => { if (ev.target === overlay) this._closeEditor(); });
    overlay.addEventListener("keydown", (ev) => { if (ev.key === "Escape") this._closeEditor(); });
    overlay.tabIndex = -1;
    this.shadowRoot.appendChild(overlay);
    this._dialog = overlay;
    overlay.focus();
    const q = (id) => overlay.querySelector(`#${id}`);
    q("dlg-cancel").addEventListener("click", () => this._closeEditor());
    q("dlg-save").addEventListener("click", () => this._saveSchedule(deviceId));
    try {
      const res = await this._hass.callWS({
        type: "call_service", domain: DOMAIN, service: "get_schedule",
        service_data: { device_id: deviceId }, return_response: true,
      });
      const schedule = res && (res.response || res);
      if (!schedule || !Array.isArray(schedule.days)) throw new Error("Unexpected schedule response");
      if (this._dialog !== overlay) return;
      this._renderSchedule(overlay, schedule);
    } catch (err) {
      if (this._dialog !== overlay) return;
      q("dlg-body").textContent = "";
      this._dlgError(`Could not load schedule: ${(err && err.message) || err}`);
    }
  }

  _closeEditor() {
    if (this._dialog) this._dialog.remove();
    this._dialog = null;
  }

  _dlgError(message) {
    if (!this._dialog) return;
    const e = this._dialog.querySelector("#dlg-error");
    e.hidden = !message;
    e.textContent = message || "";
  }

  _renderSchedule(overlay, schedule) {
    const attrs = (this._state("climate") || {}).attributes || {};
    const { min, max, step } = this._limits(attrs);
    overlay.querySelector("#dlg-enabled").checked = !!schedule.enabled;
    const body = overlay.querySelector("#dlg-body");
    body.innerHTML = "";
    const pad = (n) => String(n ?? 0).padStart(2, "0");
    const days = [...schedule.days].sort((a, b) => (a.index ?? 0) - (b.index ?? 0));
    for (const day of days) {
      const idx = day.index ?? DAY_NAMES.findIndex((n) => n.toLowerCase() === day.day);
      const wrap = document.createElement("div");
      wrap.className = "day";
      wrap.dataset.index = idx;
      wrap.dataset.day = day.day || DAY_NAMES[idx].toLowerCase();
      const head = document.createElement("div");
      head.className = "day-head";
      head.innerHTML = `<label class="grow"><input type="checkbox" data-f="enabled"> <span></span></label>
        <button data-copy title="Copy this day to all days">Copy to all</button>`;
      head.querySelector("span").textContent = DAY_NAMES[idx] || day.day;
      head.querySelector("input").checked = !!day.enabled;
      wrap.appendChild(head);
      for (const [key, label] of [["day_period", "Day"], ["night_period", "Night"]]) {
        const p = day[key] || {};
        const row = document.createElement("div");
        row.className = "period";
        row.dataset.period = key;
        row.innerHTML = `
          <span>${label}</span>
          <input type="time" data-f="time" aria-label="${label} start">
          <input type="number" data-f="heat" step="${step}" min="${min}" max="${max}" aria-label="${label} heat" title="Heat">
          <input type="number" data-f="cool" step="${step}" min="${min}" max="${max}" aria-label="${label} cool" title="Cool">
          <input type="number" data-f="auto" step="${step}" min="${min}" max="${max}" aria-label="${label} auto" title="Auto">
          <label><input type="checkbox" data-f="active"> on</label>`;
        row.querySelector('[data-f="time"]').value = `${pad(p.hour)}:${pad(p.minute)}`;
        for (const f of ["heat", "cool", "auto"]) row.querySelector(`[data-f="${f}"]`).value = p[f] ?? "";
        row.querySelector('[data-f="active"]').checked = p.active !== false;
        wrap.appendChild(row);
      }
      body.appendChild(wrap);
    }
    const hdr = document.createElement("div");
    hdr.className = "period";
    hdr.innerHTML = `<span class="hdr"></span><span class="hdr">Start</span><span class="hdr">Heat</span><span class="hdr">Cool</span><span class="hdr">Auto</span><span class="hdr">Active</span>`;
    body.prepend(hdr);
    body.addEventListener("input", () => this._validateSchedule());
    body.addEventListener("click", (ev) => {
      const btn = ev.target.closest && ev.target.closest("button[data-copy]");
      if (!btn) return;
      const src = btn.closest(".day");
      for (const dst of body.querySelectorAll(".day")) {
        if (dst === src) continue;
        const srcInputs = src.querySelectorAll("input");
        dst.querySelectorAll("input").forEach((input, i) => {
          if (input.type === "checkbox") input.checked = srcInputs[i].checked;
          else input.value = srcInputs[i].value;
        });
      }
      this._validateSchedule();
    });
    this._validateSchedule();
  }

  _validateSchedule() {
    const overlay = this._dialog;
    if (!overlay) return false;
    const attrs = (this._state("climate") || {}).attributes || {};
    const { min, max } = this._limits(attrs);
    let ok = true;
    for (const input of overlay.querySelectorAll('.period input[type="number"], .period input[type="time"]')) {
      let valid;
      if (input.type === "time") valid = /^\d{2}:\d{2}/.test(input.value);
      else {
        const v = Number(input.value);
        valid = input.value !== "" && !Number.isNaN(v) && v >= min && v <= max;
      }
      input.classList.toggle("invalid", !valid);
      ok = ok && valid;
    }
    this._dlgError(ok ? "" : `Temperatures must be between ${min} and ${max}; times must be set.`);
    overlay.querySelector("#dlg-save").disabled = !ok;
    return ok;
  }

  async _saveSchedule(deviceId) {
    const overlay = this._dialog;
    if (!overlay || !this._validateSchedule()) return;
    const days = [...overlay.querySelectorAll(".day")].map((wrap) => {
      const day = { day: wrap.dataset.day, enabled: wrap.querySelector('[data-f="enabled"]').checked };
      for (const row of wrap.querySelectorAll(".period")) {
        const [h, m] = row.querySelector('[data-f="time"]').value.split(":").map(Number);
        day[row.dataset.period] = {
          hour: h, minute: m,
          heat: Number(row.querySelector('[data-f="heat"]').value),
          cool: Number(row.querySelector('[data-f="cool"]').value),
          auto: Number(row.querySelector('[data-f="auto"]').value),
          active: row.querySelector('[data-f="active"]').checked,
        };
      }
      return day;
    });
    const save = overlay.querySelector("#dlg-save");
    save.disabled = true;
    try {
      await this._hass.callService(DOMAIN, "set_schedule", {
        device_id: deviceId,
        schedule_enabled: overlay.querySelector("#dlg-enabled").checked,
        days,
      });
      this._closeEditor();
    } catch (err) {
      save.disabled = false;
      this._dlgError(`Save failed: ${(err && err.message) || err}`);
    }
  }
}

// ---------------------------------------------------------------- editor
const EDITOR_SCHEMA = [
  { name: "entity", required: true, selector: { entity: { domain: "climate", integration: DOMAIN } } },
  { name: "name", selector: { text: {} } },
  {
    type: "grid", name: "", schema: [
      { name: "show_status", selector: { boolean: {} } },
      { name: "show_setpoints", selector: { boolean: {} } },
      { name: "show_schedule", selector: { boolean: {} } },
      { name: "show_sensors", selector: { boolean: {} } },
      { name: "show_weather", selector: { boolean: {} } },
    ],
  },
];
const EDITOR_LABELS = {
  entity: "Thermostat (climate entity)", name: "Name (optional)", show_status: "Show status",
  show_setpoints: "Show heat/cool/auto setpoints", show_schedule: "Show schedule", show_sensors: "Show sensor chips", show_weather: "Show weather",
};
const BOOL_KEYS = ["show_status", "show_setpoints", "show_schedule", "show_sensors", "show_weather"];

class Esp32ThermostatCardEditor extends HTMLElement {
  constructor() {
    super();
    this._config = {};
    this._hass = null;
    this.attachShadow({ mode: "open" });
  }

  setConfig(config) {
    this._config = { ...config };
    this._render();
  }

  set hass(hass) {
    this._hass = hass;
    if (this._form) this._form.hass = hass;
    else this._render();
  }

  connectedCallback() {
    if (!customElements.get("ha-form") && window.loadCardHelpers) {
      // Make HA lazy-load ha-form / ha-entity-picker.
      window.loadCardHelpers()
        .then((helpers) => helpers.createCardElement({ type: "entities", entities: [] }))
        .then((card) => card && card.constructor.getConfigElement && card.constructor.getConfigElement())
        .catch(() => {});
    }
    if (!customElements.get("ha-form") && customElements.whenDefined) {
      customElements.whenDefined("ha-form").then(() => { this._form = null; this._render(); });
    }
    this._render();
  }

  _data() {
    const data = { ...this._config };
    for (const k of BOOL_KEYS) if (data[k] === undefined) data[k] = true;
    return data;
  }

  _changed(config) {
    const clean = { ...config };
    for (const k of BOOL_KEYS) if (clean[k] === true) delete clean[k];
    if (!clean.name) delete clean.name;
    this._config = clean;
    fireEvent(this, "config-changed", { config: clean });
  }

  _render() {
    if (!this.shadowRoot) return;
    if (customElements.get("ha-form")) {
      if (!this._form) {
        this.shadowRoot.innerHTML = "";
        this._form = document.createElement("ha-form");
        this._form.computeLabel = (s) => EDITOR_LABELS[s.name] || s.name;
        this._form.addEventListener("value-changed", (ev) => this._changed({ ...this._config, ...ev.detail.value }));
        this.shadowRoot.appendChild(this._form);
      }
      this._form.hass = this._hass;
      this._form.schema = EDITOR_SCHEMA;
      this._form.data = this._data();
      return;
    }
    // Fallback editor without HA components.
    const hass = this._hass;
    const climates = hass ? Object.keys(hass.states).filter((id) => id.startsWith("climate.") &&
      (!hass.entities || !hass.entities[id] || !hass.entities[id].platform || hass.entities[id].platform === DOMAIN)) : [];
    const data = this._data();
    this.shadowRoot.innerHTML = `<style>label{display:block;margin:6px 0} select,input[type=text]{width:100%;padding:6px;box-sizing:border-box}</style>
      <label>${EDITOR_LABELS.entity}<select id="entity"></select></label>
      <label>${EDITOR_LABELS.name}<input type="text" id="name"></label>
      ${BOOL_KEYS.map((k) => `<label><input type="checkbox" id="${k}"> ${EDITOR_LABELS[k]}</label>`).join("")}`;
    const sel = this.shadowRoot.getElementById("entity");
    for (const id of climates.includes(data.entity) || !data.entity ? climates : [data.entity, ...climates]) {
      const opt = document.createElement("option");
      opt.value = id;
      opt.textContent = (hass && hass.states[id] && hass.states[id].attributes.friendly_name) || id;
      opt.selected = id === data.entity;
      sel.appendChild(opt);
    }
    const name = this.shadowRoot.getElementById("name");
    name.value = data.name || "";
    const collect = () => {
      const cfg = { ...this._config, entity: sel.value, name: name.value };
      for (const k of BOOL_KEYS) cfg[k] = this.shadowRoot.getElementById(k).checked;
      this._changed(cfg);
    };
    sel.addEventListener("change", collect);
    name.addEventListener("change", collect);
    for (const k of BOOL_KEYS) {
      const cb = this.shadowRoot.getElementById(k);
      cb.checked = data[k] !== false;
      cb.addEventListener("change", collect);
    }
  }
}

if (!customElements.get(CARD_TYPE)) customElements.define(CARD_TYPE, Esp32ThermostatCard);
if (!customElements.get(EDITOR_TYPE)) customElements.define(EDITOR_TYPE, Esp32ThermostatCardEditor);

window.customCards = window.customCards || [];
if (!window.customCards.some((c) => c.type === CARD_TYPE)) {
  window.customCards.push({
    type: CARD_TYPE,
    name: "ESP32 Thermostat",
    description: "Dial, modes, schedule and sensors for the ESP32 Simple Thermostat.",
    preview: true,
    documentationURL: "https://github.com/jrtaylor71/ESP32-DevKitC3-Simple-Thermostat-PCB/tree/main/custom_components/esp32_thermostat",
  });
}

console.info("%c ESP32-THERMOSTAT-CARD %c loaded ", "color:#fff;background:#ff8100;font-weight:700", "color:#ff8100");

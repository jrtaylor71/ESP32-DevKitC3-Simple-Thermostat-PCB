# MQTT Schedule Quick Start

Get your thermostat schedule syncing with Home Assistant in 5 minutes.

## ⚡ Quick Setup (5 Minutes)

### 1. Enable MQTT on Thermostat (2 min)

1. Open thermostat web interface: `http://[IP-ADDRESS]`
2. Settings tab → MQTT section
3. Check "Enable MQTT"
4. Enter your MQTT broker IP
5. Click "Save All Settings"
6. Verify cloud icon appears on display (MQTT connected)

### 2. Copy Files to Home Assistant (1 min)

From this repository:
```bash
# Copy to HA packages folder
cp multi_thermostat_schedule_sync.yaml ~/.homeassistant/packages/
cp generate_schedule_package.sh ~/.homeassistant/
chmod +x ~/.homeassistant/generate_schedule_package.sh
```

### 3. Generate Package for Your Thermostat (1 min)

```bash
cd ~/.homeassistant
./generate_schedule_package.sh shop_thermostat
```

This creates `shop_thermostat_schedule.yaml` with 77 helpers and automations.

### 4. Add to Home Assistant (1 min)

Edit `configuration.yaml`:
```yaml
homeassistant:
  packages:
    thermostat_sync: !include packages/multi_thermostat_schedule_sync.yaml
    shop_thermostat: !include packages/shop_thermostat_schedule.yaml
```

### 5. Reload in Home Assistant

1. Developer Tools → YAML
2. "Reload Automations"
3. Restart Home Assistant

## ✅ Verify It Works

1. Settings → Devices & Services → Helpers
2. Search "shop_thermostat" 
3. Should see 77 helpers (times, temperatures, enabled status)
4. Change one helper value
5. Check device updates within 2 seconds ✅

## 🎯 Next Steps

- **View Schedule**: Settings > Helpers > Search thermostat name
- **Edit Times**: Click any `input_datetime` helper
- **Change Temps**: Click any `input_number` helper  
- **Multiple Devices**: Run script for each:
  ```bash
  ./generate_schedule_package.sh studio_thermostat
  ./generate_schedule_package.sh house_thermostat
  ```

## 🆘 Stuck?

- MQTT not connecting? Check broker IP in thermostat settings
- Helpers not appearing? Reload automations and restart HA
- Device not updating? Verify MQTT topic: `mosquitto_sub -h broker.ip -t "+/schedule/+"`

See [MQTT_SCHEDULE_SYNC.md](MQTT_SCHEDULE_SYNC.md) for detailed troubleshooting.

---

**Version 1.4.001** | Bidirectional schedule sync - changes update automatically in both directions!

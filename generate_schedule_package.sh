#!/usr/bin/env bash
set -euo pipefail

# Generate a Home Assistant schedule package YAML and matching dashboard card for a given thermostat hostname.
usage() {
  echo "Usage: $0 <hostname> [template_yaml] [card_template] [dest_dir]" >&2
  echo "  <hostname>: lowercase_with_underscores (e.g., shop_thermostat, studio_thermostat)" >&2
}

if [[ ${1:-} == "-h" || ${1:-} == "--help" || $# -lt 1 ]]; then
  usage
  exit 1
fi

HOSTNAME="$1"
TEMPLATE_YAML="${2:-template_thermostat_schedule.yaml}"
CARD_TEMPLATE="${3:-ha_schedule_card.yaml}"
DEST_DIR="${4:-.}"

if ! [[ "$HOSTNAME" =~ ^[a-z0-9_]+$ ]]; then
  echo "Error: hostname must be lowercase_with_underscores. Got: $HOSTNAME" >&2
  exit 1
fi

if [[ ! -f "$TEMPLATE_YAML" ]]; then
  echo "Error: schedule template file not found: $TEMPLATE_YAML" >&2
  exit 1
fi

if [[ ! -f "$CARD_TEMPLATE" ]]; then
  echo "Error: card template file not found: $CARD_TEMPLATE" >&2
  exit 1
fi

mkdir -p "$DEST_DIR"
OUT_FILE="$DEST_DIR/${HOSTNAME}_schedule.yaml"
CARD_OUT_FILE="$DEST_DIR/${HOSTNAME}_schedule_card.yaml"

# Convert hostname: shop_thermostat -> Shop-Thermostat
MQTT_HOSTNAME=$(echo "$HOSTNAME" | sed 's/_/-/g' | sed 's/\b\(.\)/\U\1/g')

for f in "$OUT_FILE" "$CARD_OUT_FILE"; do
  if [[ -e "$f" ]]; then
    echo "Info: overwriting existing $f" >&2
  fi
done

sed \
  -e "s/shop_thermostat/${HOSTNAME}/g" \
  -e "s#HOSTNAME_PLACEHOLDER/schedule/set#${MQTT_HOSTNAME}/schedule/set#g" \
  "$TEMPLATE_YAML" > "$OUT_FILE"

sed \
  -e "s/HOST_PREFIX/${HOSTNAME}/g" \
  "$CARD_TEMPLATE" > "$CARD_OUT_FILE"

if ! grep -q "${MQTT_HOSTNAME}/schedule/set" "$OUT_FILE"; then
  echo "Warning: MQTT topic replacement not found (expected ${MQTT_HOSTNAME}/schedule/set)." >&2
fi

REPLACED_COUNT=$(grep -o "$HOSTNAME" "$OUT_FILE" | wc -l | tr -d ' ')
LINES=$(wc -l < "$OUT_FILE" | tr -d ' ')

echo "Generated package: $OUT_FILE" >&2
echo "Lines: ${LINES} | Occurrences of hostname: ${REPLACED_COUNT}" >&2
echo "Generated card:    $CARD_OUT_FILE" >&2
echo "MQTT Device Topic: ${MQTT_HOSTNAME}/schedule/set" >&2

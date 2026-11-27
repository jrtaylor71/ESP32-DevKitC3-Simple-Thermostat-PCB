import importlib
import json
import pathlib
import py_compile
import re
import sys

import yaml

ROOT = pathlib.Path(__file__).resolve().parents[2]
INTEGRATION = ROOT / "custom_components" / "esp32_thermostat"
sys.path.insert(0, str(ROOT))

for path in INTEGRATION.rglob("*.py"):
    py_compile.compile(str(path), doraise=True)

for path in [*INTEGRATION.rglob("*.json"), ROOT / "hacs.json"]:
    json.loads(path.read_text())

yaml.safe_load((ROOT / ".github/workflows/validate-ha-integration.yml").read_text())
yaml.safe_load((INTEGRATION / "services.yaml").read_text())

strings_text = (INTEGRATION / "strings.json").read_text()
assert strings_text == (INTEGRATION / "translations/en.json").read_text()
strings = json.loads(strings_text)
source = "\n".join(path.read_text() for path in INTEGRATION.glob("*.py"))
missing = []

for platform in (
    "sensor",
    "binary_sensor",
    "switch",
    "number",
    "button",
    "climate",
    "select",
):
    module = importlib.import_module(f"custom_components.esp32_thermostat.{platform}")
    for name in dir(module):
        descriptions = getattr(module, name)
        if not isinstance(descriptions, tuple) or not descriptions:
            continue
        if not hasattr(descriptions[0], "translation_key"):
            continue
        for description in descriptions:
            if description.translation_key not in strings["entity"][platform]:
                missing.append((platform, description.translation_key))
                continue
            if getattr(description, "options", None):
                states = strings["entity"][platform][description.translation_key].get(
                    "state", {}
                )
                missing.extend(
                    (platform, description.translation_key, option)
                    for option in description.options
                    if option not in states
                )

required_exceptions = {
    "command_rejected",
    "communication_error",
    "auth_failed",
    "wrong_device",
    "temperature_mode_off",
    "invalid_device_id",
    "device_not_loaded",
    "invalid_days",
    "nothing_to_set",
}
assert required_exceptions <= strings["exceptions"].keys()

exception_keys = set(
    re.findall(r'translation_domain=DOMAIN,\s*translation_key="([^"]+)"', source)
)
missing.extend(
    ("exceptions", key)
    for key in exception_keys
    if key not in strings["exceptions"]
)
assert "thermostat" in strings["entity"]["climate"]
assert not missing, f"Missing translations: {missing}"
print("HA integration static checks OK")

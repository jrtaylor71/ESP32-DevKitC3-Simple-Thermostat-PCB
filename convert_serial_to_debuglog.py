#!/usr/bin/env python3
"""
Convert all Serial.printf() and Serial.println() calls to debugLog() equivalents
while preserving all messages and formatting.
"""

import re
import sys

def convert_file(filepath):
    with open(filepath, 'r') as f:
        content = f.read()
    
    original_content = content
    
    # Convert Serial.printf(...) to debugLog(...)
    # This handles multi-line printf statements
    content = re.sub(
        r'Serial\.printf\(',
        r'debugLog(',
        content
    )
    
    # Convert Serial.println(...) to debugLog(...\n)
    # Handle both with and without arguments
    def convert_println(match):
        content = match.group(1)
        # If it ends with quotes, add \n before closing quote
        if content.rstrip().endswith('"'):
            return 'debugLog("' + content[:-1] + '\\n");'
        else:
            return 'debugLog(' + content + ', "\\n");'
    
    # Match Serial.println with single string arg (common case)
    content = re.sub(
        r'Serial\.println\("([^"]+)"\);',
        lambda m: 'debugLog("[' + m.group(1)[1:] + '\\n");' if m.group(1).startswith('[') else 'debugLog("' + m.group(1) + '\\n");',
        content
    )
    
    # Match Serial.println() with no args
    content = re.sub(
        r'Serial\.println\(\);',
        r'debugLog("\n");',
        content
    )
    
    # Match Serial.println with other content (like variables)
    content = re.sub(
        r'Serial\.println\(([^;)]+)\);',
        lambda m: f'debugLog("%s\\n", {m.group(1)});' if not m.group(1).startswith('"') else f'debugLog({m.group(1)} + "\\n");',
        content
    )
    
    # Convert Serial.print(...) to debugLog(...) - careful not to add \n
    content = re.sub(
        r'Serial\.print\("([^"]+)"\);',
        r'debugLog("\1");',
        content
    )
    
    # Handle multi-argument Serial.print calls
    content = re.sub(
        r'Serial\.print\(([^)]+)\);',
        lambda m: f'debugLog("{m.group(1)}"' + (');\n' if ');\n' in m.group(0) else ');'),
        content
    )
    
    # Write back if changed
    if content != original_content:
        with open(filepath, 'w') as f:
            f.write(content)
        return True
    return False

if __name__ == "__main__":
    files = [
        "/home/jonnt/Documents/ESP32-DevKitC3-Simple-Thermostat-PCB/src/Main-Thermostat.cpp",
        "/home/jonnt/Documents/ESP32-DevKitC3-Simple-Thermostat-PCB/src/Weather.cpp"
    ]
    
    for filepath in files:
        try:
            changed = convert_file(filepath)
            print(f"{'✓' if changed else '✗'} {filepath}")
        except Exception as e:
            print(f"ERROR {filepath}: {e}")
            sys.exit(1)
    
    print("\nConversion complete!")

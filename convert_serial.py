#!/usr/bin/env python3
"""
Convert Serial.printf/print/println to debugLog with proper formatting
"""

import re
import sys

def process_file(filename):
    with open(filename, 'r') as f:
        lines = f.readlines()
    
    output = []
    i = 0
    changes = 0
    
    while i < len(lines):
        line = lines[i]
        
        # Handle Serial.printf - these are already correctly formatted
        if 'Serial.printf(' in line:
            line = line.replace('Serial.printf(', 'debugLog(')
            changes += 1
        
        # Handle Serial.println with arguments - need to add \n to format
        # Pattern: Serial.println("string") -> debugLog("string\n");
        elif re.search(r'Serial\.println\("([^"]*?)"\)', line):
            line = re.sub(
                r'Serial\.println\("([^"]*?)"\)',
                r'debugLog("\1\\n")',
                line
            )
            changes += 1
        
        # Handle Serial.println() with no args -> debugLog("\n");
        elif 'Serial.println()' in line:
            line = line.replace('Serial.println()', 'debugLog("\\n")')
            changes += 1
        
        # Handle Serial.print("string") -> debugLog("string")
        elif re.search(r'Serial\.print\("([^"]*)"\)', line):
            line = re.sub(
                r'Serial\.print\("([^"]*)"\)',
                r'debugLog("\1")',
                line
            )
            changes += 1
        
        # Handle multi-line Serial.printf (check next lines for closing paren)
        elif 'Serial.printf(' in line and ')' not in line:
            # Multi-line printf - replace and keep structure
            line = line.replace('Serial.printf(', 'debugLog(')
            changes += 1
        
        output.append(line)
        i += 1
    
    # Write back
    with open(filename, 'w') as f:
        f.writelines(output)
    
    return changes

if __name__ == '__main__':
    files = [
        'src/Main-Thermostat.cpp',
        'src/Weather.cpp'
    ]
    
    total = 0
    for fname in files:
        try:
            count = process_file(fname)
            print(f"{fname}: {count} changes")
            total += count
        except Exception as e:
            print(f"ERROR {fname}: {e}", file=sys.stderr)
            sys.exit(1)
    
    print(f"\nTotal changes: {total}")

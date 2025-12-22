#!/bin/bash
# Generate STL files from OpenSCAD sources

echo "Generating STL files for ESP32-S3 Simple Thermostat case..."

# Front case (display side, faces room)
echo "Processing front_case_display.scad..."
openscad -o thermostat_case_front.stl front_case_display.scad
if [ $? -eq 0 ]; then
    echo "✓ Generated thermostat_case_front.stl"
    ls -lh thermostat_case_front.stl
else
    echo "✗ Failed to generate front case STL"
    exit 1
fi

# Back case (wall-mounting side)
echo "Processing back_case_wall.scad..."
openscad -o thermostat_case_back.stl back_case_wall.scad
if [ $? -eq 0 ]; then
    echo "✓ Generated thermostat_case_back.stl"
    ls -lh thermostat_case_back.stl
else
    echo "✗ Failed to generate back case STL"
    exit 1
fi

echo ""
echo "Done! Generated files:"
ls -lh thermostat_case_*.stl

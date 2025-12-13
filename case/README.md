# ESP32-S3 Smart Thermostat - 3D Printable Case

This directory contains the 3D printable case design for the ESP32-S3 Smart Thermostat.

## Design Overview

The case is a two-part design:
- **Back Case (Wall Side)**: Mounts to wall via keyholes, provides component clearance, ventilation
- **Front Case (Display Side)**: Holds PCB on standoffs, display/sensor cutouts, screws to back

### Specifications

**PCB Dimensions:**
- Length: 133.0mm
- Width: 89.5mm
- Thickness: 1.6mm
- 4x 3.2mm mounting holes at corners

**Case Dimensions:**
- Outer: 149.0mm × 105.5mm (16mm margin around PCB)
- Back case height: 30.1mm (2.5mm wall + 4mm standoff + 1.6mm PCB + 20mm component clearance for ESP32)
- Front case height: 17.1mm (2.5mm wall + 14.6mm display clearance, standoffs 13mm tall + 1.6mm above)
- Total assembled: ~47mm

**Fastening:**
- 4× M2.5 screws secure front to back (countersunk on front face)
- Screw positions: 7mm inset from each corner
- 1.5mm seating lip prevents full nesting
- 2× wall-mount keyholes on back (83mm spacing)

**Display:**
- 3.2" ILI9341 TFT LCD (320×240 pixels)
- Display opening: 50mm × 68mm (rotated 90°, centered on display holes)
- Located on PCB component side (front case interior)
- LDR photoresistor: 5.5mm hole for ambient light
- AHT20 temp/humidity sensor: 12.5×6mm rectangular cutout (rotated 90°)

## Files

- `front_case_display.scad` - Front case (display side) OpenSCAD source
- `back_case_wall.scad` - Back case (wall mount side) OpenSCAD source
- `generate_stl.sh` - Bash script to regenerate both STL files
- `thermostat_case_front.stl` - Front half STL (display side, with PCB standoffs & screw holes)
- `thermostat_case_back.stl` - Back half STL (wall mount side, with screw bosses & keyholes)

## Customization

The OpenSCAD file is fully parametric. You can modify:

- **Wall thickness**: Adjust `wall_thickness` (default: 2.5mm)
- **Clearances**: Modify `case_clearance` and component clearances
- **Display cutout**: Adjust `display_width` and `display_height` for your specific display
- **Mounting**: Change keyhole slot dimensions for different screws
- **Ventilation**: Modify vent slot size and spacing

## 3D Printing Instructions

### Print Settings (Recommended)

**Material:**
- PLA or PETG recommended
- ABS can be used but may warp on larger prints

**Layer Height:** 0.2mm (0.15mm for finer detail)

**Infill:** 15-20%

**Supports:** None required (designed support-free)

**Wall Count:** 3-4 perimeters

**Top/Bottom Layers:** 4-5 layers

**Print Orientation:**
- **Front case**: Print upside-down (standoffs & screw countersinks face build plate)
- **Back case**: Print flat back on build plate (screw bosses facing up)

### Print Time Estimates
- Front case: ~4-5 hours (larger, with standoffs and countersinks)
- Back case: ~3-4 hours (screw bosses and seating lip)
- Total: ~7-9 hours (depending on printer speed)

### Material Requirements
- Approximately 80-100g of filament total for both parts

## Assembly Instructions

### Required Hardware

1. **For case assembly:**
   - 4× M2.5 × 10-12mm countersunk screws (front to back case fastening)
   - Countersink: 3mm → 7mm taper, 3mm deep

2. **For PCB mounting:**
   - 4× M2.5 × 6mm screws (PCB to front case standoffs)
   
3. **For wall mounting:**
   - 2× #6 wood screws or drywall anchors
   - Screw heads ~8-10mm diameter
   - Keyhole slots spaced 83mm apart horizontally

### Assembly Steps

1. **Prepare the back case (wall mount side):**
   - Check screw bosses printed cleanly (8mm dia, 2.5mm pilot holes)
   - Verify keyhole slots are clear
   - Clean any support material

2. **Install PCB to front case:**
   - Place PCB component-side up onto front case standoffs (13mm tall)
   - Align PCB mounting holes with standoff holes
   - Insert 4× M2.5 × 6mm screws through PCB into standoffs
   - Tighten gently (do not overtighten plastic threads)

3. **Join front and back:**
   - Orient front case so display cutout faces outward
   - Align front bezel onto back case seating lip
   - Insert 4× M2.5 countersunk screws through front holes into back bosses
   - Tighten evenly to seat front flush on lip

4. **Wall mounting:**
   - Mark wall at desired height (83mm horizontal spacing for keyholes)
   - Install screws leaving ~3mm gap from wall
   - Hang back case on screws via keyholes
   - Slide down to lock in place
   - Insert M2.5 screws through PCB holes into standoffs
   - Tighten gently (don't over-torque)

3. **Connect cables:**
   - Route power and HVAC cables through side ventilation slots
   - Ensure cables won't interfere with back case closure

4. **Attach back case:**
   - Align back case over front case
   - The alignment lip should guide proper positioning
   - Press together gently until seated
   - Cases snap together via friction fit

5. **Wall mounting:**
   - Mark wall at desired location
   - Install screws 83mm apart (horizontal spacing)
   - Leave screw heads protruding ~3mm from wall
   - Slide case keyhole slots over screw heads
   - Gently pull down to lock in place

## Viewing the Design

To view or modify the design in OpenSCAD:

1. Install OpenSCAD (https://openscad.org/)
2. Open `thermostat_case.scad`
3. Select render mode at bottom of file:
   - `assembly_view()` - See assembled case with PCB
   - `print_layout()` - Both parts laid out for printing
   - `front_case()` - Front half only
   - `back_case()` - Back half only
4. Press F5 to preview, F6 to render

## Generating STL Files

If you modify the design and need to regenerate STL files:

```bash
# Using OpenSCAD command line
openscad -o thermostat_case_front.stl -D 'front_case();' thermostat_case.scad
openscad -o thermostat_case_back.stl -D 'back_case();' thermostat_case.scad

# Or use the provided script
./generate_stl.sh
```

## Design Features

### Front Case (Wall Side)
- Integrated PCB standoffs (3mm height) with M2.5 mounting holes
- Keyhole mounting slots for easy wall installation
- Large wire pass-through opening at bottom center (20mm diameter)
- Side wire routing openings (12mm diameter each side)
- Ventilation slots on sides for heat dissipation
- Alignment lip for precise back case positioning
- Wall thickness: 2.5mm for strength
- Total height: 24.1mm (accommodates ESP32 + pin headers + relays)

### Back Case (Display Side)
- Rectangular display opening (52mm x 70mm) for LCD viewing
- Recessed bezel area for display to sit flush
- AHT20 sensor cutout (16mm x 12mm) for ambient temperature/humidity sensing
- Alignment lip recess for snap-fit assembly
- Top and bottom edge ventilation for airflow
- Smooth finish for professional appearance
- Height: 14.5mm (accommodates display connector pins + LCD module)

### Ventilation
- Side slots in front case (both sides)
- Top slots in back case
- Ensures adequate airflow for ESP32 and components
- Prevents heat buildup

## Troubleshooting

**Parts don't fit together:**
- Check `lip_clearance` parameter (increase if too tight)
- Ensure no stringing or blobs on lip features
- May need to sand alignment lip slightly

**PCB doesn't fit:**
- Verify PCB dimensions match design
- Check `case_clearance` parameter
- Ensure standoffs aren't too tall (should be 5mm)

**Display cutout wrong size:**
- Measure your actual display bezel
- Adjust `display_width` and `display_height`
- Regenerate STL files

**Mounting holes don't align:**
- Verify your PCB matches the design specs
- Check `hole_positions` array in SCAD file
- May need to drill holes slightly larger

## License

This case design is open source and follows the same license as the main project.

## Credits

Case designed for the ESP32-S3 Smart Thermostat project by Jonn Taylor.
PCB design by Stefan Meisner.

## Version History

- v1.0 (2025-12-11): Initial release
  - Two-part snap-fit design
  - Keyhole wall mounting
  - Integrated PCB standoffs
  - Display cutout for 3.2" ILI9341

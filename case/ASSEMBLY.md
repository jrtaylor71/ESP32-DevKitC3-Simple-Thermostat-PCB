# ESP32-S3 Smart Thermostat Case Assembly Guide

⚠️ **WORK IN PROGRESS** - This design is actively being tested and refined. Dimensions and assembly procedures may change.

Complete step-by-step assembly and installation instructions for the 3D printed thermostat case.

## Table of Contents
1. [Pre-Assembly Checklist](#pre-assembly-checklist)
2. [Part Identification](#part-identification)
3. [Assembly Instructions](#assembly-instructions)
4. [Wall Installation](#wall-installation)
5. [Troubleshooting](#troubleshooting)

---

## Pre-Assembly Checklist

### 3D Printed Parts
- [ ] Front case (wall-facing, with standoffs)
- [ ] Back case (display side)
- [ ] Parts are clean and free of supports/strings
- [ ] Mounting holes are clear and smooth

### Hardware Required
- [ ] 4× M2.5 × 6mm screws (PCB to front case standoffs)
- [ ] 4× M2.5 × 10-12mm countersunk screws (front case to back case)
- [ ] 2× #6 wood screws or drywall anchors (wall mounting)
- [ ] Assembled PCB with all components
- [ ] Wiring for HVAC system (per project documentation)

### Tools Needed
- Phillips screwdriver (small, for M2.5 screws)
- Drill with appropriate bit (for wall mounting)
- Level (for straight installation)
- Pencil (marking screw locations)

---

## Part Identification

### Back Case (Wall Mount Side)
```
┌─────────────────────────────────────┐
│  ○                             ○    │  ← Keyhole mounting slots
│                                      │     (83mm spacing)
│  ║     ●   ●                   ║    │  ← Side ventilation + 
│  ║                       ●   ● ║    │     screw bosses (●)
│  ║                             ║    │
│  ○                             ○    │
└─────────────────────────────────────┘
        Interior view showing:
        - 4 screw bosses (8mm dia, 2.5mm pilot hole)
        - Perimeter seating lip (1.5mm × 1.5mm)
        - Wire/keyhole cutouts on back face
        - Ventilation slots on left/right sides
```

**Dimensions:** 149mm × 105.5mm × 30.1mm  
**Features:**
- 4× screw bosses at corners (7mm inset, Ø8mm, 10mm tall, Ø2.5mm pilot)
- 2× keyhole slots on back face for wall mounting (83mm spacing)
- 1.5mm seating lip around top interior perimeter
- 22mm wire entry hole (centered on back face)
- Ventilation slots on left/right edges
- 20mm clearance for ESP32 stack (15.2mm component + margin)

### Front Case (Display Side)
```
┌─────────────────────────────────────┐
│  ⊙                             ⊙    │  ← Countersunk screw
│                                      │     holes (⊙)
│    ┌─────────────────────────┐     │
│    │                         │     │  ← Display cutout
│    │     DISPLAY AREA        │     │     (50mm × 68mm)
│  ● │                         │ ●   │  ← Sensor holes
│    └─────────────────────────┘     │     (LDR ●, AHT20 rect)
│                                      │
│  ⊙          ═══  ═══  ═══       ⊙   │  ← Ventilation slots
└─────────────────────────────────────┘
```

**Dimensions:** 149mm × 105.5mm × 17.1mm  
**Features:**
- 4× PCB standoffs with Ø2.7mm holes (13mm tall, accommodate PCB + 1.6mm margin)
- 4× countersunk screw holes (3mm → 7mm taper, 3mm deep, 7mm inset from corners)
- Display cutout: 50mm × 68mm (rotated 90°, centered on display module holes)
- LDR photoresistor hole: Ø5.5mm (lower-right area)
- AHT20 sensor cutout: 12.5mm × 6mm rectangle (rotated 90°, upper-left area)
- Top/bottom edge ventilation slots

---

## Assembly Instructions

### Step 1: Prepare 3D Printed Parts

1. **Inspect back case:**
   - Check that all 4 screw bosses are intact (8mm dia cylinders)
   - Verify Ø2.5mm pilot holes in bosses are clear
   - Test-fit an M2.5 screw (should thread smoothly into pilot)
   - Remove any support material from keyhole slots on back face
   - Verify 1.5mm seating lip around top interior is clean

2. **Inspect front case:**
   - Ensure all 4 standoffs are intact (Ø7mm, 13mm tall)
   - Check Ø2.7mm holes in standoffs are clear
   - Verify countersunk screw holes have smooth taper (3mm → 7mm)
   - Check display cutout and sensor holes are clean
   - Remove any stringing from interior

3. **Test fit cases together:**
   - Align front bezel over back case seating lip
   - Verify screw holes align with bosses (7mm inset from corners)
   - Lip should provide ~1.5mm gap between case halves
   - The lip should guide proper positioning
   - Cases should mate without gaps
   - If too tight, lightly sand the alignment lip

### Step 2: Install PCB

1. **Orient PCB correctly:**
   ```
   Front Case (looking down from above):
   
   ┌─────────────────────┐
   │  STANDOFF  STANDOFF │
   │      ●         ●    │
   │                     │
   │    ┌──────────┐     │
   │    │  ESP32   │     │  ← ESP32 faces DOWN
   │    │  (down)  │     │     into front case
   │    └──────────┘     │
   │                     │
   │      ●         ●    │
   │  STANDOFF  STANDOFF │
   └─────────────────────┘
   ```

2. **Place PCB in front case:**
   - ESP32 side faces DOWN (toward case bottom)
   - Display connectors face UP (toward case opening)
   - Align 4 mounting holes with standoffs
   - PCB should rest evenly on all standoffs

3. **Secure PCB:**
   - Insert M2.5 x 6mm screws through PCB holes
   - Thread into standoffs
   - Tighten in diagonal pattern (prevents warping):
     1. Bottom-left
     2. Top-right  
     3. Top-left
     4. Bottom-right
   - Snug but not over-tight (PCB may crack)

4. **Verify installation:**
   - PCB should be level and stable
   - No rocking or movement
   - Screw heads flush with PCB surface
   - Display connector accessible from case opening

### Step 3: Cable Routing

1. **Power cable:**
   - Route through side ventilation slot
   - Leave 15-20cm slack for service
   - Avoid sharp bends near connectors

2. **HVAC control wires:**
   - Route through opposite side slot
   - Bundle and secure with cable tie
   - Label wires per project documentation:
     - Fan, Heat1, Heat2, Cool1, Cool2
     - Each with +5V and GND pairs

3. **Optional sensors:**
   - DS18B20 (water temp): through side slot
   - LD2410 (motion): ensure clear line of sight
   - Keep sensor cables away from power wires

4. **Cable management:**
   - Don't overfill ventilation slots
   - Ensure cables won't interfere with back case
   - Use cable ties to organize bundles

### Step 4: Attach Back Case

1. **Pre-flight check:**
   - Display properly connected to PCB
   - Cables routed and organized
   - No obstructions in case cavity
   - Alignment lip on front case is clean

2. **Position back case:**
   ```
   Side View (before closing):
   
   Back Case (flipped)          Front Case
   ═══════════════              ════════════════
   ╔═════════════╗              ║  ╔PCB╗       ║
   ║  DISPLAY    ║              ║  ║ESP║       ║
   ║  (touching  ║              ║  ║32 ║       ║
   ║   PCB)      ║              ║  ╚═══╝       ║
   ╚═════════════╝              ║              ║
   └─── Lip recess              └── Alignment lip
   ```

3. **Close case:**
   - Hold back case above front case (display facing out)
   - Align edges using lip as guide
   - Lower gently, keeping alignment
   - Press evenly around perimeter until seated
   - You should feel/hear a slight "snap"

4. **Verify closure:**
   - No gaps between case halves
   - Cases flush all around perimeter
   - Display visible and centered in cutout
   - Slight resistance if trying to pull apart

---

## Wall Installation

### Step 5: Prepare Wall Location

1. **Choose location:**
   - Standard thermostat height: 52-60 inches from floor
   - Near center of area to be controlled
   - Away from:
     - Direct sunlight
     - Heat sources (lamps, appliances)
     - Drafts (windows, doors)
     - Corners (poor air circulation)

2. **Check for obstructions:**
   - Use stud finder for studs/wiring
   - Ensure adequate clearance (6" minimum all sides)
   - Verify HVAC wiring can reach location

3. **Mark screw positions:**
   ```
   Wall marking template:
   
   Center line
       |
       ↓
   ────┼────  ← Level line
       |
   ●   |   ●  ← Screw positions
       |      (83mm apart, horizontal)
   41.5mm from center
   ```
   
   - Mark horizontal level line
   - Mark center point
   - Measure 41.5mm left and right of center
   - Marks should be 83mm apart total

4. **Drill holes:**
   - Use appropriate bit for wall type
   - Drywall: Install anchors first
   - Wood stud: Drill pilot holes
   - Depth: 25-30mm

### Step 6: Install Mounting Screws

1. **Insert screws:**
   - Use #6 wood screws (or appropriate for anchors)
   - Screw head diameter: 8-10mm
   - Leave heads protruding 3-4mm from wall
   - Verify screws are level

2. **Test fit:**
   ```
   Keyhole slot detail:
   
   ╔═══════╗  ← Wide opening (screw head)
   ║       ║
   ╚═══╤═══╝
       │       ← Narrow slot (screw shaft)
       │
   ```
   - Hold case up to screws
   - Screw heads should fit through wide part
   - Adjust screw depth if needed

### Step 7: Hang Thermostat

1. **Position case:**
   - Align keyhole slots over screw heads
   - Slide down gently
   - Screws should drop into narrow part of slots

2. **Lock in place:**
   - While supporting case, push UP slightly
   - Then pull DOWN firmly
   - Case should be secure and not rattle

3. **Verify installation:**
   - Case should be level
   - Firmly attached (no movement)
   - Display visible and centered
   - All cables connected

### Step 8: Connect and Test

1. **Final connections:**
   - Connect HVAC control wires to system
   - Connect power supply
   - Double-check polarity and connections

2. **Power on:**
   - Display should illuminate
   - Touch screen should respond
   - Check all system functions

3. **Configure settings:**
   - Follow main project documentation
   - Configure WiFi, MQTT, schedules
   - Test heating/cooling operation

---

## Troubleshooting

### Case Assembly Issues

**Problem:** Cases won't snap together  
**Solutions:**
- Check for debris in alignment lip
- Verify lip clearance (may need light sanding)
- Ensure no cables blocking closure
- Check PCB is seated properly on standoffs

**Problem:** PCB doesn't fit  
**Solutions:**
- Verify correct PCB orientation (ESP32 down)
- Check standoff height (should be 5mm)
- Ensure no warping from print
- May need to enlarge case cavity slightly

**Problem:** Screws won't thread into standoffs  
**Solutions:**
- Clean out standoff holes with 2.5mm drill bit
- Use self-tapping M2.5 screws
- Don't cross-thread (start carefully)
- Standoffs may need thread repair

### Wall Mounting Issues

**Problem:** Case won't stay on wall  
**Solutions:**
- Increase screw protrusion (4-5mm)
- Use larger screw heads (10mm diameter)
- Ensure screws are level
- Check keyhole slots aren't damaged

**Problem:** Case is crooked  
**Solutions:**
- Re-measure screw positions with level
- Adjust one screw slightly
- May need to patch and re-drill
- Use shims if wall is uneven

**Problem:** Screws pull out of drywall  
**Solutions:**
- Use proper drywall anchors
- Find wall stud if possible
- Use toggle bolts for heavier duty
- Ensure not over-tightening

### Display Issues

**Problem:** Display not visible through cutout  
**Solutions:**
- Verify display is centered on PCB
- Check PCB orientation in case
- Display may need repositioning
- Cutout may need enlarging (carefully!)

**Problem:** Can't touch screen through opening  
**Solutions:**
- Ensure touch area recess is clear
- Check display isn't recessed too far
- May need to widen touch access slightly
- Verify touch controller is working

---

## Maintenance

### Regular Care
- Wipe display with soft, dry cloth
- Clean ventilation slots quarterly
- Check mounting screws annually
- Verify cable connections

### Disassembly
To remove for service:
1. Power off system
2. Disconnect all cables (label first!)
3. Lift case UP to release from keyhole slots
4. Gently separate case halves
5. Remove PCB mounting screws

### Case Modifications
If you need to modify the case:
- Edit `thermostat_case.scad` in OpenSCAD
- Adjust parameters as needed
- Regenerate STL files with `./generate_stl.sh`
- Re-print modified parts

---

## Safety Notes

⚠️ **Important Safety Information:**

- Turn off HVAC power before wiring
- Follow local electrical codes
- Verify voltage ratings before connecting
- Don't exceed relay current ratings (see PCB docs)
- Keep away from water/moisture
- Don't cover ventilation slots
- Use appropriate wire gauge for current
- Have a licensed electrician verify if uncertain

---

## Support

For technical support:
- GitHub Issues: https://github.com/jrtaylor71/ESP32-DevKitC3-Simple-Thermostat-PCB/issues
- Project Documentation: See main README.md
- Hardware Questions: See DOCUMENTATION.md

---

## Revision History

- v1.0 (2025-12-11): Initial assembly guide

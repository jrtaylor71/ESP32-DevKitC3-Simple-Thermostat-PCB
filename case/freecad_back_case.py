#!/usr/bin/env python3
"""
ESP32-S3 Smart Thermostat - BACK CASE (Wall-mounting side)
FreeCAD Python script to generate back case geometry
This case mounts to the wall and provides wiring/mounting holes
"""

import FreeCAD as App
import Part
import Mesh
import MeshPart
import os

# Create a new document
doc = App.newDocument("BackCase")

# ---------- Parameters (match SCAD) ----------

# PCB dimensions
pcb_length = 133.0
pcb_width = 89.5
pcb_thickness = 1.6

# PCB mounting holes
pcb_mount_holes = [
    [3.5, 3.5],
    [3.5, 86.0],
    [129.5, 3.5],
    [129.5, 86.0]
]

# Case parameters
side_wall_thickness = 9.0
bottom_thickness = 4.0
wall_thickness = side_wall_thickness  # legacy name used throughout
pcb_clearance = 3.0
standoff_height = 4.0
component_height = 20.0

# Calculated dimensions
# Inner cavity dimensions (what PCB needs)
inner_length = pcb_length + (2 * pcb_clearance)
inner_width = pcb_width + (2 * pcb_clearance)
# Outer case dimensions (cavity + walls)
case_length = inner_length + (2 * side_wall_thickness)
case_width = inner_width + (2 * side_wall_thickness)
case_height = bottom_thickness + standoff_height + pcb_thickness + component_height

# Wire entry hole
wire_hole_diameter = 22.0
wire_hole_x = case_length / 2.0
wire_hole_y = case_width / 2.0

# Perimeter lip (disabled per new clip design)
lip_depth = 1.5
lip_height = 1.5
include_back_lip = False
add_print_supports = False
support_thickness = 0.6  # thin sacrificial rib thickness

# Wall mounting holes
wall_mount_hole_dia = 4.0
wall_mount_spacing_x = 83.0
wall_mount_spacing_y = 60.0
# Standard single gang electric box spacing (3.5 inches = 88.9mm)
single_gang_spacing_y = 88.9

# Rounded corners
corner_r = 4.0

# Snap-fit parameters - slots in perimeter lip to receive front case tabs
snap_tab_width = 12.0      # Must match front case
snap_tab_length = 6.0      # Must match front case
snap_tab_height = 2.5      # Must match front case
snap_tab_undercut = 1.0    # Must match front case
snap_slot_clearance = 0.2  # Extra clearance for assembly
snap_hook_thickness = 2.0  # Thinner hook beam for easier fit
snap_hook_height = 6.5     # Hook rise above wall top (compensated for taller barb)
snap_hook_offset_z = 0.0   # Hooks start at wall top (no offset)
snap_hook_protrusion = 0.0 # Hooks are hidden on the inside; no external protrusion
snap_hook_overhang = 0.1   # 1.3 Overhang (barb) depth into window region for better grip
snap_hook_barb_height = 1.5  # 1.5 Vertical height of overhang barb near the hook tip
snap_hook_base_extension = 8.0  # How far hook extends down the inside wall for strength
snap_hook_gusset_depth = 3.0   # How far gusset extends from hook toward interior
lip_top_margin = 0.5       # Leave uncut lip at top to create a catch ledge

# Snap slot positions (match front tabs)
snap_positions = [
    {'side': 'top', 'pos': case_length / 2.0},
    {'side': 'bottom', 'pos': case_length / 2.0},
    {'side': 'left', 'pos': case_width / 2.0},
    {'side': 'right', 'pos': case_width / 2.0}
]

# Ventilation slots (front-style, 3mm wide)
vent_slot_count = 15
vent_slot_count_side = 10
vent_slot_length = 8.0
vent_slot_width = 2.0   # Keep slot width; widen fins via spacing
vent_slot_height = 35.0  # Increased to cut through lip (was 20.0)
vent_spacing = 10.5  # Wider spacing to enlarge fins between vents
vent_corner_margin = 15.0  # Increase corner clearance to keep vents further from corners

# Tolerance for near() comparisons
tolerance = 0.1

def near(a, b):
    return abs(a - b) < tolerance

# ---------- Build Back Case ----------

# Outer shell with rounded corners
outer = Part.makeBox(case_length, case_width, case_height)

# Round vertical corners
try:
    corner_edges = []
    for e in outer.Edges:
        v1 = e.Vertexes[0].Point
        v2 = e.Vertexes[1].Point
        # Vertical edge: x and y same at both vertices, z different
        if near(v1.x, v2.x) and near(v1.y, v2.y) and not near(v1.z, v2.z):
            # At the corners: x is 0 or case_length; y is 0 or case_width
            if (near(v1.x, 0) or near(v1.x, case_length)) and (near(v1.y, 0) or near(v1.y, case_width)):
                corner_edges.append(e)
    if corner_edges:
        outer = outer.makeFillet(corner_r, corner_edges)
except Exception as ex:
    App.Console.PrintWarning("Corner fillet failed (non-critical): %s\n" % ex)

# Hollow interior
inner = Part.makeBox(inner_length, inner_width, case_height - bottom_thickness + 1)
inner.translate(App.Vector(side_wall_thickness, side_wall_thickness, bottom_thickness))

shell = outer.cut(inner)
App.Console.PrintMessage("Created back case shell\n")

# ---------- Cutouts ----------

# Wire entry hole through back panel
wire_hole = Part.makeCylinder(wire_hole_diameter / 2.0, bottom_thickness + 1.0,
                               App.Vector(wire_hole_x, wire_hole_y, -0.5),
                               App.Vector(0, 0, 1))
shell = shell.cut(wire_hole)
App.Console.PrintMessage("Cut wire entry hole\n")

# Wall mounting holes - 4 round holes at corners
for x_offset in [-wall_mount_spacing_x / 2.0, wall_mount_spacing_x / 2.0]:
    for y_offset in [-wall_mount_spacing_y / 2.0, wall_mount_spacing_y / 2.0]:
        mount_hole = Part.makeCylinder(wall_mount_hole_dia / 2.0, bottom_thickness + 1.0,
                                        App.Vector(case_length / 2.0 + x_offset,
                                                  case_width / 2.0 + y_offset,
                                                  -0.5),
                                        App.Vector(0, 0, 1))
        shell = shell.cut(mount_hole)
App.Console.PrintMessage("Cut 4 wall mounting holes\n")

# 2 additional mounting holes between corner holes, towards center
# Top and bottom center holes - positioned for standard single gang electric box (88.9mm spacing)
screw_head_diameter = 8.0  # Diameter of screw head clearance
screw_head_height = 10.0  # Height of clearance cylinder above bottom (tall enough to go through lip)

for y_offset in [-single_gang_spacing_y / 2.0, single_gang_spacing_y / 2.0]:
    # Main mounting hole through bottom panel
    mount_hole = Part.makeCylinder(wall_mount_hole_dia / 2.0, bottom_thickness + 1.0,
                                    App.Vector(case_length / 2.0,
                                              case_width / 2.0 + y_offset,
                                              -0.5),
                                    App.Vector(0, 0, 1))
    shell = shell.cut(mount_hole)
    
    # Vertical clearance cutout above the mounting hole for screw head
    # This extends upward from the bottom, cutting through the inner lip
    screw_clearance = Part.makeCylinder(screw_head_diameter / 2.0, screw_head_height,
                                         App.Vector(case_length / 2.0,
                                                   case_width / 2.0 + y_offset,
                                                   bottom_thickness),
                                         App.Vector(0, 0, 1))
    shell = shell.cut(screw_clearance)

App.Console.PrintMessage("Cut 2 center mounting holes for single gang box (88.9mm spacing) with vertical screw head clearance\n")

# Ventilation slots - front style (horizontal through walls, 3mm wide)
# Top edge vent slots
for i in range(vent_slot_count):
    x = case_length / 2.0 + (i - (vent_slot_count - 1) / 2.0) * vent_spacing
    if x < vent_corner_margin or x > case_length - vent_corner_margin:
        continue
    slot = Part.makeBox(vent_slot_length, vent_slot_width, vent_slot_height)
    slot.translate(App.Vector(-vent_slot_length / 2.0, -vent_slot_width / 2.0, -vent_slot_height / 2.0))
    slot = slot.rotate(App.Vector(0, 0, 0), App.Vector(1, 0, 0), 90)
    slot.translate(App.Vector(x, wall_thickness / 2.0, case_height / 2.0))
    try:
        shell = shell.cut(slot)
        App.Console.PrintMessage("Cut top vent slot %d\n" % i)
    except Exception as ex:
        App.Console.PrintWarning("Top vent slot %d failed: %s\n" % (i, ex))

# Bottom edge vent slots
for i in range(vent_slot_count):
    x = case_length / 2.0 + (i - (vent_slot_count - 1) / 2.0) * vent_spacing
    if x < vent_corner_margin or x > case_length - vent_corner_margin:
        continue
    slot = Part.makeBox(vent_slot_length, vent_slot_width, vent_slot_height)
    slot.translate(App.Vector(-vent_slot_length / 2.0, -vent_slot_width / 2.0, -vent_slot_height / 2.0))
    slot = slot.rotate(App.Vector(0, 0, 0), App.Vector(1, 0, 0), 90)
    slot.translate(App.Vector(x, case_width - wall_thickness / 2.0, case_height / 2.0))
    try:
        shell = shell.cut(slot)
        App.Console.PrintMessage("Cut bottom vent slot %d\n" % i)
    except Exception as ex:
        App.Console.PrintWarning("Bottom vent slot %d failed: %s\n" % (i, ex))

# Left edge vent slots
for i in range(vent_slot_count_side):
    y = case_width / 2.0 + (i - (vent_slot_count_side - 1) / 2.0) * vent_spacing
    if y < vent_corner_margin or y > case_width - vent_corner_margin:
        continue
    slot = Part.makeBox(vent_slot_length, vent_slot_width, vent_slot_height)
    slot.translate(App.Vector(-vent_slot_length / 2.0, -vent_slot_width / 2.0, -vent_slot_height / 2.0))
    slot = slot.rotate(App.Vector(0, 0, 0), App.Vector(1, 0, 0), 90)
    slot = slot.rotate(App.Vector(0, 0, 0), App.Vector(0, 0, 1), 90)
    slot.translate(App.Vector(wall_thickness / 2.0, y, case_height / 2.0))
    try:
        shell = shell.cut(slot)
        App.Console.PrintMessage("Cut left vent slot %d\n" % i)
    except Exception as ex:
        App.Console.PrintWarning("Left vent slot %d failed: %s\n" % (i, ex))

# Right edge vent slots
for i in range(vent_slot_count_side):
    y = case_width / 2.0 + (i - (vent_slot_count_side - 1) / 2.0) * vent_spacing
    if y < vent_corner_margin or y > case_width - vent_corner_margin:
        continue
    slot = Part.makeBox(vent_slot_length, vent_slot_width, vent_slot_height)
    slot.translate(App.Vector(-vent_slot_length / 2.0, -vent_slot_width / 2.0, -vent_slot_height / 2.0))
    slot = slot.rotate(App.Vector(0, 0, 0), App.Vector(1, 0, 0), 90)
    slot = slot.rotate(App.Vector(0, 0, 0), App.Vector(0, 0, 1), 90)
    slot.translate(App.Vector(case_length - wall_thickness / 2.0, y, case_height / 2.0))
    try:
        shell = shell.cut(slot)
        App.Console.PrintMessage("Cut right vent slot %d\n" % i)
    except Exception as ex:
        App.Console.PrintWarning("Right vent slot %d failed: %s\n" % (i, ex))

# ---------- Add Features ----------

if include_back_lip:
    # Perimeter seating lip near top
    lip_outer = Part.makeBox(case_length - 2 * wall_thickness,
                             case_width - 2 * wall_thickness,
                             lip_height)
    lip_outer.translate(App.Vector(wall_thickness, wall_thickness, case_height - lip_height))

    lip_inner = Part.makeBox(case_length - 2 * wall_thickness - 2 * lip_depth,
                             case_width - 2 * wall_thickness - 2 * lip_depth,
                             lip_height + 0.2)
    lip_inner.translate(App.Vector(wall_thickness + lip_depth, 
                                   wall_thickness + lip_depth, 
                                   case_height - lip_height - 0.1))

    lip = lip_outer.cut(lip_inner)
    try:
        shell = shell.fuse(lip)
        App.Console.PrintMessage("Added perimeter seating lip\n")
    except Exception as ex:
        App.Console.PrintWarning("Lip fuse failed: %s\n" % ex)

# Integrated print supports under lip (thin ribs) - optional
if include_back_lip and add_print_supports:
    # Top support rib under lip
    try:
        top_support = Part.makeBox(case_length - 2 * wall_thickness - 2 * lip_depth,
                                   support_thickness,
                                   lip_height - lip_top_margin)
        top_support.translate(App.Vector(wall_thickness + lip_depth,
                                         wall_thickness + lip_depth,
                                         case_height - lip_height - (lip_height - lip_top_margin)))
        shell = shell.fuse(top_support)
        App.Console.PrintMessage("Added top lip print support rib\n")
    except Exception as ex:
        App.Console.PrintWarning("Top support rib fuse failed: %s\n" % ex)

    # Bottom support rib under lip
    try:
        bottom_support = Part.makeBox(case_length - 2 * wall_thickness - 2 * lip_depth,
                                      support_thickness,
                                      lip_height - lip_top_margin)
        bottom_support.translate(App.Vector(wall_thickness + lip_depth,
                                            case_width - wall_thickness - lip_depth - support_thickness,
                                            case_height - lip_height - (lip_height - lip_top_margin)))
        shell = shell.fuse(bottom_support)
        App.Console.PrintMessage("Added bottom lip print support rib\n")
    except Exception as ex:
        App.Console.PrintWarning("Bottom support rib fuse failed: %s\n" % ex)

    # Left support rib under lip
    try:
        left_support = Part.makeBox(support_thickness,
                                    case_width - 2 * wall_thickness - 2 * lip_depth,
                                    lip_height - lip_top_margin)
        left_support.translate(App.Vector(wall_thickness + lip_depth,
                                          wall_thickness + lip_depth,
                                          case_height - lip_height - (lip_height - lip_top_margin)))
        shell = shell.fuse(left_support)
        App.Console.PrintMessage("Added left lip print support rib\n")
    except Exception as ex:
        App.Console.PrintWarning("Left support rib fuse failed: %s\n" % ex)

    # Right support rib under lip
    try:
        right_support = Part.makeBox(support_thickness,
                                     case_width - 2 * wall_thickness - 2 * lip_depth,
                                     lip_height - lip_top_margin)
        right_support.translate(App.Vector(case_length - wall_thickness - lip_depth - support_thickness,
                                           wall_thickness + lip_depth,
                                           case_height - lip_height - (lip_height - lip_top_margin)))
        shell = shell.fuse(right_support)
        App.Console.PrintMessage("Added right lip print support rib\n")
    except Exception as ex:
        App.Console.PrintWarning("Right support rib fuse failed: %s\n" % ex)

if include_back_lip:
    # ---------- Add snap-fit slots ----------
    # Slots cut into the perimeter lip to receive front case tabs
    for snap in snap_positions:
        side = snap['side']
        pos = snap['pos']
        
        # Slot dimensions with clearance
        slot_width = snap_tab_width + 2 * snap_slot_clearance
        slot_length = snap_tab_length + snap_slot_clearance
        slot_height = snap_tab_height + snap_tab_undercut + snap_slot_clearance
        
        # Create slot box that cuts through perimeter lip, leaving a small top shelf
        if side == 'top':
            # Top side - slot cuts through the lip inward
            slot = Part.makeBox(slot_width, slot_length, slot_height)
            slot.translate(App.Vector(pos - slot_width / 2.0,
                                      wall_thickness,
                                      case_height - slot_height - lip_top_margin))
        elif side == 'bottom':
            # Bottom side - slot cuts through the lip inward
            slot = Part.makeBox(slot_width, slot_length, slot_height)
            slot.translate(App.Vector(pos - slot_width / 2.0,
                                      case_width - wall_thickness - slot_length,
                                      case_height - slot_height - lip_top_margin))
        elif side == 'left':
            # Left side - slot cuts through the lip inward
            slot = Part.makeBox(slot_length, slot_width, slot_height)
            slot.translate(App.Vector(wall_thickness,
                                      pos - slot_width / 2.0,
                                      case_height - slot_height - lip_top_margin))
        elif side == 'right':
            # Right side - slot cuts through the lip inward
            slot = Part.makeBox(slot_length, slot_width, slot_height)
            slot.translate(App.Vector(case_length - wall_thickness - slot_length,
                                      pos - slot_width / 2.0,
                                      case_height - slot_height - lip_top_margin))
        
        try:
            shell = shell.cut(slot)
            App.Console.PrintMessage("Cut snap slot on %s side in perimeter lip\n" % side)
        except Exception as ex:
            App.Console.PrintWarning("Snap slot %s cut failed: %s\n" % (side, ex))

# ---------- Add continuous inner perimeter lip for front case alignment ----------
# Continuous lip on the INSIDE of the walls that extends all the way around
# This creates a raised inner ledge that the front case sits against
lip_width = 2.0  # Width of the lip projecting inward from wall
lip_protrusion_above = 6.0  # Height extending above wall top
lip_total_height = (case_height - bottom_thickness) + lip_protrusion_above  # Full height from bottom

try:
    # Inner dimensions where the lip sits (just inside the walls)
    inner_start_x = wall_thickness
    inner_start_y = wall_thickness
    inner_end_x = case_length - wall_thickness
    inner_end_y = case_width - wall_thickness
    
    # Create four rectangular pieces that form the continuous inner lip
    
    # Top wall lip (runs full length)
    top_lip = Part.makeBox(
        case_length - 2 * wall_thickness,
        lip_width,
        lip_total_height
    )
    top_lip.translate(App.Vector(
        wall_thickness,
        inner_end_y - lip_width,
        bottom_thickness
    ))
    shell = shell.fuse(top_lip)
    
    # Bottom wall lip (runs full length)
    bottom_lip = Part.makeBox(
        case_length - 2 * wall_thickness,
        lip_width,
        lip_total_height
    )
    bottom_lip.translate(App.Vector(
        wall_thickness,
        wall_thickness,
        bottom_thickness
    ))
    shell = shell.fuse(bottom_lip)
    
    # Left wall lip (runs full width, filling corners)
    left_lip = Part.makeBox(
        lip_width,
        case_width - 2 * wall_thickness,
        lip_total_height
    )
    left_lip.translate(App.Vector(
        wall_thickness,
        wall_thickness,
        bottom_thickness
    ))
    shell = shell.fuse(left_lip)
    
    # Right wall lip (runs full width, filling corners)
    right_lip = Part.makeBox(
        lip_width,
        case_width - 2 * wall_thickness,
        lip_total_height
    )
    right_lip.translate(App.Vector(
        inner_end_x - lip_width,
        wall_thickness,
        bottom_thickness
    ))
    shell = shell.fuse(right_lip)
    
    App.Console.PrintMessage("Added continuous inner perimeter lip (4 walls, corners filled)\n")
    
    # Add screw head clearance cutouts for the two center mounting holes
    # These must be cut AFTER the lip is added, otherwise the lip covers them
    screw_head_diameter = 8.0
    screw_head_height = lip_total_height + 1.0  # Cut through entire lip height
    
    for y_offset in [-single_gang_spacing_y / 2.0, single_gang_spacing_y / 2.0]:
        screw_clearance = Part.makeCylinder(
            screw_head_diameter / 2.0,
            screw_head_height,
            App.Vector(case_length / 2.0, case_width / 2.0 + y_offset, bottom_thickness),
            App.Vector(0, 0, 1)
        )
        shell = shell.cut(screw_clearance)
    
    App.Console.PrintMessage("Cut screw head clearance in lip for center mounting holes\n")
    
    # Add 1mm holes in center of left and right lip protrusions
    hole_diameter = 1.0
    hole_length = lip_width + 3.0  # Increased length to ensure full penetration through lip
    hole_height = case_height + 3.0  # 3mm above wall top (center of 6mm protrusion)
    
    # Left side hole - center of left lip in Y direction, 3mm above wall top in Z
    left_hole = Part.makeCylinder(
        hole_diameter / 2.0,
        hole_length,
        App.Vector(wall_thickness - 0.5, case_width / 2.0, hole_height),
        App.Vector(1, 0, 0)  # Hole runs along X axis (through the lip width)
    )
    shell = shell.cut(left_hole)
    
    # Right side hole - center of right lip in Y direction, 3mm above wall top in Z
    right_hole = Part.makeCylinder(
        hole_diameter / 2.0,
        hole_length,
        App.Vector(inner_end_x - lip_width - 1.5, case_width / 2.0, hole_height),
        App.Vector(1, 0, 0)  # Hole runs along X axis (through the lip width)
    )
    shell = shell.cut(right_hole)
    
    App.Console.PrintMessage("Added 1mm holes in left and right lip protrusions (3mm above wall top)\n")
    
    # Re-cut ventilation slots AFTER lip is added to ensure they penetrate the lip
    # Top edge vent slots
    for i in range(vent_slot_count):
        x = case_length / 2.0 + (i - (vent_slot_count - 1) / 2.0) * vent_spacing
        if x < vent_corner_margin or x > case_length - vent_corner_margin:
            continue
        slot = Part.makeBox(vent_slot_length, vent_slot_width, vent_slot_height)
        slot.translate(App.Vector(-vent_slot_length / 2.0, -vent_slot_width / 2.0, -vent_slot_height / 2.0))
        slot = slot.rotate(App.Vector(0, 0, 0), App.Vector(1, 0, 0), 90)
        slot.translate(App.Vector(x, wall_thickness / 2.0, case_height / 2.0))
        try:
            shell = shell.cut(slot)
        except Exception as ex:
            pass
    
    # Bottom edge vent slots
    for i in range(vent_slot_count):
        x = case_length / 2.0 + (i - (vent_slot_count - 1) / 2.0) * vent_spacing
        if x < vent_corner_margin or x > case_length - vent_corner_margin:
            continue
        slot = Part.makeBox(vent_slot_length, vent_slot_width, vent_slot_height)
        slot.translate(App.Vector(-vent_slot_length / 2.0, -vent_slot_width / 2.0, -vent_slot_height / 2.0))
        slot = slot.rotate(App.Vector(0, 0, 0), App.Vector(1, 0, 0), 90)
        slot.translate(App.Vector(x, case_width - wall_thickness / 2.0, case_height / 2.0))
        try:
            shell = shell.cut(slot)
        except Exception as ex:
            pass
    
    # Left edge vent slots
    for i in range(vent_slot_count_side):
        y = case_width / 2.0 + (i - (vent_slot_count_side - 1) / 2.0) * vent_spacing
        if y < vent_corner_margin or y > case_width - vent_corner_margin:
            continue
        slot = Part.makeBox(vent_slot_length, vent_slot_width, vent_slot_height)
        slot.translate(App.Vector(-vent_slot_length / 2.0, -vent_slot_width / 2.0, -vent_slot_height / 2.0))
        slot = slot.rotate(App.Vector(0, 0, 0), App.Vector(1, 0, 0), 90)
        slot = slot.rotate(App.Vector(0, 0, 0), App.Vector(0, 0, 1), 90)
        slot.translate(App.Vector(wall_thickness / 2.0, y, case_height / 2.0))
        try:
            shell = shell.cut(slot)
        except Exception as ex:
            pass
    
    # Right edge vent slots
    for i in range(vent_slot_count_side):
        y = case_width / 2.0 + (i - (vent_slot_count_side - 1) / 2.0) * vent_spacing
        if y < vent_corner_margin or y > case_width - vent_corner_margin:
            continue
        slot = Part.makeBox(vent_slot_length, vent_slot_width, vent_slot_height)
        slot.translate(App.Vector(-vent_slot_length / 2.0, -vent_slot_width / 2.0, -vent_slot_height / 2.0))
        slot = slot.rotate(App.Vector(0, 0, 0), App.Vector(1, 0, 0), 90)
        slot = slot.rotate(App.Vector(0, 0, 0), App.Vector(0, 0, 1), 90)
        slot.translate(App.Vector(case_length - wall_thickness / 2.0, y, case_height / 2.0))
        try:
            shell = shell.cut(slot)
        except Exception as ex:
            pass
    
    App.Console.PrintMessage("Re-cut ventilation slots through lip\n")
    
except Exception as ex:
    App.Console.PrintWarning("Continuous inner perimeter lip failed: %s\n" % ex)

# ---------- Mirror on Y-axis ----------
# Mirror to match front case orientation
shell = shell.mirror(App.Vector(0, 0, 0), App.Vector(1, 0, 0))
App.Console.PrintMessage("Mirrored shell on Y-axis\n")

# ---------- Export ----------
part_obj = doc.addObject("Part::Feature", "BackCaseShell")
part_obj.Shape = shell

# Determine output directory
try:
    script_dir = os.path.dirname(os.path.abspath(__file__))
except NameError:
    script_dir = os.getcwd()
out_dir = os.path.join(script_dir, "freecad_outputs")
if not os.path.exists(out_dir):
    os.makedirs(out_dir)

step_path = os.path.join(out_dir, "back_case_wall_freecad.step")
stl_path = os.path.join(out_dir, "back_case_wall_freecad.stl")

# Export STEP
Part.export([part_obj], step_path)

# Export STL with highest quality mesh for 3D printing
# Use same method as FreeCAD GUI export with premium settings
import Mesh
mesh = Mesh.Mesh()
mesh.addFacets(MeshPart.meshFromShape(
    Shape=shell,
    LinearDeflection=0.005,      # 5 microns - very fine detail
    AngularDeflection=0.174533,  # 10 degrees in radians - smooth curves
    Relative=False
).Facets)
mesh.write(stl_path)

App.Console.PrintMessage("Exported STEP to: %s\n" % step_path)
App.Console.PrintMessage("Exported STL to: %s\n" % stl_path)

# Save document
fcstd_path = os.path.join(out_dir, "back_case_wall_freecad.FCStd")
doc.saveAs(fcstd_path)
App.Console.PrintMessage("Saved FCStd to: %s\n" % fcstd_path)

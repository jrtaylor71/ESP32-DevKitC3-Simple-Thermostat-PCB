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

# Rounded corners
corner_r = 4.0

# Snap-fit parameters - slots in perimeter lip to receive front case tabs
snap_tab_width = 12.0      # Must match front case
snap_tab_length = 6.0      # Must match front case
snap_tab_height = 2.5      # Must match front case
snap_tab_undercut = 1.0    # Must match front case
snap_slot_clearance = 0.2  # Extra clearance for assembly
snap_hook_thickness = 3.0  # Thicker hook beam for strength
snap_hook_height = 6.5     # Hook rise above wall top (compensated for taller barb)
snap_hook_offset_z = 0.0   # Hooks start at wall top (no offset)
snap_hook_protrusion = 0.0 # Hooks are hidden on the inside; no external protrusion
snap_hook_overhang = 1.3   # Overhang (barb) depth into window region for better grip
snap_hook_barb_height = 1.5  # Vertical height of overhang barb near the hook tip
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
vent_slot_height = 20.0
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
# Top and bottom center holes
for y_offset in [-wall_mount_spacing_y / 2.0, wall_mount_spacing_y / 2.0]:
    mount_hole = Part.makeCylinder(wall_mount_hole_dia / 2.0, bottom_thickness + 1.0,
                                    App.Vector(case_length / 2.0,
                                              case_width / 2.0 + y_offset,
                                              -0.5),
                                    App.Vector(0, 0, 1))
    shell = shell.cut(mount_hole)
App.Console.PrintMessage("Cut 2 additional center mounting holes\n")

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

else:
    # ---------- Add back-case snap hooks (cantilever) ----------
    # Hooks protrude through front-case latch windows; beams stay in wall region
    window_width = snap_tab_width + 2 * snap_slot_clearance
    for snap in snap_positions:
        side = snap['side']
        pos = snap['pos']
        # Hooks extend down the inside wall and rise above wall top
        z_hook_base = case_height - snap_hook_base_extension
        
        if side == 'top':
            # Inside of top wall: vertical hook beam extending down wall + upward + outward barb
            hook_beam = Part.makeBox(window_width, snap_hook_thickness, snap_hook_base_extension + snap_hook_height)
            hook_beam.translate(App.Vector(pos - window_width / 2.0,
                                           case_width - wall_thickness - snap_hook_thickness,
                                           z_hook_base))
            barb = Part.makeBox(window_width, snap_hook_overhang, snap_hook_barb_height)
            barb.translate(App.Vector(pos - window_width / 2.0,
                                      case_width - wall_thickness,
                                      case_height + snap_hook_height - snap_hook_barb_height))
            hook = hook_beam.fuse(barb)
            
        elif side == 'bottom':
            # Inside of bottom wall
            hook_beam = Part.makeBox(window_width, snap_hook_thickness, snap_hook_base_extension + snap_hook_height)
            hook_beam.translate(App.Vector(pos - window_width / 2.0,
                                           wall_thickness,
                                           z_hook_base))
            barb = Part.makeBox(window_width, snap_hook_overhang, snap_hook_barb_height)
            barb.translate(App.Vector(pos - window_width / 2.0,
                                      wall_thickness - snap_hook_overhang,
                                      case_height + snap_hook_height - snap_hook_barb_height))
            hook = hook_beam.fuse(barb)
            
        elif side == 'left':
            # Inside of left wall
            hook_beam = Part.makeBox(snap_hook_thickness, window_width, snap_hook_base_extension + snap_hook_height)
            hook_beam.translate(App.Vector(wall_thickness,
                                           pos - window_width / 2.0,
                                           z_hook_base))
            barb = Part.makeBox(snap_hook_overhang, window_width, snap_hook_barb_height)
            barb.translate(App.Vector(wall_thickness - snap_hook_overhang,
                                      pos - window_width / 2.0,
                                      case_height + snap_hook_height - snap_hook_barb_height))
            hook = hook_beam.fuse(barb)
            
        elif side == 'right':
            # Inside of right wall
            hook_beam = Part.makeBox(snap_hook_thickness, window_width, snap_hook_base_extension + snap_hook_height)
            hook_beam.translate(App.Vector(case_length - wall_thickness - snap_hook_thickness,
                                           pos - window_width / 2.0,
                                           z_hook_base))
            barb = Part.makeBox(snap_hook_overhang, window_width, snap_hook_barb_height)
            barb.translate(App.Vector(case_length - wall_thickness,
                                      pos - window_width / 2.0,
                                      case_height + snap_hook_height - snap_hook_barb_height))
            hook = hook_beam.fuse(barb)
            
        try:
            shell = shell.fuse(hook)
            App.Console.PrintMessage("Added snap hook on %s wall\n" % side)
        except Exception as ex:
            App.Console.PrintWarning("Snap hook %s fuse failed: %s\n" % (side, ex))

# ---------- Add triangular supports at hook bases ----------
# Create triangular support: wide base against wall, tapers up to inner hook edge
try:
    z_hook_base = case_height - snap_hook_base_extension
    support_height = 1.5  # Height of triangle support (extends upward from wall)
    
    for snap in snap_positions:
        side = snap['side']
        pos = snap['pos']
        window_width = snap_tab_width + 2 * snap_slot_clearance
        half_width = window_width / 2.0
        
        if side == 'top':
            # Triangle in Y-Z plane: extrude along X across tab width
            # Wide base at wall surface, taper point at inner hook edge
            tri_pts = [
                App.Vector(0, case_width - wall_thickness, z_hook_base - support_height),  # left at wall, lower
                App.Vector(0, case_width - wall_thickness, z_hook_base),  # right at wall, at tab
                App.Vector(0, case_width - wall_thickness - snap_hook_thickness, z_hook_base)  # apex at inner edge
            ]
            wire = Part.makePolygon(tri_pts + [tri_pts[0]])
            tri_face = Part.Face(wire)
            tri = tri_face.extrude(App.Vector(window_width, 0, 0))
            tri = tri.translate(App.Vector(pos - half_width, 0, 0))
            shell = shell.fuse(tri)
            
        elif side == 'bottom':
            # Triangle in Y-Z plane: extrude along X across tab width
            tri_pts = [
                App.Vector(0, wall_thickness, z_hook_base - support_height),  # left at wall, lower
                App.Vector(0, wall_thickness, z_hook_base),  # right at wall, at tab
                App.Vector(0, wall_thickness + snap_hook_thickness, z_hook_base)  # apex at inner edge
            ]
            wire = Part.makePolygon(tri_pts + [tri_pts[0]])
            tri_face = Part.Face(wire)
            tri = tri_face.extrude(App.Vector(window_width, 0, 0))
            tri = tri.translate(App.Vector(pos - half_width, 0, 0))
            shell = shell.fuse(tri)
            
        elif side == 'left':
            # Triangle in X-Z plane: extrude along Y across tab width
            tri_pts = [
                App.Vector(wall_thickness, 0, z_hook_base - support_height),  # bottom at wall, lower
                App.Vector(wall_thickness, 0, z_hook_base),  # top at wall, at tab
                App.Vector(wall_thickness + snap_hook_thickness, 0, z_hook_base)  # apex at inner edge
            ]
            wire = Part.makePolygon(tri_pts + [tri_pts[0]])
            tri_face = Part.Face(wire)
            tri = tri_face.extrude(App.Vector(0, window_width, 0))
            tri = tri.translate(App.Vector(0, pos - half_width, 0))
            shell = shell.fuse(tri)
            
        elif side == 'right':
            # Triangle in X-Z plane: extrude along Y across tab width
            tri_pts = [
                App.Vector(case_length - wall_thickness, 0, z_hook_base - support_height),  # bottom at wall, lower
                App.Vector(case_length - wall_thickness, 0, z_hook_base),  # top at wall, at tab
                App.Vector(case_length - wall_thickness - snap_hook_thickness, 0, z_hook_base)  # apex at inner edge
            ]
            wire = Part.makePolygon(tri_pts + [tri_pts[0]])
            tri_face = Part.Face(wire)
            tri = tri_face.extrude(App.Vector(0, window_width, 0))
            tri = tri.translate(App.Vector(0, pos - half_width, 0))
            shell = shell.fuse(tri)
    
    App.Console.PrintMessage("Added triangular supports to all snap hooks\n")
except Exception as ex:
    App.Console.PrintWarning("Adding triangular supports failed: %s\n" % ex)

# ---------- Add L-shaped corner alignment tabs ----------
# L-shaped tabs in each corner that extend from bottom to 3mm above wall top for easy printing
tab_arm_width = 2.0  # Width of each arm of the L
tab_arm_length = 6.0  # Length of each arm of the L
tab_protrusion_above = 3.0  # Height extending above wall top
tab_total_height = (case_height - bottom_thickness) + tab_protrusion_above  # Full height from bottom

# Bottom-left corner - arms extend right and up
try:
    arm_right = Part.makeBox(tab_arm_length, tab_arm_width, tab_total_height)
    arm_up = Part.makeBox(tab_arm_width, tab_arm_length, tab_total_height)
    arm_right.translate(App.Vector(wall_thickness, wall_thickness, bottom_thickness))
    arm_up.translate(App.Vector(wall_thickness, wall_thickness, bottom_thickness))
    shell = shell.fuse(arm_right)
    shell = shell.fuse(arm_up)
except Exception as ex:
    App.Console.PrintWarning("Bottom-left corner tab failed: %s\n" % ex)

# Bottom-right corner - arms extend left and up
try:
    arm_left = Part.makeBox(tab_arm_length, tab_arm_width, tab_total_height)
    arm_up = Part.makeBox(tab_arm_width, tab_arm_length, tab_total_height)
    arm_left.translate(App.Vector(case_length - wall_thickness - tab_arm_length, wall_thickness, bottom_thickness))
    arm_up.translate(App.Vector(case_length - wall_thickness - tab_arm_width, wall_thickness, bottom_thickness))
    shell = shell.fuse(arm_left)
    shell = shell.fuse(arm_up)
except Exception as ex:
    App.Console.PrintWarning("Bottom-right corner tab failed: %s\n" % ex)

# Top-right corner - arms extend left and down
try:
    arm_left = Part.makeBox(tab_arm_length, tab_arm_width, tab_total_height)
    arm_down = Part.makeBox(tab_arm_width, tab_arm_length, tab_total_height)
    arm_left.translate(App.Vector(case_length - wall_thickness - tab_arm_length, case_width - wall_thickness - tab_arm_width, bottom_thickness))
    arm_down.translate(App.Vector(case_length - wall_thickness - tab_arm_width, case_width - wall_thickness - tab_arm_length, bottom_thickness))
    shell = shell.fuse(arm_left)
    shell = shell.fuse(arm_down)
except Exception as ex:
    App.Console.PrintWarning("Top-right corner tab failed: %s\n" % ex)

# Top-left corner - arms extend right and down
try:
    arm_right = Part.makeBox(tab_arm_length, tab_arm_width, tab_total_height)
    arm_down = Part.makeBox(tab_arm_width, tab_arm_length, tab_total_height)
    arm_right.translate(App.Vector(wall_thickness, case_width - wall_thickness - tab_arm_width, bottom_thickness))
    arm_down.translate(App.Vector(wall_thickness, case_width - wall_thickness - tab_arm_length, bottom_thickness))
    shell = shell.fuse(arm_right)
    shell = shell.fuse(arm_down)
except Exception as ex:
    App.Console.PrintWarning("Top-left corner tab failed: %s\n" % ex)

App.Console.PrintMessage("Added 4 L-shaped corner alignment tabs (full height)\n")

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

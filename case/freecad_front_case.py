# FreeCAD script to build front case shell with edge-only fillet at face/wall junction
# Run with: freecadcmd case/freecad_front_case.py

import FreeCAD as App
import Part
import Mesh
import MeshPart
import os

# ---------- Parameters (mirrored from front_case_display.scad) ----------
pcb_length = 133.0
pcb_width = 89.5
pcb_thickness = 1.6
pcb_clearance = 3.0

# PCB mounting holes
pcb_mount_holes = [
    [3.5, 3.5],
    [3.5, 86.0],
    [129.5, 3.5],
    [129.5, 86.0]
]

# Case parameters
face_thickness = 1.8
wall_thickness = 5.0
standoff_height = 17.3
standoff_diameter = 15.0
hole_diameter = 2.7

display_clearance = 18.9
# Inner cavity dimensions (what PCB needs)
inner_length = pcb_length + (2 * pcb_clearance)
inner_width = pcb_width + (2 * pcb_clearance)
# Outer case dimensions (cavity + walls)
case_length = inner_length + (2 * wall_thickness)
case_width = inner_width + (2 * wall_thickness)
case_height = face_thickness + display_clearance

# Display opening
display_holes = [
    [45.5, 16.1],
    [45.5, 65.0],
    [128.61, 16.1],
    [128.61, 65.0]
]
display_center_x = (display_holes[0][0] + display_holes[3][0]) / 2.0 + 5.6
display_center_y = (display_holes[0][1] + display_holes[3][1]) / 2.0
display_width = 68.0
display_height = 50.0
display_x = wall_thickness + pcb_clearance + display_center_x - display_width / 2.0
display_y = wall_thickness + pcb_clearance + display_center_y - display_height / 2.0

# Sensor holes
ldr_diameter = 5.5
ldr_x = wall_thickness + pcb_clearance + 104.8 - 0.5
ldr_y = wall_thickness + pcb_clearance + 6.5

sensor_box_width = 20.0
sensor_box_height = 32.2
sensor_box_depth = 17.5
sensor_box_wall = 1.5
# Sensor opening position (post-mirror offsets, measured from INSIDE perimeter walls)
# Right offset from inside right wall to right edge of cutout: 11.0 mm
# Top offset from inside top wall to top edge of cutout: 15.83 mm
# Opening is 12.5 x 6.0 then rotated 90°, so final size is 6.0 (X) by 12.5 (Y).
# Mirror on X: right edge in final corresponds to left edge pre-mirror.
sensor_x = (wall_thickness + 11.5) + (6.0 / 2.0)
sensor_y = (case_width - wall_thickness - 15.83) - (12.5 / 2.0)

corner_r = 4.0
face_edge_radius = 4.0  # Outer fillet radius at face/wall junction
inner_face_edge_radius = 0.5  # Smaller radius for inner reinforcement fillet (to avoid BRep failures)

# Snap-fit parameters - tabs on front case rim that clip into back wall catches
snap_tab_width = 12.0      # Width of snap tab
snap_tab_length = 6.0      # How far tab extends inward from rim (flex length)
snap_tab_height = 2.5      # Height/thickness of snap tab (strength)
snap_tab_undercut = 1.0    # Undercut depth for locking
snap_slot_clearance = 0.2  # Clearance for easy snapping
snap_latch_overtravel = 5.0  # Tab tip protrusion above wall (mm)
add_tab_print_supports = True
tab_support_thickness = 0.6  # thin sacrificial rib thickness

# Snap tab positions - 4 tabs on inside rim (top, bottom, left, right center)
snap_positions = [
    {'side': 'top', 'pos': case_length / 2.0},     # Top center
    {'side': 'bottom', 'pos': case_length / 2.0},  # Bottom center
    {'side': 'left', 'pos': case_width / 2.0},     # Left center
    {'side': 'right', 'pos': case_width / 2.0}     # Right center
]

# ---------- Helpers ----------
TOL = 1e-6

def near(a, b, tol=TOL):
    return abs(a - b) <= tol

# ---------- Build outer and inner boxes ----------
doc = App.newDocument("FrontCaseFreeCAD")

# Outer solid (origin at lower-left-back)
outer = Part.makeBox(case_length, case_width, case_height)

# Inner box to create wall cavity (preserves straight walls and face thickness)
inner = Part.makeBox(inner_length, inner_width, case_height - face_thickness)
inner.Placement.Base = App.Vector(wall_thickness, wall_thickness, face_thickness)

shell = outer.cut(inner)

# Fillet the 4 vertical corner edges to match rounded corners
try:
    corner_edges = []
    for e in shell.Edges:
        v1 = e.Vertexes[0].Point
        v2 = e.Vertexes[1].Point
        # Vertical edge: x and y same at both vertices, z different
        if near(v1.x, v2.x) and near(v1.y, v2.y) and not near(v1.z, v2.z):
            # At the corners: x is 0 or case_length; y is 0 or case_width
            if (near(v1.x, 0) or near(v1.x, case_length)) and (near(v1.y, 0) or near(v1.y, case_width)):
                corner_edges.append(e)
    if corner_edges:
        shell = shell.makeFillet(corner_r, corner_edges)
except Exception as ex:
    App.Console.PrintWarning("Corner fillet failed (non-critical): %s\n" % ex)

# ---------- Cutouts (subtract from shell) ----------
# LDR07 hole through front face
ldr_hole = Part.makeCylinder(ldr_diameter / 2.0, face_thickness + 2, 
                             App.Vector(ldr_x, ldr_y, -1), App.Vector(0, 0, 1))
shell = shell.cut(ldr_hole)

# LDR tube/collar - 10mm tall, 6mm inner diameter
ldr_tube_height = 10.0
ldr_tube_inner_dia = 6.0
ldr_tube_outer_dia = 8.0  # 1mm wall thickness
ldr_tube_outer = Part.makeCylinder(ldr_tube_outer_dia / 2.0, ldr_tube_height,
                                    App.Vector(ldr_x, ldr_y, 0), App.Vector(0, 0, 1))
ldr_tube_inner = Part.makeCylinder(ldr_tube_inner_dia / 2.0, ldr_tube_height + 0.2,
                                    App.Vector(ldr_x, ldr_y, -0.1), App.Vector(0, 0, 1))
ldr_tube = ldr_tube_outer.cut(ldr_tube_inner)
try:
    shell = shell.fuse(ldr_tube)
    App.Console.PrintMessage("Added LDR light tube\n")
except Exception as ex:
    App.Console.PrintWarning("LDR tube fuse failed: %s\n" % ex)

# Temp/Humidity sensor front opening (AHT20 footprint 12.5 x 6.0, rotated 90°)
# Match SCAD: center at origin, rotate, then translate to sensor_x, sensor_y
sensor_opening_shape = Part.makeBox(12.5, 6.0, face_thickness + 0.2)
sensor_opening_shape.translate(App.Vector(-12.5/2.0, -6.0/2.0, -0.1))
sensor_opening_shape = sensor_opening_shape.rotate(App.Vector(0, 0, 0), App.Vector(0, 0, 1), 90)
sensor_opening_shape.translate(App.Vector(sensor_x, sensor_y, 0))
shell = shell.cut(sensor_opening_shape)

# Add protective lattice mesh across sensor opening - thin ribs that can be cut out if needed
mesh_thickness = 0.4  # Thin mesh ribs
mesh_depth = face_thickness / 3.0  # 1/3 face thickness
mesh_spacing = 2.5  # Spacing between diagonal bars
lattice_bars = []

# Create diagonal bars in one direction (/)
for offset in range(-3, 4):  # Multiple diagonal bars
    bar = Part.makeBox(mesh_thickness, 15.0, mesh_depth)  # Long enough to cover opening
    bar.translate(App.Vector(offset * mesh_spacing - mesh_thickness/2, -7.5, 0))
    bar = bar.rotate(App.Vector(0, 0, 0), App.Vector(0, 0, 1), 45)
    lattice_bars.append(bar)

# Create diagonal bars in other direction (\)
for offset in range(-3, 4):  # Multiple diagonal bars
    bar = Part.makeBox(mesh_thickness, 15.0, mesh_depth)
    bar.translate(App.Vector(offset * mesh_spacing - mesh_thickness/2, -7.5, 0))
    bar = bar.rotate(App.Vector(0, 0, 0), App.Vector(0, 0, 1), -45)
    lattice_bars.append(bar)

# Combine all bars
lattice_mesh = lattice_bars[0]
for bar in lattice_bars[1:]:
    try:
        lattice_mesh = lattice_mesh.fuse(bar)
    except:
        pass

# Rotate 90° and translate to sensor position (same as opening)
lattice_mesh = lattice_mesh.rotate(App.Vector(0, 0, 0), App.Vector(0, 0, 1), 90)
lattice_mesh.translate(App.Vector(sensor_x, sensor_y, 0))
try:
    shell = shell.fuse(lattice_mesh)
    App.Console.PrintMessage("Added lattice mesh to sensor opening\n")
except Exception as ex:
    App.Console.PrintWarning("Sensor mesh fuse failed: %s\n" % ex)

# Sensor protection box: recreate with ONLY bottom + left walls
# Placement is defined by offsets from INSIDE right/top walls (final mirrored part).
# NOTE: The shell is mirrored on the YZ plane later, so X offsets are mirror-aware.
left_wall_offset_from_inside_right = 19.0  # mm from inside right wall to LEFT wall (final)
bottom_wall_inside_offset_from_top = 30.58  # mm from inside top wall to INSIDE of bottom wall

# Final-part reference edges (after mirror)
inside_right_final_x = -wall_thickness
inside_top_final_y = case_width - wall_thickness

# Left wall placement (final coords)
left_wall_inside_final_x = inside_right_final_x - left_wall_offset_from_inside_right
left_wall_outer_final_x = left_wall_inside_final_x - sensor_box_wall

# Bottom wall placement (final coords)
bottom_wall_inside_final_y = inside_top_final_y - bottom_wall_inside_offset_from_top
bottom_wall_outer_final_y = bottom_wall_inside_final_y - sensor_box_wall

# Bottom wall runs left from inside right wall by sensor_box_width
bottom_wall_left_final_x = inside_right_final_x - sensor_box_width
bottom_wall_right_final_x = inside_right_final_x

# Convert final coords to pre-mirror coords (x_pre = -x_final)
left_wall_x = -left_wall_outer_final_x
left_wall_y = bottom_wall_outer_final_y

bottom_wall_x = -bottom_wall_right_final_x
bottom_wall_y = bottom_wall_outer_final_y

# Left wall (vertical)
left_wall = Part.makeBox(sensor_box_wall, sensor_box_height, sensor_box_depth)
left_wall.Placement.Base = App.Vector(left_wall_x, left_wall_y, 0)

# Bottom wall (horizontal)
bottom_wall = Part.makeBox(sensor_box_width + 2.0, sensor_box_wall, sensor_box_depth)
bottom_wall.Placement.Base = App.Vector(bottom_wall_x, bottom_wall_y, 0)

sensor_box = left_wall.fuse(bottom_wall)

# Add sensor box to shell
try:
    shell = shell.fuse(sensor_box)
except Exception as ex:
    App.Console.PrintWarning("Sensor box fuse failed: %s\n" % ex)

# Display opening
display_opening = Part.makeBox(display_width, display_height, face_thickness + 0.2)
display_opening.Placement.Base = App.Vector(display_x, display_y, -0.1)
shell = shell.cut(display_opening)

# Fillet the display opening edges (outer edge at z=0) - important for touch sensitivity
# Try filleting edges individually since batch filleting fails on complex geometry
display_fillet_radius = 1.0  # Small radius for display opening
display_edges = []

for e in shell.Edges:
    if len(e.Vertexes) >= 2:
        v1 = e.Vertexes[0].Point
        v2 = e.Vertexes[1].Point
        # Edges at z=0 within the display opening rectangle
        if near(v1.z, 0) and near(v2.z, 0):
            # Check if edge is within display opening bounds
            in_display_x = (display_x <= v1.x <= display_x + display_width and 
                           display_x <= v2.x <= display_x + display_width)
            in_display_y = (display_y <= v1.y <= display_y + display_height and 
                           display_y <= v2.y <= display_y + display_height)
            
            if (in_display_x or in_display_y) and not (v1.x == v2.x and v1.y == v2.y):
                display_edges.append(e)

if display_edges:
    successes = 0
    for edge in display_edges:
        try:
            shell = shell.makeFillet(display_fillet_radius, [edge])
            successes += 1
        except:
            pass  # Some edges can't be filleted due to geometry - skip silently
    if successes > 0:
        App.Console.PrintMessage("Display opening fillet: %d/%d edges filleted\n" % (successes, len(display_edges)))

# Top edge vent slots - 20mm long horizontal slots through wall (rotate 90° around X)
vent_slot_count = 15
vent_slot_length = 8.0
vent_slot_width = 2.0
vent_slot_height = wall_thickness
vent_spacing = 10.5  # Match back case spacing for alignment
vent_corner_margin = 15.0  # Skip vents near corners for strength
for i in range(vent_slot_count):
    x = case_length / 2.0 + (i - (vent_slot_count - 1) / 2.0) * vent_spacing
    if x < vent_corner_margin or x > case_length - vent_corner_margin:
        continue
    # Create vertical slot then rotate to be horizontal (through Y wall)
    slot = Part.makeBox(vent_slot_length, vent_slot_width, vent_slot_height)
    slot.translate(App.Vector(-vent_slot_length / 2.0, -vent_slot_width / 2.0, -vent_slot_height / 2.0))
    slot = slot.rotate(App.Vector(0, 0, 0), App.Vector(1, 0, 0), 90)  # Rotate around X-axis
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

# Left edge vent slots - rotate 90° around X then 90° around Z
vent_slot_count_side = 10
for i in range(vent_slot_count_side):
    y = case_width / 2.0 + (i - (vent_slot_count_side - 1) / 2.0) * vent_spacing
    if y < vent_corner_margin or y > case_width - vent_corner_margin:
        continue
    slot = Part.makeBox(vent_slot_length, vent_slot_width, vent_slot_height)
    slot.translate(App.Vector(-vent_slot_length / 2.0, -vent_slot_width / 2.0, -vent_slot_height / 2.0))
    slot = slot.rotate(App.Vector(0, 0, 0), App.Vector(1, 0, 0), 90)  # First rotate around X
    slot = slot.rotate(App.Vector(0, 0, 0), App.Vector(0, 0, 1), 90)  # Then rotate around Z
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

# ---------- Add standoffs ----------
for hole in pcb_mount_holes:
    x = wall_thickness + hole[0] + pcb_clearance
    y = wall_thickness + hole[1] + pcb_clearance
    # Standoff cylinder (hollow with hole)
    standoff_outer = Part.makeCylinder(standoff_diameter / 2.0, standoff_height, 
                                        App.Vector(x, y, face_thickness), 
                                        App.Vector(0, 0, 1))
    standoff_hole = Part.makeCylinder(hole_diameter / 2.0, standoff_height + 0.2, 
                                       App.Vector(x, y, face_thickness - 0.1), 
                                       App.Vector(0, 0, 1))
    standoff = standoff_outer.cut(standoff_hole)
    try:
        shell = shell.fuse(standoff)
    except Exception as ex:
        App.Console.PrintWarning("Standoff fuse failed: %s\n" % ex)

# ---------- Cut latch holes in front walls (BEFORE MIRROR) ----------
# Removed latch windows, keeping only the holes on left and right walls
latch_hole_diameter = 2.0  # Small hole for hook/tab push-through
hole_z_position = case_height - 3.0  # 3mm from top of wall

for snap in snap_positions:
    side = snap['side']
    pos = snap['pos']
    
    if side == 'left':
        # Hole pointing inward toward cavity (+X direction), 3mm from top of wall
        hole = Part.makeCylinder(latch_hole_diameter / 2.0, wall_thickness + 2,
                                 App.Vector(-2, pos, hole_z_position),
                                 App.Vector(1, 0, 0))
        try:
            shell = shell.cut(hole)
            App.Console.PrintMessage("Cut latch hole on left wall (3mm from top)\n")
        except Exception as ex:
            App.Console.PrintWarning("Left latch hole cut failed: %s\n" % ex)
            
    elif side == 'right':
        # Hole pointing inward toward cavity (-X direction), 3mm from top of wall
        hole = Part.makeCylinder(latch_hole_diameter / 2.0, wall_thickness + 2,
                                 App.Vector(case_length + 2, pos, hole_z_position),
                                 App.Vector(-1, 0, 0))
        try:
            shell = shell.cut(hole)
            App.Console.PrintMessage("Cut latch hole on right wall (3mm from top)\n")
        except Exception as ex:
            App.Console.PrintWarning("Right latch hole cut failed: %s\n" % ex)

# Perimeter seating lip at top of walls (BEFORE MIRROR)
# Lip dimensions: 2.1mm inward depth, 6mm downward extent, 6mm height
lip_depth = 2.2
lip_height = 6.2

# Top lip (extends inward from top edge)
top_lip = Part.makeBox(case_length - 2 * wall_thickness, lip_depth, lip_height)
top_lip.translate(App.Vector(wall_thickness, case_width - wall_thickness - lip_depth, case_height - lip_height))

# Bottom lip (extends inward from bottom edge)
bottom_lip = Part.makeBox(case_length - 2 * wall_thickness, lip_depth, lip_height)
bottom_lip.translate(App.Vector(wall_thickness, wall_thickness, case_height - lip_height))

# Left lip (extends inward from left edge)
left_lip = Part.makeBox(lip_depth, case_width - 2 * wall_thickness, lip_height)
left_lip.translate(App.Vector(wall_thickness, wall_thickness, case_height - lip_height))

# Right lip (extends inward from right edge)
right_lip = Part.makeBox(lip_depth, case_width - 2 * wall_thickness, lip_height)
right_lip.translate(App.Vector(case_length - wall_thickness - lip_depth, wall_thickness, case_height - lip_height))

# Combine all lips
lip = top_lip.fuse(bottom_lip)
lip = lip.fuse(left_lip)
lip = lip.fuse(right_lip)

try:
    shell = shell.cut(lip)
    App.Console.PrintMessage("Cut perimeter seating lip\n")
except Exception as ex:
    App.Console.PrintWarning("Lip cut failed: %s\n" % ex)


# ---------- Mirror entire shell on Y-axis ----------
# Mirror on YZ plane (flip left/right, X coordinates negated)
# Using mirror() instead of transformGeometry to preserve all cuts
shell = shell.mirror(App.Vector(0, 0, 0), App.Vector(1, 0, 0))
App.Console.PrintMessage("Mirrored shell on Y-axis\n")

# ---------- Fillet the perimeter edges (AFTER mirror) ----------

# First: Inner edge reinforcement at z=face_thickness (inside where face meets cavity)
try:
    inner_edges = []
    App.Console.PrintMessage("Searching for inner reinforcement edges at z=%.2f...\n" % face_thickness)
    
    # Tolerance for inner edge detection - increased to catch edges near but not exactly on perimeter
    edge_tolerance = 1.0  # 1mm tolerance
    
    for e in shell.Edges:
        if len(e.Vertexes) >= 2:
            v1 = e.Vertexes[0].Point
            v2 = e.Vertexes[1].Point
            if near(v1.z, face_thickness) and near(v2.z, face_thickness):
                # Skip edges near display opening (they're broken/complex and can't be filleted)
                # Display is at x: display_x to display_x+display_width, y: display_y to display_y+display_height
                # After mirror, display_x becomes negative
                display_margin = 5.0  # mm margin around display to exclude
                v1_near_display = (-display_x - display_width - display_margin < v1.x < -display_x + display_margin and
                                  display_y - display_margin < v1.y < display_y + display_height + display_margin)
                v2_near_display = (-display_x - display_width - display_margin < v2.x < -display_x + display_margin and
                                  display_y - display_margin < v2.y < display_y + display_height + display_margin)
                
                if v1_near_display or v2_near_display:
                    continue  # Skip edges near display
                
                # Inner perimeter (cavity edge) - after mirror, around x = -wall_thickness or -(case_length - wall_thickness)
                # Use broader tolerance to catch edges broken by display opening
                v1_inner = (abs(v1.x + wall_thickness) < edge_tolerance or abs(v1.x + (case_length - wall_thickness)) < edge_tolerance or 
                           abs(v1.y - wall_thickness) < edge_tolerance or abs(v1.y - (case_width - wall_thickness)) < edge_tolerance)
                v2_inner = (abs(v2.x + wall_thickness) < edge_tolerance or abs(v2.x + (case_length - wall_thickness)) < edge_tolerance or 
                           abs(v2.y - wall_thickness) < edge_tolerance or abs(v2.y - (case_width - wall_thickness)) < edge_tolerance)
                
                if v1_inner and v2_inner:
                    inner_edges.append(e)
    
    App.Console.PrintMessage("Found %d inner edges to fillet with radius %.2f mm\n" % (len(inner_edges), inner_face_edge_radius))
    if inner_edges:
        fallback_radius = max(0.3, min(inner_face_edge_radius * 0.5, 0.8))  # Smaller backup radius for stability
        successes = 0
        failures = 0
        for idx, edge in enumerate(inner_edges):
            try:
                shell = shell.makeFillet(inner_face_edge_radius, [edge])
                successes += 1
            except Exception as ex:
                try:
                    shell = shell.makeFillet(fallback_radius, [edge])
                    successes += 1
                except Exception as ex2:
                    failures += 1
                    # Silently skip - these are edges broken by display opening that can't be filleted
        if successes > 0:
            App.Console.PrintMessage("Inner fillet per-edge applied: %d success, %d skipped\n" % (successes, failures))
        else:
            App.Console.PrintMessage("Inner fillet: all edges skipped (likely broken by cutouts)\n")
except Exception as ex:
    App.Console.PrintWarning("Inner edge selection failed: %s\n" % ex)

# Second: Outer edge rounding at z=0 (outside where face meets outer walls)
try:
    outer_edges = []
    App.Console.PrintMessage("Searching for outer fillet edges at z=0 (face bottom, outer perimeter)...\n")
    
    for e in shell.Edges:
        if len(e.Vertexes) >= 2:
            v1 = e.Vertexes[0].Point
            v2 = e.Vertexes[1].Point
            # Both vertices at z=0 (bottom of face, top of outer wall)
            if near(v1.z, 0) and near(v2.z, 0):
                # Outermost perimeter - after mirror: x from -case_length to 0, y from 0 to case_width
                v1_outer = (near(v1.x, 0) or near(v1.x, -case_length) or 
                           near(v1.y, 0) or near(v1.y, case_width))
                v2_outer = (near(v2.x, 0) or near(v2.x, -case_length) or 
                           near(v2.y, 0) or near(v2.y, case_width))
                
                if v1_outer and v2_outer:
                    outer_edges.append(e)
                    App.Console.PrintMessage("  Outer edge: (%.2f,%.2f,%.2f) to (%.2f,%.2f,%.2f)\n" % 
                                           (v1.x, v1.y, v1.z, v2.x, v2.y, v2.z))
    
    App.Console.PrintMessage("Found %d outer edges to fillet with radius %.2f mm\n" % (len(outer_edges), face_edge_radius))
    if outer_edges:
        try:
            shell = shell.makeFillet(face_edge_radius, outer_edges)
            App.Console.PrintMessage("Outer fillet applied successfully\n")
        except Exception as ex:
            App.Console.PrintWarning("Outer fillet failed: %s\n" % ex)
    else:
        App.Console.PrintWarning("No outer edges found for fillet\n")
except Exception as ex:
    App.Console.PrintWarning("Outer edge selection failed: %s\n" % ex)

# ---------- Export ----------
# Create a Part feature to visualize in FreeCAD GUI if desired
part_obj = doc.addObject("Part::Feature", "FrontCaseShell")
part_obj.Shape = shell

# Determine output directory
try:
    script_dir = os.path.dirname(os.path.abspath(__file__))
except NameError:
    script_dir = os.getcwd()
out_dir = os.path.join(script_dir, "freecad_outputs")
if not os.path.exists(out_dir):
    os.makedirs(out_dir)

step_path = os.path.join(out_dir, "front_case_display_freecad.step")
stl_path = os.path.join(out_dir, "front_case_display_freecad.stl")

# Export STEP
Part.export([part_obj], step_path)

# Export STL with highest quality mesh for 3D printing
# Use same method as FreeCAD GUI export with premium settings
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

# Save doc (optional)
doc_file = os.path.join(out_dir, "front_case_display_freecad.FCStd")
doc.saveAs(doc_file)
App.Console.PrintMessage("Saved FCStd to: %s\n" % doc_file)

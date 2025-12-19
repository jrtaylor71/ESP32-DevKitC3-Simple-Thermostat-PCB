// ESP32-S3 Smart Thermostat - BACK CASE (Wall-mounting side)
// This case mounts to the wall and provides wiring/keyholes only (no PCB standoffs)
// Wire entry is through the back wall (the wall that touches the house wall)

// PCB dimensions (from Edge.Cuts: 21.5,28 to 154.5,117.5)
pcb_length = 133.0;  // 154.5 - 21.5
pcb_width = 89.5;    // 117.5 - 28.0
pcb_thickness = 1.6;

// PCB mounting holes - 3.2mm holes at PCB CORNERS
pcb_mount_holes = [
    [3.5, 3.5],       // Bottom left (25-21.5, 31.5-28)
    [3.5, 86.0],      // Top left (25-21.5, 114-28)
    [129.5, 3.5],     // Bottom right (151-21.5, 31.5-28)
    [129.5, 86.0]     // Top right (151-21.5, 114-28)
];

// Case parameters
wall_thickness = 2.5;
pcb_clearance = 8.0;          // Space around PCB edges (room for screw bosses)
standoff_height = 4.0;        // (Unused for back standoffs, kept for height calc)
standoff_diameter = 7.0;
hole_diameter = 3.2;          // 3.2mm mounting holes

// Component clearance on front (ESP32 side facing AWAY from wall)
component_height = 20.0;      // ESP32 stack is 15.2mm tall; give ~4.8mm margin

// Calculated dimensions
case_length = pcb_length + (2 * pcb_clearance);
case_width = pcb_width + (2 * pcb_clearance);
// Keep generous height for component clearance; adjust if needed
case_height = wall_thickness + standoff_height + pcb_thickness + component_height;

// Wire entry in BACK PANEL (pierce through panel thickness along Z)
wire_hole_diameter = 22.0;    // Large hole for wire bundle
wire_hole_x = case_length / 2; // Center horizontally
wire_hole_y = case_width / 2;  // Center vertically on panel
wire_hole_z = 10.0;           // Height from bottom

// Screw bosses to mate with front through-holes
screw_hole_dia = 3.0;        // clearance in front (match front)
screw_thread_dia = 2.5;      // pilot for self-tap M2.5 in bosses
screw_boss_dia = 8.0;
screw_boss_height = 10.0;
// Place screws outside PCB but inside case walls
screw_inset = wall_thickness + screw_boss_dia/2 + 0.5;
screw_positions = [
    [screw_inset, screw_inset],                           // bottom-left
    [case_length - screw_inset, screw_inset],             // bottom-right
    [screw_inset, case_width - screw_inset],              // top-left
    [case_length - screw_inset, case_width - screw_inset] // top-right
];

// Perimeter lip to seat front bezel (front rests ON the lip instead of fully nesting)
lip_depth = 1.5;
lip_height = 1.5;

// Wall mounting keyholes
keyhole_spacing = 83.0;
keyhole_width = 4.0;
keyhole_height = 15.0;
keyhole_head_dia = 10.0;

$fn = 64;

// === MODULES ===

module rounded_rect(l, w, h, r) {
    hull() {
        translate([r, r, 0]) cylinder(r=r, h=h);
        translate([l-r, r, 0]) cylinder(r=r, h=h);
        translate([r, w-r, 0]) cylinder(r=r, h=h);
        translate([l-r, w-r, 0]) cylinder(r=r, h=h);
    }
}

// Back case should NOT have PCB standoffs; PCB mounts to front cover

module keyhole() {
    // Head recess
    translate([0, keyhole_height/2, 0])
        cylinder(d=keyhole_head_dia, h=20, center=true);
    // Slot
    translate([0, 0, 0])
        cube([keyhole_width, keyhole_height, 20], center=true);
}

module vent_grid(rows, cols) {
    spacing = 7.0;
    for (r = [0:rows-1]) {
        for (c = [0:cols-1]) {
            translate([c*spacing, r*spacing, 0])
                cylinder(d=3.0, h=20, center=true);
        }
    }
}

// Short vent slots - cleaner look than long grid
module short_vent_slots(count, slot_length=12.0, slot_width=2.0, spacing=8.0) {
    for (i = [0:count-1]) {
        translate([i*spacing - (count-1)*spacing/2, 0, 0])
            cube([slot_length, slot_width, 20], center=true);
    }
}

// === BACK CASE (wall-mounting side) ===

module front_case() {
    difference() {
        // Outer shell
        rounded_rect(case_length, case_width, case_height, 4);
        
        // Hollow interior
        translate([wall_thickness, wall_thickness, wall_thickness])
            cube([
                case_length - 2*wall_thickness,
                case_width - 2*wall_thickness,
                case_height
            ]);
        
        // Wire entry hole through BACK PANEL (cuts along Z normal to panel)
        translate([wire_hole_x, wire_hole_y, -0.5])
            cylinder(d=wire_hole_diameter, h=wall_thickness + 1.0, center=false);
        
        // Keyhole mounting slots through BACK PANEL (cuts along Z)
        for (x_offset = [-keyhole_spacing/2, keyhole_spacing/2]) {
            // Slot rectangle through panel
            translate([case_length/2 + x_offset - keyhole_width/2, case_width/2 - keyhole_height/2, -0.5])
                cube([keyhole_width, keyhole_height, wall_thickness + 1.0]);
            // Head circle through panel
            translate([case_length/2 + x_offset, case_width/2, -0.5])
                cylinder(d=keyhole_head_dia, h=wall_thickness + 1.0, center=false);
        }
        
        // Ventilation on left and right sides - short slots instead of grid
        translate([wall_thickness/2, case_width/2, case_height*0.6])
            rotate([0, 90, 0])
                short_vent_slots(8, slot_length=10.0, slot_width=2.5, spacing=10.0);
        
        translate([case_length - wall_thickness/2, case_width/2, case_height*0.6])
            rotate([0, 90, 0])
                short_vent_slots(8, slot_length=10.0, slot_width=2.5, spacing=10.0);
    }
    
    // Perimeter seating lip near top to support front bezel
    translate([wall_thickness, wall_thickness, case_height - lip_height])
        difference() {
            cube([case_length - 2*wall_thickness, case_width - 2*wall_thickness, lip_height]);
            translate([lip_depth, lip_depth, -0.1])
                cube([case_length - 2*wall_thickness - 2*lip_depth, case_width - 2*wall_thickness - 2*lip_depth, lip_height + 0.2]);
        }

    // Screw bosses to receive front screws
    for (pos = screw_positions) {
        translate([pos[0], pos[1], wall_thickness])
            difference() {
                cylinder(d=screw_boss_dia, h=screw_boss_height);
                translate([0, 0, -0.1])
                    cylinder(d=screw_thread_dia, h=screw_boss_height + 0.2);
            }
    }

    // No PCB standoffs on back case; it is a flat wall-mount plate
}

front_case();

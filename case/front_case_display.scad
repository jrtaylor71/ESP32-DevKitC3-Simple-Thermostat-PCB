// ESP32-S3 Smart Thermostat - FRONT CASE (Display side, faces room)
// This case has the display opening and faces outward into the room
// Snaps onto the back case which holds the PCB

// Optional BOSL2 library for precise edge filleting (kept disabled by default)
// To enable, install BOSL2 to your OpenSCAD user libraries and set
// `enable_face_edge_round = true;`
// BOSL2 is included here to support the opt-in edge fillet.
include <BOSL2/std.scad>

// PCB dimensions
pcb_length = 133.0;
pcb_width = 89.5;
pcb_thickness = 1.6;

// PCB mounting holes - add posts on THIS case too for alignment
pcb_mount_holes = [
    [3.5, 3.5],       // Bottom left
    [3.5, 86.0],      // Top left  
    [129.5, 3.5],     // Bottom right
    [129.5, 86.0]     // Top right
];

// Case parameters
face_thickness = 1.8;   // Front face thickness (only the panel)
wall_thickness = 2.6;   // Side wall thickness (restored)
pcb_clearance = 8.0;
standoff_height = 17.3;  // Updated for display pin header clearance
standoff_diameter = 7.0;
hole_diameter = 2.7;  // tighter fit for M2.5

// Component clearance on display side
// Set so case wall sits 1.6mm ABOVE standoff tops (standoff_height + 1.6)
display_clearance = 18.9;  // 2.5 wall + 18.9 = 21.4mm overall height

// Display module mounting hole centers (relative to PCB origin at lower-left)
display_holes = [
    [45.5, 16.1],   // from PCB coords: (67, 44.1) minus (21.5, 28)
    [45.5, 65.0],   // (67, 93)
    [128.61, 16.1], // (150.11, 44.1)
    [128.61, 65.0]  // (150.11, 93)
];

display_center_x = (display_holes[0][0] + display_holes[3][0]) / 2; // avg of min/max
display_center_y = (display_holes[0][1] + display_holes[3][1]) / 2;

// Calculated dimensions
case_length = pcb_length + (2 * pcb_clearance);
case_width = pcb_width + (2 * pcb_clearance);
case_height = face_thickness + display_clearance;

// Display opening rotated 90° about Z (swap width/height) and aligned to display hole center
display_width = 68.0;   // swapped
display_height = 50.0;  // swapped
display_x = pcb_clearance + display_center_x - display_width/2;
display_y = pcb_clearance + display_center_y - display_height/2;

// Sensor holes (pass through front cover)
// LDR07 (R12) - needs hole for light to enter, ~5mm diameter
ldr_diameter = 5.5;           // Light entry hole for photoresistor
ldr_x = pcb_clearance + 104.8; // Relative X in case
ldr_y = pcb_clearance + 6.5;   // Relative Y in case

// Temperature/Humidity Sensor (J5) - protective box with airflow holes
// Supports AHT20, DHT11, BME280 - all use same footprint
sensor_box_width = 20.0;      // Width of box frame
sensor_box_height = 15.0;     // Height of box frame
sensor_box_depth = 16.0;      // Protrudes from case front
sensor_box_wall = 1.5;        // Wall thickness of sensor box
sensor_x = pcb_clearance + 11.5; // Relative X in case (J5 center)
sensor_y = pcb_clearance + 68.42; // Relative Y in case (J5 center)

// Screw fasteners (front-through holes, back bosses)
screw_hole_dia = 3.0;        // clearance for M2.5
screw_csk_dia = 7.0;         // countersink diameter for screw heads
screw_csk_depth = 3.0;       // countersink depth (taper angle ~82°)
screw_boss_dia = 8.0;        // match back boss size
// Place screws outside PCB but inside case walls
screw_inset = wall_thickness + screw_boss_dia/2 + 0.5;
screw_positions = [
    [screw_inset, screw_inset],                           // bottom-left
    [case_length - screw_inset, screw_inset],             // bottom-right
    [screw_inset, case_width - screw_inset],              // top-left
    [case_length - screw_inset, case_width - screw_inset] // top-right
];

$fn = 64;

// Toggle for BOSL2-based face/wall edge rounding (off by default)
enable_face_edge_round = false;      // Default OFF to preserve stable geometry
face_edge_radius = 1.0;              // Radius to apply at face/wall junction when enabled

// === MODULES ===

// Rounded rectangle for outer shell
module rounded_rect(l, w, h, r) {
    hull() {
        translate([r, r, 0]) cylinder(r=r, h=h);
        translate([l-r, r, 0]) cylinder(r=r, h=h);
        translate([r, w-r, 0]) cylinder(r=r, h=h);
        translate([l-r, w-r, 0]) cylinder(r=r, h=h);
    }
}

// Simple box with straight walls
module rounded_edge_box(l, w, h, corner_r, edge_r) {
    // No rounding at face/wall junction: plain rounded-rectangle hull
    hull() {
        translate([corner_r, corner_r, 0]) cylinder(r=corner_r, h=h);
        translate([l - corner_r, corner_r, 0]) cylinder(r=corner_r, h=h);
        translate([corner_r, w - corner_r, 0]) cylinder(r=corner_r, h=h);
        translate([l - corner_r, w - corner_r, 0]) cylinder(r=corner_r, h=h);
    }
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

// Short vent slots - cleaner look than long slots
module short_vent_slots(count, slot_length=12.0, slot_width=2.0, spacing=8.0) {
    for (i = [0:count-1]) {
        translate([i*spacing - (count-1)*spacing/2, 0, 0])
            cube([slot_length, slot_width, 20], center=true);
    }
}

// Protective sensor box with airflow holes
module sensor_protection_box() {
    // 4-wall rectangular frame rotated 90° around sensor center; no recess
    translate([sensor_x, sensor_y, 0])
        rotate([0,0,90])
            difference() {
                // Outer frame flush on front face
                translate([-sensor_box_width/2, -sensor_box_height/2, 0])
                    cube([sensor_box_width, sensor_box_height, sensor_box_depth]);

                // Inner cavity through full depth to leave only four side walls
                translate([-(sensor_box_width/2 - sensor_box_wall),
                           -(sensor_box_height/2 - sensor_box_wall),
                           -0.1])
                    cube([sensor_box_width - 2*sensor_box_wall,
                          sensor_box_height - 2*sensor_box_wall,
                          sensor_box_depth + 0.2]);

                // Vent holes through the outer walls themselves
                // Top wall vents (penetrate full wall thickness)
                for (i = [0:3]) {
                    translate([-(sensor_box_width/2 - 1), -(sensor_box_height/2), 2 + i*2.5])
                        cube([sensor_box_width - 2, 4.0, 1.5]);
                }

                // Bottom wall vents (penetrate full wall thickness)
                for (i = [0:3]) {
                    translate([-(sensor_box_width/2 - 1), (sensor_box_height/2 - 4.0), 2 + i*2.5])
                        cube([sensor_box_width - 2, 4.0, 1.5]);
                }

                // Left wall vents (penetrate full wall thickness)
                for (i = [0:2]) {
                    translate([-(sensor_box_width/2), -(sensor_box_height/2 - 1), 2 + i*3])
                        cube([4.0, sensor_box_height - 2, 1.5]);
                }

                // Right wall vents (penetrate full wall thickness)
                for (i = [0:2]) {
                    translate([(sensor_box_width/2 - 4.0), -(sensor_box_height/2 - 1), 2 + i*3])
                        cube([4.0, sensor_box_height - 2, 1.5]);
                }
            }
}

module pcb_standoff() {
    difference() {
        cylinder(d=standoff_diameter, h=standoff_height);
        translate([0, 0, -0.1])
            cylinder(d=hole_diameter, h=standoff_height + 0.2);
    }
}

// === FRONT CASE (display side) ===

module front_shell() {
    if (enable_face_edge_round) {
        // BOSL2-powered branch with precise edge-only fillet at face/wall junction
        // Use diff() to subtract both the fillet mask and all cutouts in one pass
        diff() {
            // Parent: BOSL2 rect_tube replicates our shell with straight walls
            // and rounded corners, preserving wall thickness. Anchor at bottom-left-back
            // to match origin [0,0,0].
            rect_tube(size=[case_length, case_width], height=case_height,
                      wall=wall_thickness, rounding=4, anchor=BOT+LEFT+BACK) {
                // Edge-only fillet: apply a consistent-height roundover along the bottom perimeter
                // Height is controlled by face_edge_radius
                edge_profile(BOT)
                    mask2d_roundover(height=face_edge_radius, mask_angle=$edge_angle, $fn=64);

                // CUT SENSOR HOLES THROUGH OUTER PANEL FIRST (z=0 face)
                // LDR07 hole - light entry through front face
                tag("remove") translate([ldr_x, ldr_y, -1])
                    cylinder(d=ldr_diameter, h=face_thickness + 2);

                // Temp/Humidity sensor front opening - match AHT20 footprint 12.5 x 6.0, rotated 90°
                tag("remove") translate([sensor_x, sensor_y, -0.1])
                    rotate([0,0,90])
                        translate([-12.5/2, -6.0/2, 0])
                            cube([12.5, 6.0, face_thickness + 0.2]);

                // Interior cavity is inherent in rect_tube; no separate subtraction needed


            // Recreate the front face plate of thickness face_thickness so the face is present.
            // Use original rounded rectangle hull to match outer shape exactly.
            tag("keep") rounded_rect(case_length, case_width, face_thickness, 4);
                // Display opening
                tag("remove") translate([display_x, display_y, -0.1])
                    cube([display_width, display_height, face_thickness + 0.2]);

                // Top edge ventilation - short slots instead of long grid
                tag("remove") translate([case_length/2, wall_thickness/2, case_height/2])
                    rotate([90, 0, 0])
                        short_vent_slots(15, slot_length=8.0, slot_width=2.0, spacing=8.5);

                // Bottom edge ventilation - short slots
                tag("remove") translate([case_length/2, case_width - wall_thickness/2, case_height/2])
                    rotate([90, 0, 0])
                        short_vent_slots(15, slot_length=8.0, slot_width=2.0, spacing=8.5);

                // Left edge ventilation (short end)
                tag("remove") translate([wall_thickness/2, case_width/2, case_height/2])
                    rotate([90, 0, 90])
                        short_vent_slots(10, slot_length=8.0, slot_width=2.0, spacing=8.5);

                // Right edge ventilation (short end)
                tag("remove") translate([case_length - wall_thickness/2, case_width/2, case_height/2])
                    rotate([90, 0, 90])
                        short_vent_slots(10, slot_length=8.0, slot_width=2.0, spacing=8.5);

                // Short-end ventilation TBD: will add precise slots aligned to the front panel thickness
            }
        }
    } else {
        // Original branch: plain rounded-rectangle hull, straight walls, no perimeter rounding
        difference() {
            // Outer shell with no rounding at face/wall junction
            rounded_edge_box(case_length, case_width, case_height, 4, 0);
            
            // CUT SENSOR HOLES THROUGH OUTER PANEL FIRST (z=0 face)
            // LDR07 hole - light entry through front face
            translate([ldr_x, ldr_y, -1])
                cylinder(d=ldr_diameter, h=face_thickness + 2);
            
            // Temp/Humidity sensor front opening - match AHT20 footprint 12.5 x 6.0, rotated 90°
            translate([sensor_x, sensor_y, -0.1])
                rotate([0,0,90])
                    translate([-12.5/2, -6.0/2, 0])
                        cube([12.5, 6.0, face_thickness + 0.2]);
            
            // THEN cut hollow interior (z >= face_thickness)
            translate([wall_thickness, wall_thickness, face_thickness])
                cube([
                    case_length - 2*wall_thickness,
                    case_width - 2*wall_thickness,
                    case_height
                ]);
            
            // Display opening
            translate([display_x, display_y, -0.1])
                cube([display_width, display_height, face_thickness + 0.2]);
            
            // Top edge ventilation - short slots instead of long grid
            translate([case_length/2, wall_thickness/2, case_height/2])
                rotate([90, 0, 0])
                    short_vent_slots(15, slot_length=8.0, slot_width=2.0, spacing=8.5);
            
            // Bottom edge ventilation - short slots
            translate([case_length/2, case_width - wall_thickness/2, case_height/2])
                rotate([90, 0, 0])
                    short_vent_slots(15, slot_length=8.0, slot_width=2.0, spacing=8.5);
            
            // Left edge ventilation (short end)
            translate([wall_thickness/2, case_width/2, case_height/2])
                rotate([90, 0, 90])
                    short_vent_slots(10, slot_length=8.0, slot_width=2.0, spacing=8.5);
            
            // Right edge ventilation (short end)
            translate([case_length - wall_thickness/2, case_width/2, case_height/2])
                rotate([90, 0, 90])
                    short_vent_slots(10, slot_length=8.0, slot_width=2.0, spacing=8.5);

            // Short-end ventilation TBD: will add precise slots aligned to the front panel thickness
        }
    }
}

module front_case() {
    difference() {
        union() {
            // Invert the shell so the current outward face becomes inward
            translate([0, 0, case_height])
                mirror([0, 0, 1])
                    front_shell();
            // PCB mounting standoffs (with holes) at all 4 corners
            for (hole = pcb_mount_holes) {
                translate([hole[0] + pcb_clearance, hole[1] + pcb_clearance, face_thickness])
                    pcb_standoff();
            }
            // Add sensor protection box on the outward front face using same mirror/translate
            translate([0, 0, case_height])
                mirror([0, 0, 1])
                    sensor_protection_box();
        }
        // Screw clearance holes through entire assembly
        for (pos = screw_positions) {
            // Through-hole
            translate([pos[0], pos[1], -0.5])
                cylinder(d=screw_hole_dia, h=case_height + 1.0);
        }
        
        // Tapered countersinks on exterior (top face, z=case_height)
        for (pos = screw_positions) {
            translate([pos[0], pos[1], case_height - screw_csk_depth])
                cylinder(d1=screw_hole_dia, d2=screw_csk_dia, h=screw_csk_depth + 1);
        }
    }
}

// Print orientation: flat front down
translate([0, 0, case_height])
    rotate([180, 0, 0])
        front_case();

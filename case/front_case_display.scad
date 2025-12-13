// ESP32-S3 Smart Thermostat - FRONT CASE (Display side, faces room)
// This case has the display opening and faces outward into the room
// Snaps onto the back case which holds the PCB

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
wall_thickness = 2.5;
pcb_clearance = 8.0;
standoff_height = 13.0;
standoff_diameter = 7.0;
hole_diameter = 2.7;  // tighter fit for M2.5

// Component clearance on display side
// Set so case wall sits 1.6mm ABOVE standoff tops (standoff_height + 1.6)
display_clearance = 14.6;  // 2.5 wall + 14.6 = 17.1mm overall height

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
case_height = wall_thickness + display_clearance;

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

// AHT20 (J5) - rectangular hole 12.5mm x 6mm, rotated 90°
aht20_width = 12.5;
aht20_height = 6.0;
aht20_x = pcb_clearance + 11.5; // Relative X in case
aht20_y = pcb_clearance + 66.42; // Relative Y in case

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

// === MODULES ===

module rounded_rect(l, w, h, r) {
    hull() {
        translate([r, r, 0]) cylinder(r=r, h=h);
        translate([l-r, r, 0]) cylinder(r=r, h=h);
        translate([r, w-r, 0]) cylinder(r=r, h=h);
        translate([l-r, w-r, 0]) cylinder(r=r, h=h);
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

module pcb_standoff() {
    difference() {
        cylinder(d=standoff_diameter, h=standoff_height);
        translate([0, 0, -0.1])
            cylinder(d=hole_diameter, h=standoff_height + 0.2);
    }
}

// === FRONT CASE (display side) ===

module front_shell() {
    difference() {
        // Outer shell
        rounded_rect(case_length, case_width, case_height, 4);
        
        // CUT SENSOR HOLES THROUGH OUTER PANEL FIRST (z=0 face)
        // LDR07 hole - light entry through front face
        translate([ldr_x, ldr_y, -1])
            cylinder(d=ldr_diameter, h=wall_thickness + 2);
        
        // AHT20 sensor hole - rectangular 12.5mm x 6mm through front face, rotated 90°
        translate([aht20_x, aht20_y, -1])
            rotate([0,0,90])
                translate([-aht20_width/2, -aht20_height/2, 0])
                    cube([aht20_width, aht20_height, wall_thickness + 2]);
        
        // THEN cut hollow interior (z >= wall_thickness)
        translate([wall_thickness, wall_thickness, wall_thickness])
            cube([
                case_length - 2*wall_thickness,
                case_width - 2*wall_thickness,
                case_height
            ]);
        
        // Display opening
        translate([display_x, display_y, -0.1])
            cube([display_width, display_height, wall_thickness + 0.2]);
        
        // Ventilation on top and bottom edges
        translate([case_length/2, wall_thickness/2, case_height/2])
            rotate([90, 0, 0])
                vent_grid(2, 12);
        
        translate([case_length/2, case_width - wall_thickness/2, case_height/2])
            rotate([90, 0, 0])
                vent_grid(2, 12);
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
                translate([hole[0] + pcb_clearance, hole[1] + pcb_clearance, wall_thickness])
                    pcb_standoff();
            }
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

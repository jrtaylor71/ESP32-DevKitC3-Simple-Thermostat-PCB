// Debug: show what the case dimensions and sensor positions are
pcb_length = 133.0;
pcb_width = 89.5;
pcb_clearance = 3.0;
wall_thickness = 2.5;

case_length = pcb_length + (2 * pcb_clearance);
case_width = pcb_width + (2 * pcb_clearance);

echo("Case dimensions: ", case_length, " x ", case_width);

// Sensor positions
ldr_x = pcb_clearance + 104.8;
ldr_y = pcb_clearance + 6.5;
aht20_x = pcb_clearance + 11.5;
aht20_y = pcb_clearance + 66.42;

echo("LDR position (X,Y): ", ldr_x, ",", ldr_y);
echo("AHT20 position (X,Y): ", aht20_x, ",", aht20_y);

// Check if within bounds
echo("LDR X in bounds [", wall_thickness, " - ", case_length - wall_thickness, "]? ", (ldr_x >= wall_thickness && ldr_x <= case_length - wall_thickness));
echo("LDR Y in bounds [", wall_thickness, " - ", case_width - wall_thickness, "]? ", (ldr_y >= wall_thickness && ldr_y <= case_width - wall_thickness));
echo("AHT20 X in bounds [", wall_thickness, " - ", case_length - wall_thickness, "]? ", (aht20_x >= wall_thickness && aht20_x <= case_length - wall_thickness));
echo("AHT20 Y in bounds [", wall_thickness, " - ", case_width - wall_thickness, "]? ", (aht20_y >= wall_thickness && aht20_y <= case_width - wall_thickness));

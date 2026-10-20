// buttons.scad - flex insert for the two front buttons, built step by step (same coordinates as build.scad)
// & "C:\Program Files\OpenSCAD\openscad.exe" -o ..\lilygo_buttons_test5.stl buttons.scad
$fn = 48;

// STEP 1: settings, copied from build.scad so the insert lines up with the shell
fit = 0.20;  rw = 2.0;  plug_over = 0.54;
pcb_w = 25.51;  blk_w = 25.6;  in_w = blk_w + 2*fit;   // inside width of the shell (26.0)
blk_y0 = (pcb_w - blk_w) / 2;
plate = 1.2;  blk_h = 6.44;  pcb_top = plate + blk_h + 0.1;   // z of the board's top surface
bx = rw + plug_over + fit;                       // shell x of the board's USB-C edge
by = rw + fit - blk_y0;                          // shell y of the board's edge
blk_x0 = 6.00;
step_x1 = blk_x0 - fit;  step_extra = 1.1;  step_wall = 1.0;  wall_cham = 0.8;
step_under = 3.5;  step_plate = 1.6;
wall_top = pcb_top - step_under;                 // top of the roof (4.24)
roof_bot = wall_top - step_plate;                // underside of the roof (2.64), the recess is open below this
recess_x1 = bx + step_x1 + step_extra - step_wall;   // where the recess ends at the drop-down wall's face
btn_x0 = 1.65;  btn_x1 = 5.82;                   // button x range on the board
btn_y0 = 3.04;  btn_y1 = 6.40;                   // button y range, from the board edge

// the insert's own settings
ins_gap = 0.15;                                  // free space between the insert and the recess walls, each side
top_gap = 0.10;                                  // free space between the insert's top and the roof underside
btn_top = 5.7;                                   // MEASURED: button top, up from the outer surface (bed) of the recess
btn_h   = pcb_top - btn_top;                    // so the button stands this far below the board's top surface (2.04)
btn_gap = 0.1;                                   // air gap between a stem's tip and its button (so nothing is pressed at rest)
stem_clr = 0.2;                                  // each stem is this much smaller than the button, each side

// STEP 2: the plate. It fills the recess from the bed (z = 0) up to just under the roof, clear of the walls by ins_gap.
ins_x0 = rw + ins_gap;
ins_x1 = recess_x1 - ins_gap;
ins_y0 = rw + ins_gap;
ins_y1 = rw + in_w - ins_gap;
ins_t  = 1.6;                                    // plate thickness (thinner so it flexes; outer face stays flush at z = 0)
echo("insert size x*y*z:", ins_x1 - ins_x0, ins_y1 - ins_y0, ins_t);

// STEP 3: bevel on the long edge next to the drop-down wall (matches wall_cham on the wall).
// The plate is a side profile (x and z) pushed along y: bevelled corner at the bed, on the wall side.
module plate() {
  L = ins_x1 - ins_x0;
  translate([ins_x0, ins_y1, 0])
    rotate([90, 0, 0])
      linear_extrude(height = ins_y1 - ins_y0)
        polygon([[0, 0], [L - wall_cham, 0], [L, wall_cham], [L, ins_t], [0, ins_t]]);
}
plate();


// rounded rectangle, same as in build.scad: a hull of four cylinders in the corners
stem_r = 1.4;                                   // corner radius of the stems (the finger holes use 1.0, the stems sit 0.25 inside them)
module rrect(l, w, h, r) {
  hull()
    for (x = [r, l - r], y = [r, w - r])
      translate([x, y, 0]) cylinder(r = r, h = h);
}

// STEP 4: a stem over each button. It rises from the plate through the finger hole and stops btn_gap short of the button.
stem_top = pcb_top - btn_h - btn_gap;            // z of the stem tip
for (m = [0, 1]) {
  yb = (m == 0) ? btn_y0 : pcb_w - btn_y1;
  sx0 = bx + btn_x0 + stem_clr;
  sx1 = min(bx + btn_x1 - stem_clr, ins_x1 - 0.03);    // stop at the plate's edge
  sy0 = by + yb + stem_clr;
  sy1 = by + yb + (btn_y1 - btn_y0) - stem_clr;
  translate([sx0, sy0, ins_t - 0.01])
    rrect(sx1 - sx0, sy1 - sy0, stem_top - ins_t + 0.01, stem_r);
}
echo("stem tip z:", stem_top, " stem length above plate:", stem_top - ins_t);
// EXPORT TO STL (run in PowerShell from this folder; same as F6 then F7 in the app):
// & "C:\Program Files\OpenSCAD\openscad.exe" -o ..\lilygo_front_shell_test2.stl build.scad
// build.scad - LilyGO front shell, built step by step (plate-down)
$fn = 48;

// STEP 1: settings (all sizes in mm)
pcb_l = 51.74;  pcb_w = 25.51;  pcb_t = 1.16;   // the circuit board
fit   = 0.20;                                    // gap around the board
rw    = 2.0;                                     // wall thickness
bat_extra = 5.0;   // how far the 802030 battery sticks out above the old 13.9 mm shell (measured)
bat_clr   = 0.0;   // extra clearance above the battery (the 5 mm already includes it)
depth = 13.9 + bat_extra + bat_clr;          // total height of the shell (18.9)
plate = 1.2;                                     // STEP 2: thickness of the top plate

// STEP 3: more measurements, all taken from the board's USB-C edge (x) and long edge (y)
plug_over = 0.54;                                // white plugs stick out past the USB-C end
blk_x0 = 6.00;  blk_w = 25.6;                    // clear acrylic block: starts at x = 6, 25.6 wide
blk_y0 = (pcb_w - blk_w) / 2;                    // block is centred on the board
pan_l = 37.18;  pan_w = 20.39;                   // black screen panel = the window size
pan_dx = 5.06;  pan_dtop = 2.72;                 // panel inset from block's left and top edges
pan_x0 = blk_x0 + pan_dx;
pan_y0 = blk_y0 + blk_w - pan_dtop - pan_w;

// STEP 4: USB-C port, and the height of the PCB's top surface
usbc_w = 9.06;  usbc_h = 3.19;                   // USB-C port size
usb_clr = 0.20;                                  // clearance around the port
blk_h = 6.44;                                    // PCB top -> top of the clear block
pcb_top = plate + blk_h + 0.1;                   // z of the PCB's top surface (plate-down: measured up from the bed)

// STEP 5: side buttons (power / reset), one on each long wall
side_x = 10.2;                                  // distance from the USB-C edge to the slot's centre
slot_len = 5.5;                                 // slot length along the wall
slot_top = 3.2;                                  // slot reaches this far above the PCB top

// STEP 6: where the low button end stops (just before the clear block)
step_x1 = blk_x0 - fit;                       // board x where the clear block starts (the cut itself ends step_extra further on)

// STEP 7: the drop-down wall
step_extra = 1.1;  // widen the step 6 cut by this much
step_wall  = 1.0;  // NOTE: thinned to 1.0 so the button finger holes clear; far face stays at the end of the cut                              // thickness of the cross wall
wall_cham = 0.8;                   // size of the bevel on the wall's outside edge (wall is 1.0 thick, so keep it under that)
step_under = 3.5;                              // underside of the future step roof, below the PCB top
step_plate = 1.6;                              // thickness of the future step roof
wall_top   = pcb_top - step_under;              // top of the cross wall (plate-down height)

// inside size (now includes the plugs and the wider block)
in_l = pcb_l + plug_over + 2*fit;
in_w = blk_w + 2*fit;

// where the board's own corner (x = 0, y = 0) sits inside the shell
bx = rw + plug_over + fit;
by = rw + fit - blk_y0;

corner_r = 2.0;   // outside corner radius

difference() {
  // the outer block = inside size + a wall on every side
  rrect(in_l + 2*rw, in_w + 2*rw, depth, corner_r);

  // STEP 2: the cavity, sitting on top of the plate, open at the top
  translate([rw, rw, plate])
    cube([in_l, in_w, depth]);

  // STEP 3: the screen window, cut straight through the plate
  translate([bx + pan_x0, by + pan_y0, -1])
    cube([pan_l, pan_w, plate + 2]);

  // STEP 4: USB-C opening through the end wall (x = 0 side)
  // centred on the board's width; from just below the PCB up past the port top
  // STEP 10: rounded corners. hull() wraps a skin around four cylinders lying along x, one per corner
  usb_r = 1.0;                       // corner radius
  usb_w = usbc_w + 2*usb_clr;        // opening width
  usb_h = usbc_h + 0.3 + 0.15;       // opening height
  translate([-1, by + pcb_w/2 - usb_w/2, pcb_top - usbc_h - 0.3])
    hull()
      for (y = [usb_r, usb_w - usb_r], z = [usb_r, usb_h - usb_r])
        translate([0, y, z]) rotate([0, 90, 0]) cylinder(r = usb_r, h = rw + 2);

  // STEP 5: side-button slots, both long walls (the for loop runs the cut twice)
  slot_r = 1.0;   // radius of the rounded bottom end
  for (yy = [-1, rw + in_w - 1])
    translate([bx + side_x - slot_len/2, yy, pcb_top - slot_top])
      hull() {                                  // rounded bottom end: two cylinders + the straight upper part
        for (x = [slot_r, slot_len - slot_r])
          translate([x, 0, slot_r]) rotate([-90, 0, 0]) cylinder(r = slot_r, h = rw + 2);
        translate([0, 0, slot_r]) cube([slot_len, rw + 2, depth]);
      }

  // STEP 6: cut the button-end square out of the plate, inside the walls
  translate([rw, rw, -1])
    cube([bx + step_x1 + step_extra - rw, in_w, plate + 2]);   // +2 so the top pokes above the plate (no shared surface)
}

// STEP 7: the drop-down wall, ADDED after the cuts so nothing removes it
difference() {
  translate([bx + step_x1 + step_extra - step_wall, rw, 0])
    cube([step_wall, in_w, wall_top]);

  // red-line chamfer: a square bar turned 45 degrees, centred on the wall's outside edge (button side, screen-side face).
  // the bar is wall_cham*sqrt(2) wide, so it slices off a wall_cham (0.8 mm) 45 degree corner. 1 mm extra each end in y so it cuts clean through.
  translate([bx + step_x1 + step_extra - step_wall, rw - 1, 0])
    rotate([0, 45, 0])
      translate([-wall_cham*sqrt(2)/2, 0, -wall_cham*sqrt(2)/2])
        cube([wall_cham*sqrt(2), in_w + 2, wall_cham*sqrt(2)]);
}

// STEP 8: the step roof, a thin slab on top of the wall, bridging the opened button end
// (its top is level with the wall top; step_plate is how thick it is)
// STEP 9: finger holes through the roof, one over each front button (mirrored across the width)
btn_x0 = 1.65;  btn_x1 = 5.82;    // button x range on the board
btn_y0 = 3.04;  btn_y1 = 6.40;    // button y range, measured from the board edge
mark   = 0.30;                    // extra room around each button for a finger
hole_x1 = step_x1 + step_extra - step_wall;   // stop the holes at the wall's face

cham    = 0.5;                    // chamfer size on the hole rims
hole_r  = 1.0;                    // corner radius of the finger holes

// a box with rounded vertical edges: hull() wraps a skin around four cylinders, one per corner
module rrect(l, w, h, r) {
  hull()
    for (x = [r, l - r], y = [r, w - r])
      translate([x, y, 0]) cylinder(r = r, h = h);
}

difference() {
  translate([rw, rw, wall_top - step_plate])
    cube([bx + step_x1 + step_extra - rw, in_w, step_plate]);
  for (m = [0, 1]) {
    yb = (m == 0) ? btn_y0 : pcb_w - btn_y1;
    translate([bx + btn_x0 - mark, by + yb - mark, wall_top - step_plate - 1])
      rrect(hole_x1 - btn_x0 + mark, btn_y1 - btn_y0 + 2*mark, step_plate + 2, hole_r);

    // chamfer: a 45 degree bevel around the top rim of each hole (hull of the hole outline at the roof's top and a bigger one 1 mm above)
    hull() {
      translate([bx + btn_x0 - mark, by + yb - mark, wall_top - cham])
        rrect(hole_x1 - btn_x0 + mark, btn_y1 - btn_y0 + 2*mark, 0.01, hole_r);
      translate([bx + btn_x0 - mark - cham - 1, by + yb - mark - cham - 1, wall_top + 1])
        rrect(hole_x1 - btn_x0 + mark + 2*(cham+1), btn_y1 - btn_y0 + 2*mark + 2*(cham+1), 0.01, hole_r + cham + 1);
    }

    // same bevel on the other face of the roof (the mirror image, mirrored about the roof's underside)
    hull() {
      translate([bx + btn_x0 - mark, by + yb - mark, wall_top - step_plate + cham - 0.01])
        rrect(hole_x1 - btn_x0 + mark, btn_y1 - btn_y0 + 2*mark, 0.01, hole_r);
      translate([bx + btn_x0 - mark - cham - 1, by + yb - mark - cham - 1, wall_top - step_plate - 1])
        rrect(hole_x1 - btn_x0 + mark + 2*(cham+1), btn_y1 - btn_y0 + 2*mark + 2*(cham+1), 0.01, hole_r + cham + 1);
    }
  }
}

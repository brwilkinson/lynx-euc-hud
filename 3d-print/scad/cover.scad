// EXPORT TO STL (run in PowerShell from this folder; same as F6 then F7 in the app):
// & "C:\Program Files\OpenSCAD\openscad.exe" -o ..\lilygo_cover_test9.stl cover.scad
// cover.scad - LilyGO top cover, built step by step (prototype in PLA, final in TPU)
$fn = 48;

// STEP 1: settings - copied from build.scad so the cover matches the shell outline
pcb_l = 51.74;  pcb_w = 25.51;                   // the circuit board
fit   = 0.20;                                    // gap around the board
rw    = 2.0;                                     // shell wall thickness
plug_over = 0.54;                                // white plugs stick out past the USB-C end
blk_w = 25.6;                                    // clear block width
in_l = pcb_l + plug_over + 2*fit;                // inside length of the shell (52.68)
in_w = blk_w + 2*fit;                            // inside width of the shell (26.0)
corner_r = 2.0;                                  // outside corner radius, same as the shell

lid_t = 1.6;                                     // thickness of the flat cover plate

// a box with rounded vertical edges (same module as in build.scad)
module rrect(l, w, h, r) {
  hull()
    for (x = [r, l - r], y = [r, w - r])
      translate([x, y, 0]) cylinder(r = r, h = h);
}

// STEP 2: the flat cover plate, same outline as the shell (56.68 x 30.0)
rrect(in_l + 2*rw, in_w + 2*rw, lid_t, corner_r);
// STEP 3: the lip - a hollow ring that drops into the shell's opening and keeps the cover centred
lip_gap  = 0.03;   // clearance on each side (0.20 loose, 0.08 still a bit loose, 0.03 = tight press fit)
lip_wall = 2.0;    // thickness of the ring (was 1.2)
lip_h    = 3.0;    // how far it sticks down into the shell

lip_l = in_l - 2*lip_gap;
lip_w = in_w - 2*lip_gap;

translate([rw + lip_gap, rw + lip_gap, lid_t])        // sits on the plate, inside the shell's cavity outline
  difference() {
    cube([lip_l, lip_w, lip_h]);                      // solid block...
    translate([lip_wall, lip_wall, -1])               // ...hollowed out, leaving a wall of lip_wall
      cube([lip_l - 2*lip_wall, lip_w - 2*lip_wall, lip_h + 2]);
  }
// STEP 4: button posts - one on each long wall, running down the side slot toward the power/reset buttons
// (shell numbers copied from build.scad; change them there and here together)
plate = 1.2;  blk_h = 6.44;
pcb_top   = plate + blk_h + 0.1;                 // z of the board's top surface in the shell
side_x    = 10.2;  slot_len = 5.5;  slot_top = 3.2;
bat_extra = 5.0;                                 // matches build.scad (taller body). Use 0 to fit the OLD 13.9 mm body.
bat_clr   = 0.0;
depth     = 13.9 + bat_extra + bat_clr;          // shell height
bx        = rw + plug_over + fit;                // shell x of the board's USB-C edge

post_tip_z = pcb_top - slot_top + 0.4;           // slot bottom is at pcb_top - slot_top (4.54); stop 0.4 above it so the tab reaches the full slot
post_len   = depth - post_tip_z;                 // lid underside sits at z = depth, so this is how far the post reaches down
post_w     = slot_len - 0.15;                    // 0.075 clearance each side of the slot
post_t     = rw - 0.4;                           // thickness in the wall plane, 0.2 clearance each side
shell_w    = in_w + 2*rw;                        // outer width of the shell

ext = lip_gap + lip_wall;                        // extra thickness so each post joins the ring wall (only along the ring, see below)

// STEP 5b: pocket on the inside face of each tab, so the flex only touches the button when pressed
pocket_d = 0.6;                                  // how deep the pocket goes into the tab (leaves post_t - pocket_d of skin)
pocket_h = 3.4;                                  // pocket height along the tab
pocket_w = post_w - 2.0;                         // pocket width, leaving 1 mm rails each side
btn_from_tip = 2.0;                              // button centre is 2 mm from the tab tip
tip_z = lid_t - 0.01 + post_len;                 // z of the tab tip in this plate-down lid

thick_len = post_len - (btn_from_tip + pocket_h/2) - 0.3;   // thick part ends 0.3 mm above the pocket top (measured from the lid plate)
module tab(far) {
  x0 = bx + side_x - post_w/2;
  y0 = far ? shell_w - 0.2 - post_t : 0.2;       // outer face at the shell edge, slot-thin all the way down
  jy = far ? shell_w - 0.2 - post_t - ext : 0.2; // wider start where it joins the ring
  difference() {
    union() {
      translate([x0, y0, lid_t - 0.01]) cube([post_w, post_t, post_len]);               // slim tab, fits inside the slot only
      translate([x0, jy, lid_t - 0.01]) cube([post_w, post_t + ext, thick_len]);   // thick joint, down to just above the pocket
    }
    // pocket on the inner face (toward the board), centred btn_from_tip below the tip
    py = far ? y0 - 0.01 : y0 + post_t - pocket_d;
    translate([bx + side_x - pocket_w/2, py, tip_z - btn_from_tip - pocket_h/2])
      cube([pocket_w, pocket_d + 0.01, pocket_h]);
  }
}
tab(false);   // near wall
tab(true);    // far wall (mirror)
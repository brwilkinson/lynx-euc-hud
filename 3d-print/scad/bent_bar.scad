// Bent GoPro 2-prong to 2-prong bar for the helmet HUD
// Measured from gopro_2-prong_to_2-prong_straight_adapter_bar_60mm_gap.stl
$fn = 64;

bend  = 15;   // degrees of bend (photo shows ~15 deg rise to cancel)
L1    = 25;   // visor-end hole to the bend
L2    = 62;   // bend to HUD-end hole (L1+L2 = 87, 1 cm longer than the 60 mm bar)
R     = 7.5;  // round end radius, half of the 15 mm height
W     = 13;   // body width
pw    = 2.8;  // prong thickness
gap   = 3.4;  // gap between the two prongs
hole  = 5.5;  // thumbscrew hole
neck  = 9;    // hole centre to where the solid body starts

module tab() {               // two prongs, hole at the origin, pointing -X
  difference() {
    for (s = [-1, 1])
      translate([0, s * (gap + pw) / 2, 0]) rotate([90, 0, 0])
        linear_extrude(pw, center = true)
          hull() { circle(R); translate([neck + 1, -R]) square([0.01, 2 * R]); }
    rotate([90, 0, 0]) cylinder(d = hole, h = 20, center = true);
  }
}
module body(a, b) { translate([a, -W / 2, -R]) cube([b - a, W, 2 * R]); }
module joint()    { rotate([90, 0, 0]) cylinder(r = R, h = W, center = true); }

tab();
body(neck, L1);
translate([L1, 0, 0]) {
  joint();
  rotate([0, -bend, 0]) {
    body(0, L2 - neck);
    translate([L2, 0, 0]) rotate([0, 0, 180]) tab();
  }
}

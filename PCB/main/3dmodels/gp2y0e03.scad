$fn=64;
translate([-5.5,-8.5]) color("green") cube([11,17,1.2]);

color("gray") difference() {
  translate([-5,-8.4]) cube([10,4,3]);
  translate([-4,-9.3]) cube([8,5,2.4]);
}

color("gray") difference() {
  translate([-5,-1]) cube([10,5.8,5]);
  translate([2.5,1.9,-1]) cylinder(d=3.8,h=10);
  translate([-2.5,1.9,-1]) cylinder(d=3.8,h=10);
}
color("brown") {
  translate([2.5,1.9,3.2]) sphere(d=3.4);
  translate([-2.5,1.9,3.2]) sphere(d=3.4);
}
  
for (i = [-4:1:4]) {
  translate([i,6.5,1]) cube([0.5,1,0.6]);
}

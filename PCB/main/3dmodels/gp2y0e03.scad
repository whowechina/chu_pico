$fn=64;
translate([-5.5,-8.5]) color("darkgreen") cube([11,17,1.2]);

color("#202020") difference() {
  translate([-5,-8.4]) cube([10,4,3]);
  translate([-4,-9.3]) cube([8,4,2.4]);
}

color("#303040") difference() {
  translate([-5,-1]) cube([10,5.8,5]);
  translate([2.5,1.9,3]) cylinder(d=3.8,h=10);
  translate([-2.5,1.9,3]) cylinder(d=3.8,h=10);
}
color("#602020") {
  translate([2.5,1.9,3.2]) sphere(d=3.4);
  translate([-2.5,1.9,3.2]) sphere(d=3.4);
}
  
for (i = [-4:1:4]) {
  color("yellow") translate([i,6.5,1]) cube([0.5,1,0.6]);
}

for (i = [-4:1:4]) {
  color("silver") translate([i-0.2,-1.8,0.8]) cube([0.5,1,0.6]);
  color("silver") translate([i-0.2,4.6,0.8]) cube([0.5,1,0.6]);
}

for (i = [-4:1.3:4]) {
  color("silver") translate([i-0.3,-5,1]) cube([0.6,2,0.6]);
  color("silver") translate([i-0.6,-7,2]) cube([0.2,2,0.6]);
}

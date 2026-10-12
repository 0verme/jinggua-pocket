// Issue #15 — first-pass open-access StickS3 bumper envelope.
// DRAFT ONLY: clearances and print process are engineering assumptions, not fit validated.
// The source model is M5Stack's MIT-licensed K150 StickS3 STL in ./reference/.

part = "preview"; // [preview, shell]
show_reference = true;
$fn = 48;

// Manufacturer nominal body envelope (M5Stack product specification, mm).
device_width = 24.0;
device_length = 48.0;
device_depth = 15.0;
device_corner_radius = 3.0; // M5Stack model-size drawing calls out R3.

// Prototype-only assumptions; tune only after physical fit-coupon testing.
fit_clearance = 0.35; // per side; not measured on a physical StickS3
wall_thickness = 1.20; // nominal printed wall; not structurally qualified
interface_clearance = 0.35;
side_access_margin = 2.0; // deliberate clearance for modeled side controls/connectors
end_access_width = 18.7; // derived from the model's 18 mm end connector envelope
end_access_height = 9.0; // generous access envelope; verify on hardware
rear_access_width = 20.7; // leaves a perimeter rail; exposes rear audio/fasteners
rear_access_length = 40.7;

outer_size = [
    device_width + 2 * (fit_clearance + wall_thickness),
    device_length + 2 * (fit_clearance + wall_thickness),
    device_depth + 2 * (fit_clearance + wall_thickness)
];
inner_size = [
    device_width + 2 * fit_clearance,
    device_length + 2 * fit_clearance,
    device_depth + 2 * fit_clearance
];
body_offset = [wall_thickness + fit_clearance,
               wall_thickness + fit_clearance,
               wall_thickness + fit_clearance];
outer_radius = device_corner_radius + wall_thickness + fit_clearance;
inner_radius = device_corner_radius + fit_clearance;

// Opening dimensions measured from the pinned official STL's front-face mesh.
// Values are nominal model apertures; the shell adds interface_clearance.
screen_nominal = [16.030, 26.0911];
screen_center = [12.000, 30.97015];
screen_corner_radius = 1.00; // rounded-corner fit to the source mesh
button_nominal = [11.940, 2.340];
button_center = [12.000, 10.5607];
button_corner_radius = 1.17;

// Rear screw centers are reference-derived. Their explicit access cylinders
// are currently redundant inside the broad rear window; retain them if that
// opening is narrowed. Hole diameter is provisional, not a screw specification.
rear_screw_centers = [[3.000, 8.560], [21.000, 8.560]];
rear_screw_access_diameter = 3.20;

module rounded_rectangle_2d(width, length, radius) {
    hull() {
        for (x = [radius, width - radius]) {
            for (y = [radius, length - radius]) {
                translate([x, y]) circle(r = radius);
            }
        }
    }
}

module rounded_prism(size, radius) {
    hull() {
        for (x = [radius, size[0] - radius]) {
            for (y = [radius, size[1] - radius]) {
                translate([x, y, 0]) cylinder(h = size[2], r = radius);
            }
        }
    }
}

module rounded_through_cut(center_xy, size_xy, radius, z, depth) {
    translate([center_xy[0] - size_xy[0] / 2,
               center_xy[1] - size_xy[1] / 2,
               z])
        linear_extrude(height = depth)
            rounded_rectangle_2d(size_xy[0], size_xy[1], radius);
}

module tapered_front_cut(center_xy, nominal_size, corner_radius, flare) {
    inner_size_xy = [nominal_size[0] + 2 * interface_clearance,
                     nominal_size[1] + 2 * interface_clearance];
    outer_size_xy = [inner_size_xy[0] + 2 * flare,
                     inner_size_xy[1] + 2 * flare];
    inner_radius = corner_radius + interface_clearance;
    outer_radius = inner_radius + flare;
    cut_z = outer_size[2] - wall_thickness - 0.05;
    cut_depth = wall_thickness + 0.10;

    // A shallow flare reduces viewing-angle obstruction from the protective lip.
    hull() {
        translate([center_xy[0] - inner_size_xy[0] / 2,
                   center_xy[1] - inner_size_xy[1] / 2,
                   cut_z])
            linear_extrude(height = 0.02)
                rounded_rectangle_2d(inner_size_xy[0], inner_size_xy[1], inner_radius);
        translate([center_xy[0] - outer_size_xy[0] / 2,
                   center_xy[1] - outer_size_xy[1] / 2,
                   cut_z + cut_depth - 0.02])
            linear_extrude(height = 0.02)
                rounded_rectangle_2d(outer_size_xy[0], outer_size_xy[1], outer_radius);
    }
}

module rear_access_cutout() {
    rear_center = [body_offset[0] + device_width / 2,
                   body_offset[1] + device_length / 2];
    rear_z = -0.05;
    rear_depth = wall_thickness + 0.10;

    // Broad rear access avoids covering the modeled speaker-area feature,
    // microphone locations not fully dimensioned in public material, and screws.
    rounded_through_cut(rear_center,
                        [rear_access_width, rear_access_length],
                        2.0,
                        rear_z,
                        rear_depth);

    for (center = rear_screw_centers) {
        translate([body_offset[0] + center[0],
                   body_offset[1] + center[1],
                   rear_z])
            cylinder(h = rear_depth, d = rear_screw_access_diameter);
    }
}

module side_and_end_access_cutouts() {
    side_y = body_offset[1] + side_access_margin;
    side_length = device_length - 2 * side_access_margin;
    side_z = body_offset[2] + 3.0;
    side_height = device_depth - 6.0;
    cut_extra = 0.10;

    // Long side windows intentionally leave controls, USB-C, and other side
    // features exposed instead of relying on unverified individual port sizes.
    translate([-cut_extra, side_y, side_z])
        cube([wall_thickness + 2 * cut_extra, side_length, side_height]);
    translate([outer_size[0] - wall_thickness - cut_extra, side_y, side_z])
        cube([wall_thickness + 2 * cut_extra, side_length, side_height]);

    // Both short ends retain a generous open access window. This also avoids
    // guessing the microphone/connector interface from an unmeasured unit.
    end_x = body_offset[0] + (device_width - end_access_width) / 2;
    end_z = body_offset[2] + (device_depth - end_access_height) / 2;
    translate([end_x, -cut_extra, end_z])
        cube([end_access_width, wall_thickness + 2 * cut_extra, end_access_height]);
    translate([end_x, outer_size[1] - wall_thickness - cut_extra, end_z])
        cube([end_access_width, wall_thickness + 2 * cut_extra, end_access_height]);
}

module bumper_shell() {
    assert(fit_clearance >= 0, "fit_clearance must be non-negative");
    assert(wall_thickness >= 0.8, "prototype wall must be at least 0.8 mm");
    assert(inner_radius < min(inner_size[0], inner_size[1]) / 2,
           "inner corner radius does not fit the cavity");

    difference() {
        rounded_prism(outer_size, outer_radius);
        translate([wall_thickness, wall_thickness, wall_thickness])
            rounded_prism(inner_size, inner_radius);

        tapered_front_cut(
            [body_offset[0] + screen_center[0], body_offset[1] + screen_center[1]],
            screen_nominal,
            screen_corner_radius,
            1.00);
        tapered_front_cut(
            [body_offset[0] + button_center[0], body_offset[1] + button_center[1]],
            button_nominal,
            button_corner_radius,
            0.40);
        rear_access_cutout();
        side_and_end_access_cutouts();
    }
}

module sticks3_reference() {
    // The vendor STL is an assembly export with repeated/exploded components.
    // Clip to the first 24 x 48 mm device group; this is preview geometry only.
    translate([body_offset[0],
               body_offset[1] - 0.0587905,
               body_offset[2] + 14.077364])
        intersection() {
            import("reference/StickS3.stl", convexity = 10);
            translate([-0.01, 0.04, -14.09])
                cube([24.03, 48.04, 15.04]);
        }
}

if (part == "shell") {
    bumper_shell();
} else {
    color([0.12, 0.32, 0.56, 0.48]) bumper_shell();
    if (show_reference) color([0.92, 0.56, 0.18, 0.85]) sticks3_reference();
}

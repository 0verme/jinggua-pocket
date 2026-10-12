# Issue #15 — hardware validation plan

**Current result: NOT RUN / `PENDING_HARDWARE_VALIDATION`.** No target StickS3, physical prototype, caliper, audio measurement fixture, or thermal instrument was available while preparing this software package. CAD source measurements and successful software tests do not satisfy the Issue #15 physical acceptance criteria.

## Entry criteria

1. Obtain the exact StickS3 revision intended for v1.0 and record its model/serial and firmware commit.
2. Compare the device with the pinned M5Stack reference drawing/model; measure body, screen, button, USB-C, side controls, speaker, microphone, and connector clearances. Record discrepancies before changing SCAD parameters.
3. Render the SCAD model in OpenSCAD and inspect the exported mesh for manifoldness, wall thickness, openings, and print orientation. OpenSCAD rendering/export was not available in the current runner.
4. Print fit coupons at 0.20, 0.35, and 0.50 mm clearance before printing a full prototype. Record printer, material lot, nozzle, layer height, orientation, and actual coupon dimensions.

## Test matrix

| Area | Method | Evidence to record | Pass condition / current state |
| --- | --- | --- | --- |
| Dimensions and fit | Caliper-check the target unit; fit/remove coupons and full prototype without forcing the board or compressing the battery | Measurements, photos, chosen clearance, installation/removal observations | No interference, deformation, scratching, or unintended pressure; **PENDING** |
| Screen | Exercise every screen and inspect the full panel at normal viewing angles and brightness | Photos/video and any obscured pixels or viewing-angle loss | All active display area remains legible and unobstructed; **PENDING** |
| Buttons | Compare un-cased and cased press feel; repeat normal and deliberate presses and inspect event logs | Button-by-button cycle count, missed/double/false events, tactile observations | No added false/missed events; acceptable feel and no pocket-induced activation; exact force threshold is not yet specified; **PENDING** |
| USB-C and other connectors | Insert/remove production cables and verify access to the documented end connector | Cable types, insertion photos, charging/data results | Full insertion and normal retention without case interference; **PENDING** |
| Speaker and microphone | Compare the existing local audio paths with and without the case using the same device, firmware, placement, and input/output level; do not upload audio | Local test configuration, recordings only if separately authorized, subjective/quantitative results | No blocked opening or unacceptable change; microphone position and quantitative limit must first be confirmed; **PENDING** |
| IMU shake experience | Run the existing shake-validation sequence with and without the case; include stationary, pick-up, single-shake, repeated-shake, and six-line flows | Raw IMU/logs, missed/extra detections, firmware thresholds, repetitions | No new false triggers or missed intended shakes; do not retune until baseline data is captured; **PENDING** |
| Thermal | Run representative idle and active workloads in a controlled ambient environment; measure device/case and battery area with a documented instrument | Ambient, workload, duration, sensor locations, time-series temperatures | No unsafe heating or adverse charging behavior; product-specific numeric limits require owner/hardware review; **PENDING** |
| Grip and pocket carry | Use representative one-hand operation and carry in intended clothing/pocket conditions; inspect for snagging and accidental button presses | Reviewer notes, clothing, carry duration, button/IMU events | Stakeholder sign-off that use and carry are acceptable; user/sample scope still to be agreed; **PENDING** |
| Assembly and serviceability | Install/remove repeatedly; inspect seams, retention, rattling, device enclosure, and ability to access service points | Cycle count, photos, failures, repair/removal steps | No cracks, loosening, trapped controls, or damage; assembly method and cycle target need approval; **PENDING** |
| Prototype acceptance | Review the assembled physical sample against the Issue #15 checklist | Signed checklist and sample identifier | A physical or explicitly approved equivalent validation article is available; **PENDING** |

## Reporting

Keep each unrun or inconclusive item marked `PENDING_HARDWARE_VALIDATION`. Attach raw observations and photos to the hardware review record; do not infer device performance from native tests, compilation, the STL, or OpenSCAD geometry. Only update the Issue/STATUS acceptance state after review of the actual sample and evidence.

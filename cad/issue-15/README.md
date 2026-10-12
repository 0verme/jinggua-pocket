# Issue #15 — StickS3 enclosure study

**Status: DRAFT / `PENDING_HARDWARE_VALIDATION`.** This is a parametric, open-access bumper concept and a traceable dimensional study. It is not a production-ready or physically validated enclosure. No StickS3 is available in this work environment; the OpenSCAD CLI is also unavailable, so no render, mesh inspection, fit, assembly, tactile, audio, thermal, IMU, or carry result is claimed.

## CAD

- [`sticks3_bumper.scad`](sticks3_bumper.scad) defines a rounded envelope bumper with front display/button apertures, rear access, and deliberately generous side/end windows.
- [`reference/StickS3.stl`](reference/StickS3.stl) is the unchanged M5Stack reference model. It is imported for preview only; the bumper uses simple parametric envelopes rather than a Boolean against the vendor assembly mesh.
- [`reference/LICENSE`](reference/LICENSE) preserves the upstream MIT notice.

After installing OpenSCAD, export the shell from this directory:

```sh
cd cad/issue-15
openscad -D 'part="shell"' -o /tmp/sticks3-bumper.stl sticks3_bumper.scad
```

The preview is the default. The fit clearance and wall thickness are editable SCAD parameters; varying a parameter creates a design variant, not evidence of fit. Test coupons should precede a full print.

## Traceable dimensional basis

The public M5Stack StickS3 specification gives a nominal **48.0 × 24.0 × 15.0 mm** body and 20 g product weight. Its model-size drawing calls out R3 corners and two rear M2 screw positions with 18 mm pitch and an 8.5 mm offset. The pinned vendor STL measures approximately **24.001 × 48.002 × 15.000 mm** for the first device group.

The front-face aperture measurements below are extracted from the pinned model mesh and independently checked by `test/python/test_sticks3_enclosure_reference.py`:

| Feature | Nominal model geometry | Center in CAD x/y coordinates | Treatment in concept |
| --- | ---: | ---: | --- |
| Display aperture | 16.030 × 26.091 mm, ~1.0 mm corner radius | 12.000, 30.970 mm | Adds 0.35 mm nominal clearance per side plus a 1.0 mm outer flare |
| Front button slot | 11.940 × 2.340 mm, ~1.17 mm corner radius | 12.000, 10.561 mm | Adds 0.35 mm nominal clearance per side plus a 0.40 mm outer flare |
| Rear circular model feature | ~12.0 mm diameter | 12.000, 24.060 mm | Broad rear opening leaves this and other rear features exposed; its acoustic function is not confirmed here |
| Rear screw centers | 18 mm pitch; model centers at x=3/21, y≈8.56 mm | 3.000/21.000, 8.560 mm | Rear opening and provisional 3.2 mm access holes; screw-head clearance is not verified |

Coordinates are x across device width and y along the 48 mm body. The bumper uses 0.35 mm fit/interface clearances and 1.2 mm nominal walls as **prototype assumptions**, producing a nominal 27.1 × 51.1 × 18.1 mm outer envelope. The wide side/end and rear openings are intentional: they avoid making unsupported microphone, speaker, USB-C, and button-location assumptions from incomplete public drawings. They reduce coverage and still require a physical port-by-port review.

## References

1. M5Stack, [StickS3 product documentation](https://docs.m5stack.com/en/core/StickS3), including nominal size and weight; checked 2026-10-12.
2. M5Stack, [StickS3 model-size drawing](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-sticks3.pdf), including body outline, R3 callout, and rear screw layout.
3. M5Stack, [M5_Hardware `StickS3.stl`](https://github.com/m5stack/M5_Hardware/blob/a240115c94b19ecf647f229c47fa9a8ce46ccdc4/Products/K150_StickS3/Structures/StickS3.stl), pinned to commit `a240115c94b19ecf647f229c47fa9a8ce46ccdc4`; SHA-256 `cadfa88716cb74a85d0d76dc866d83dbd627d6f5c310b528d43e588dab37ecec`.

The public documents/model are dimensional references, not a substitute for measuring the actual production unit. The model contains multiple assembly/export components; the SCAD preview clips it to the first device group.

## Unresolved before production

- Confirm actual unit dimensions and all control, USB-C, microphone, speaker, and connector positions against hardware.
- Confirm display viewing angle, button travel/force, fit and removal without preload or damage.
- Choose retention/assembly method and material/process from prototype evidence. TPU 95A is only a candidate for a flexible prototype; no production material or manufacturing release is selected.
- Complete pocket/grip, IMU shake, acoustic, thermal, serviceability, and sample/validation reviews in [`VALIDATION.md`](VALIDATION.md).

See [`BOM.md`](BOM.md) for prototype-only materials and [`VALIDATION.md`](VALIDATION.md) for the evidence plan. Do not mark Issue #15 accepted or close it based on these CAD/software checks. Physical acceptance remains `PENDING_HARDWARE_VALIDATION`.

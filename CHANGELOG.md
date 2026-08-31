# Changelog

All notable changes to `jinggua-pocket` are recorded here.

## Unreleased

### Added

- Phase 0 PlatformIO project targeting only M5Stack StickS3 / ESP32-S3.
- Host-testable three-coin, six-line divination domain model.
- King Wen order mapping for all 64 hexagrams.
- Button-driven application state machine and basic StickS3 renderer.
- IMU shake detector interface with provisional calibration values.

### Changed

- Enabled StickS3 shake casting with acceleration/gyro thresholds, peak
  release/debounce, and cooldown protection.
- A sequence of six Shake events now completes six lines; Button casting
  remains available and shares the same session cast path.

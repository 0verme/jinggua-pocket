# Changelog

All notable changes to `jinggua-pocket` are recorded here.

## Unreleased

### Added

- Phase 0 PlatformIO project targeting only M5Stack StickS3 / ESP32-S3.
- Host-testable three-coin, six-line divination domain model.
- King Wen order mapping for all 64 hexagrams.
- Button-driven application state machine and basic StickS3 renderer.
- IMU shake detector interface with provisional calibration values.
- Opt-in StickS3 microphone research environment with short PCM capture statistics.
- **Offline-first Wi-Fi connection flow (Issue #7):** `WifiController` interface
  (ports/adapter) + `Esp32WifiManager` non-blocking implementation.  Wi-Fi is
  off by default; only the user can trigger a connection from the Settings page.
  States: Off → Connecting → Connected / Failed / Timeout.  No auto-retry.
  Credentials injected via environment variables, never committed.

### Changed

- Polished the 135×240 StickS3 Pocket UI around one primary action per screen,
  shared layout/theme/typography constants, compact divination results, and
  user-facing History/Wi-Fi language.
- Re-generated the Noto Sans SC subset for the updated firmware copy.
- Enabled StickS3 shake casting with acceleration/gyro thresholds, peak
  release/debounce, and cooldown protection.
- A sequence of six Shake events now completes six lines; Button casting
  remains available and shares the same session cast path.

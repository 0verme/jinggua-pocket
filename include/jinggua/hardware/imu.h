#pragma once

#include <cstdint>

namespace jinggua::hardware {

// Shake detection remains available for native calibration tests, but is not
// part of the stable Button Mode firmware path unless explicitly enabled.
#ifndef JINGGUA_ENABLE_SHAKE_EXPERIMENTAL
#define JINGGUA_ENABLE_SHAKE_EXPERIMENTAL 0
#endif

struct ImuSample {
  float accelerationX{0.0F};
  float accelerationY{0.0F};
  float accelerationZ{0.0F};
  float angularVelocityX{0.0F};
  float angularVelocityY{0.0F};
  float angularVelocityZ{0.0F};
  std::uint32_t timestampMs{0};
};

struct ShakeDetectorConfig {
  // Provisional values. Calibrate against a real StickS3 in Hardware v0.2.
  float accelerationMagnitudeThresholdG{1.6F};
  std::uint32_t detectionWindowMs{500};
  std::uint32_t cooldownMs{1200};
  std::uint8_t requiredPeaks{2};
};

class ShakeDetector final {
 public:
  explicit ShakeDetector(
      ShakeDetectorConfig config = ShakeDetectorConfig{}) noexcept;

  bool update(const ImuSample& sample) noexcept;
  void reset() noexcept;

 private:
  ShakeDetectorConfig config_{};
  bool previousAboveThreshold_{false};
  bool candidateActive_{false};
  std::uint8_t peakCount_{0};
  std::uint32_t candidateStartedAt_{0};
  bool hasLastShake_{false};
  std::uint32_t lastShakeAt_{0};
};

class StickS3Imu final {
 public:
  void begin() noexcept;
  bool read(ImuSample& sample) noexcept;
};

}  // namespace jinggua::hardware

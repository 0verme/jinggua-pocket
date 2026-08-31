#pragma once

#include <cstdint>

namespace jinggua::hardware {

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
  // Conservative defaults for StickS3. Keep these values explicit so a
  // hardware calibration can be reviewed without changing the detector.
  float accelerationMagnitudeThresholdG{1.6F};
  std::uint32_t detectionWindowMs{500};
  std::uint32_t cooldownMs{1200};
  std::uint8_t requiredPeaks{2};
  float angularVelocityMagnitudeThresholdDps{180.0F};
  float releaseAccelerationMagnitudeThresholdG{1.2F};
  float releaseAngularVelocityMagnitudeThresholdDps{60.0F};
  std::uint32_t minimumPeakIntervalMs{80};
};

class ShakeDetector final {
 public:
  explicit ShakeDetector(
      ShakeDetectorConfig config = ShakeDetectorConfig{}) noexcept;

  bool update(const ImuSample& sample) noexcept;
  void reset() noexcept;

 private:
  ShakeDetectorConfig config_{};
  bool readyForPeak_{true};
  bool candidateActive_{false};
  std::uint8_t peakCount_{0};
  std::uint32_t candidateStartedAt_{0};
  bool hasLastPeak_{false};
  std::uint32_t lastPeakAt_{0};
  bool hasLastShake_{false};
  std::uint32_t lastShakeAt_{0};
};

class StickS3Imu final {
 public:
  void begin() noexcept;
  bool read(ImuSample& sample) noexcept;
};

}  // namespace jinggua::hardware

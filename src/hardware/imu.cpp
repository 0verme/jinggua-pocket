#include "jinggua/hardware/imu.h"

#include <cmath>

#if defined(ARDUINO)
#include <M5Unified.h>
#endif

namespace jinggua::hardware {
namespace {

float accelerationMagnitude(const ImuSample& sample) noexcept {
  return std::sqrt(sample.accelerationX * sample.accelerationX +
                   sample.accelerationY * sample.accelerationY +
                   sample.accelerationZ * sample.accelerationZ);
}

std::uint32_t elapsedSince(std::uint32_t now,
                           std::uint32_t then) noexcept {
  return now - then;
}

}  // namespace

ShakeDetector::ShakeDetector(ShakeDetectorConfig config) noexcept
    : config_(config) {
  if (config_.requiredPeaks == 0) {
    config_.requiredPeaks = 1;
  }
}

bool ShakeDetector::update(const ImuSample& sample) noexcept {
  const bool aboveThreshold =
      accelerationMagnitude(sample) >= config_.accelerationMagnitudeThresholdG;

  if (hasLastShake_ &&
      elapsedSince(sample.timestampMs, lastShakeAt_) < config_.cooldownMs) {
    previousAboveThreshold_ = aboveThreshold;
    candidateActive_ = false;
    peakCount_ = 0;
    return false;
  }

  if (candidateActive_ &&
      elapsedSince(sample.timestampMs, candidateStartedAt_) >
          config_.detectionWindowMs) {
    candidateActive_ = false;
    peakCount_ = 0;
  }

  if (aboveThreshold && !previousAboveThreshold_) {
    if (!candidateActive_) {
      candidateActive_ = true;
      candidateStartedAt_ = sample.timestampMs;
      peakCount_ = 0;
    }
    ++peakCount_;
    if (peakCount_ >= config_.requiredPeaks) {
      hasLastShake_ = true;
      lastShakeAt_ = sample.timestampMs;
      candidateActive_ = false;
      peakCount_ = 0;
      previousAboveThreshold_ = aboveThreshold;
      return true;
    }
  }

  previousAboveThreshold_ = aboveThreshold;
  return false;
}

void ShakeDetector::reset() noexcept {
  previousAboveThreshold_ = false;
  candidateActive_ = false;
  peakCount_ = 0;
  candidateStartedAt_ = 0;
  hasLastShake_ = false;
  lastShakeAt_ = 0;
}

void StickS3Imu::begin() noexcept {
  // M5Unified initializes the BMI270 through M5.begin().
}

bool StickS3Imu::read(ImuSample& sample) noexcept {
#if defined(ARDUINO)
  if (!M5.Imu.update()) {
    return false;
  }
  const auto data = M5.Imu.getImuData();
  sample.accelerationX = data.accel.x;
  sample.accelerationY = data.accel.y;
  sample.accelerationZ = data.accel.z;
  sample.angularVelocityX = data.gyro.x;
  sample.angularVelocityY = data.gyro.y;
  sample.angularVelocityZ = data.gyro.z;
  sample.timestampMs = static_cast<std::uint32_t>(::millis());
  return true;
#else
  (void)sample;
  return false;
#endif
}

}  // namespace jinggua::hardware

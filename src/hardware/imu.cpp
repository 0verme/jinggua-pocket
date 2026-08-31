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

float angularVelocityMagnitude(const ImuSample& sample) noexcept {
  return std::sqrt(sample.angularVelocityX * sample.angularVelocityX +
                   sample.angularVelocityY * sample.angularVelocityY +
                   sample.angularVelocityZ * sample.angularVelocityZ);
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
  const float acceleration = accelerationMagnitude(sample);
  const float angularVelocity = angularVelocityMagnitude(sample);
  const bool aboveThreshold =
      acceleration >= config_.accelerationMagnitudeThresholdG ||
      angularVelocity >= config_.angularVelocityMagnitudeThresholdDps;
  const bool belowReleaseThreshold =
      acceleration <= config_.releaseAccelerationMagnitudeThresholdG &&
      angularVelocity <= config_.releaseAngularVelocityMagnitudeThresholdDps;

  if (hasLastShake_ &&
      elapsedSince(sample.timestampMs, lastShakeAt_) < config_.cooldownMs) {
    if (belowReleaseThreshold) {
      readyForPeak_ = true;
    }
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

  if (belowReleaseThreshold) {
    readyForPeak_ = true;
  }

  if (aboveThreshold && readyForPeak_) {
    const bool sufficientlySeparated =
        !hasLastPeak_ ||
        elapsedSince(sample.timestampMs, lastPeakAt_) >=
            config_.minimumPeakIntervalMs;
    if (sufficientlySeparated) {
      if (!candidateActive_) {
        candidateActive_ = true;
        candidateStartedAt_ = sample.timestampMs;
        peakCount_ = 0;
      }
      ++peakCount_;
      lastPeakAt_ = sample.timestampMs;
      hasLastPeak_ = true;
      readyForPeak_ = false;
      if (peakCount_ >= config_.requiredPeaks) {
        hasLastShake_ = true;
        lastShakeAt_ = sample.timestampMs;
        candidateActive_ = false;
        peakCount_ = 0;
        return true;
      }
    }
  }

  return false;
}

void ShakeDetector::reset() noexcept {
  readyForPeak_ = true;
  candidateActive_ = false;
  peakCount_ = 0;
  candidateStartedAt_ = 0;
  hasLastPeak_ = false;
  lastPeakAt_ = 0;
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

#pragma once

#include <cstddef>
#include <cstdint>

namespace jinggua::hardware {

struct MicResearchConfig {
  std::uint32_t sampleRateHz = 16000;
  std::size_t blockSamples = 256;
  std::size_t dmaBufferCount = 4;
  std::uint32_t captureDurationMs = 3000;
};

struct MicCaptureStats {
  std::uint32_t requestedDurationMs = 0;
  std::uint32_t elapsedMs = 0;
  std::uint32_t sampleRateHz = 0;
  std::size_t sampleCount = 0;
  std::size_t blockSamples = 0;
  std::size_t blockBytes = 0;
  std::size_t dmaBufferCount = 0;
  std::int16_t minimum = 0;
  std::int16_t maximum = 0;
  std::int32_t meanMilli = 0;
  std::uint32_t rmsMilli = 0;
  std::uint32_t peak = 0;
  std::uint32_t clippedSamples = 0;
  std::uint32_t zeroCrossings = 0;
  std::uint32_t freeHeapBefore = 0;
  std::uint32_t freeHeapAfter = 0;
  std::uint32_t freePsramBefore = 0;
  std::uint32_t freePsramAfter = 0;
};

/// Opt-in StickS3 microphone capture probe for hardware research.
///
/// The probe computes signal statistics in RAM and never persists or uploads
/// the captured PCM samples. It is not part of the product voice flow.
class StickS3MicrophoneResearch {
 public:
  explicit StickS3MicrophoneResearch(
      MicResearchConfig config = MicResearchConfig{}) noexcept
      : config_(config) {}

  bool begin() noexcept;
  bool capture(MicCaptureStats& stats) noexcept;

  const MicResearchConfig& config() const noexcept { return config_; }

 private:
  MicResearchConfig config_;
};

}  // namespace jinggua::hardware

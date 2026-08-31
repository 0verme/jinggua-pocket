#include "jinggua/hardware/microphone_research.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

#if defined(ARDUINO) && defined(JINGGUA_ENABLE_MIC_RESEARCH) && \
    JINGGUA_ENABLE_MIC_RESEARCH
#include <Arduino.h>
#include <M5Unified.h>
#include <esp_heap_caps.h>
#endif

namespace jinggua::hardware {

bool StickS3MicrophoneResearch::begin() noexcept {
#if defined(ARDUINO) && defined(JINGGUA_ENABLE_MIC_RESEARCH) && \
    JINGGUA_ENABLE_MIC_RESEARCH
  if (config_.sampleRateHz == 0 || config_.blockSamples == 0 ||
      config_.dmaBufferCount == 0 || config_.captureDurationMs == 0) {
    return false;
  }

  auto micConfig = M5.Mic.config();
  micConfig.sample_rate = config_.sampleRateHz;
  micConfig.dma_buf_len = config_.blockSamples;
  micConfig.dma_buf_count = config_.dmaBufferCount;
  micConfig.over_sampling = 1;
  micConfig.noise_filter_level = 0;
  micConfig.magnification = 1;
  micConfig.stereo = false;
  M5.Mic.config(micConfig);
  return M5.Mic.begin() && M5.Mic.isEnabled();
#else
  return false;
#endif
}

bool StickS3MicrophoneResearch::capture(MicCaptureStats& stats) noexcept {
  stats = MicCaptureStats{};
  stats.requestedDurationMs = config_.captureDurationMs;
  stats.sampleRateHz = config_.sampleRateHz;
  stats.blockSamples = config_.blockSamples;
  stats.blockBytes = config_.blockSamples * sizeof(std::int16_t);
  stats.dmaBufferCount = config_.dmaBufferCount;

#if defined(ARDUINO) && defined(JINGGUA_ENABLE_MIC_RESEARCH) && \
    JINGGUA_ENABLE_MIC_RESEARCH
  if (!M5.Mic.isEnabled() || config_.sampleRateHz == 0 ||
      config_.blockSamples == 0 || config_.captureDurationMs == 0) {
    return false;
  }

  const auto targetSamples =
      (static_cast<std::uint64_t>(config_.sampleRateHz) *
       config_.captureDurationMs) /
      1000U;
  if (targetSamples == 0 || targetSamples > std::numeric_limits<std::size_t>::max()) {
    return false;
  }

  if (config_.blockSamples >
      std::numeric_limits<std::size_t>::max() / sizeof(std::int16_t)) {
    return false;
  }

  stats.freeHeapBefore = ESP.getFreeHeap();
  stats.freePsramBefore = ESP.getFreePsram();
  auto* block = static_cast<std::int16_t*>(heap_caps_malloc(
      stats.blockBytes, MALLOC_CAP_8BIT));
  if (block == nullptr) {
    return false;
  }

  const auto startedAt = millis();
  std::int64_t sum = 0;
  std::uint64_t sumSquares = 0;
  std::int16_t previous = 0;
  bool hasPrevious = false;
  stats.minimum = std::numeric_limits<std::int16_t>::max();
  stats.maximum = std::numeric_limits<std::int16_t>::min();

  while (stats.sampleCount < targetSamples) {
    const auto remaining = targetSamples - stats.sampleCount;
    const auto requested = static_cast<std::size_t>(std::min<std::uint64_t>(
        remaining, config_.blockSamples));
    if (!M5.Mic.record(block, requested, config_.sampleRateHz, false)) {
      heap_caps_free(block);
      return false;
    }
    // record() queues the block to M5Unified's capture task. Waiting for both
    // slots to become free makes the samples safe to inspect before reuse.
    while (M5.Mic.isRecording() != 0) {
      M5.delay(1);
    }

    for (std::size_t index = 0; index < requested; ++index) {
      const auto sample = block[index];
      const auto wide = static_cast<std::int32_t>(sample);
      const auto magnitude = static_cast<std::uint32_t>(
          std::abs(static_cast<std::int64_t>(wide)));
      stats.minimum = std::min(stats.minimum, sample);
      stats.maximum = std::max(stats.maximum, sample);
      stats.peak = std::max(stats.peak, magnitude);
      stats.clippedSamples +=
          (sample == std::numeric_limits<std::int16_t>::min() ||
           sample == std::numeric_limits<std::int16_t>::max())
              ? 1U
              : 0U;
      sum += wide;
      sumSquares += static_cast<std::uint64_t>(wide * wide);
      if (hasPrevious && ((previous < 0 && sample >= 0) ||
                          (previous >= 0 && sample < 0))) {
        ++stats.zeroCrossings;
      }
      previous = sample;
      hasPrevious = true;
    }
    stats.sampleCount += requested;
  }

  stats.elapsedMs = millis() - startedAt;
  stats.meanMilli = static_cast<std::int32_t>(
      (sum * 1000) / static_cast<std::int64_t>(stats.sampleCount));
  const auto meanSquare = static_cast<double>(sumSquares) /
                          static_cast<double>(stats.sampleCount);
  stats.rmsMilli = static_cast<std::uint32_t>(
      std::sqrt(meanSquare) * 1000.0);
  heap_caps_free(block);
  stats.freeHeapAfter = ESP.getFreeHeap();
  stats.freePsramAfter = ESP.getFreePsram();
  return true;
#else
  (void)stats;
  return false;
#endif
}

}  // namespace jinggua::hardware

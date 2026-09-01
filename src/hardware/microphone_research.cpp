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

namespace {

constexpr std::size_t kMicTaskStackBase = 2048;
constexpr std::uint32_t kMicResearchBlockTimeoutMs = 2000;

bool isValidConfig(const MicResearchConfig& config) noexcept {
  const auto maxSize = std::numeric_limits<std::size_t>::max();
  if (config.sampleRateHz == 0 || config.blockSamples == 0 ||
      config.dmaBufferCount == 0 || config.captureDurationMs == 0 ||
      config.captureDurationMs > kMicResearchMaxCaptureDurationMs) {
    return false;
  }
  if (config.blockSamples > maxSize / sizeof(std::int16_t)) {
    return false;
  }

  const auto blockBytes = config.blockSamples * sizeof(std::int16_t);
  if (blockBytes > maxSize / 2 ||
      config.dmaBufferCount > maxSize / blockBytes) {
    return false;
  }
  if (maxSize < kMicTaskStackBase ||
      config.blockSamples >
          (maxSize - kMicTaskStackBase) / sizeof(std::uint32_t)) {
    return false;
  }

  const auto targetSamples =
      (static_cast<std::uint64_t>(config.sampleRateHz) *
       config.captureDurationMs) /
      1000U;
  return targetSamples != 0 && targetSamples <= maxSize &&
         targetSamples <= maxSize / sizeof(std::int16_t);
}

}  // namespace

bool StickS3MicrophoneResearch::begin() noexcept {
#if defined(ARDUINO) && defined(JINGGUA_ENABLE_MIC_RESEARCH) && \
    JINGGUA_ENABLE_MIC_RESEARCH
  if (!isValidConfig(config_)) {
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
  micConfig.left_channel = false;
  micConfig.use_adc = false;
  M5.Mic.config(micConfig);
  return M5.Mic.begin() && M5.Mic.isEnabled();
#else
  return false;
#endif
}

bool StickS3MicrophoneResearch::configure(MicResearchConfig config) noexcept {
  if (!isValidConfig(config)) {
    return false;
  }
#if defined(ARDUINO) && defined(JINGGUA_ENABLE_MIC_RESEARCH) && \
    JINGGUA_ENABLE_MIC_RESEARCH
  if (M5.Mic.isRunning()) {
    return false;
  }
#endif
  config_ = config;
  return true;
}

void StickS3MicrophoneResearch::end() noexcept {
#if defined(ARDUINO) && defined(JINGGUA_ENABLE_MIC_RESEARCH) && \
    JINGGUA_ENABLE_MIC_RESEARCH
  M5.Mic.end();
#endif
}

bool StickS3MicrophoneResearch::capture(MicCaptureStats& stats) noexcept {
  stats = MicCaptureStats{};
  stats.requestedDurationMs = config_.captureDurationMs;
  stats.sampleRateHz = config_.sampleRateHz;
  stats.blockSamples = config_.blockSamples;
  stats.dmaBufferCount = config_.dmaBufferCount;

  if (!isValidConfig(config_)) {
    return false;
  }

  stats.blockBytes = config_.blockSamples * sizeof(std::int16_t);
  const auto targetSamples =
      (static_cast<std::uint64_t>(config_.sampleRateHz) *
       config_.captureDurationMs) /
      1000U;
  stats.expectedPcmBytes = static_cast<std::size_t>(targetSamples) *
                           sizeof(std::int16_t);
  stats.dmaPayloadBytesEstimate =
      stats.blockBytes * config_.dmaBufferCount;
  stats.dmaReadChunkBytes = stats.blockBytes * 2;
  stats.peakTemporaryBufferBytes = stats.blockBytes;

#if defined(ARDUINO) && defined(JINGGUA_ENABLE_MIC_RESEARCH) && \
    JINGGUA_ENABLE_MIC_RESEARCH
  stats.freeHeapBefore = ESP.getFreeHeap();
  stats.freePsramBefore = ESP.getFreePsram();
  if (!begin()) {
    end();
    stats.freeHeapAfter = ESP.getFreeHeap();
    stats.freePsramAfter = ESP.getFreePsram();
    return false;
  }

  auto* block = static_cast<std::int16_t*>(heap_caps_malloc(
      stats.blockBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (block == nullptr) {
    end();
    stats.freeHeapAfter = ESP.getFreeHeap();
    stats.freePsramAfter = ESP.getFreePsram();
    return false;
  }
  stats.temporaryBufferInternalBytes = stats.blockBytes;

  bool hasDuringResourceSample = false;
  const auto observeResources = [&]() {
    const auto freeHeap = ESP.getFreeHeap();
    const auto freePsram = ESP.getFreePsram();
    if (!hasDuringResourceSample) {
      stats.freeHeapDuringMin = freeHeap;
      stats.freePsramDuringMin = freePsram;
      hasDuringResourceSample = true;
    } else {
      stats.freeHeapDuringMin =
          std::min(stats.freeHeapDuringMin, freeHeap);
      stats.freePsramDuringMin =
          std::min(stats.freePsramDuringMin, freePsram);
    }
  };
  const auto observeRecordingState = [&]() {
    const auto activeSlots = M5.Mic.isRecording();
    stats.maxRecordingSlots = std::max(
        stats.maxRecordingSlots, static_cast<std::uint32_t>(activeSlots));
    if (activeSlots >= 2) {
      ++stats.queueFullObservations;
    }
  };

  observeResources();
  const auto startedAt = millis();
  long double sum = 0.0L;
  long double sumSquares = 0.0L;
  std::int16_t previous = 0;
  bool hasPrevious = false;
  stats.minimum = std::numeric_limits<std::int16_t>::max();
  stats.maximum = std::numeric_limits<std::int16_t>::min();

  const auto finishCapture = [&]() {
    stats.elapsedMs = millis() - startedAt;
    if (stats.elapsedMs != 0) {
      stats.effectiveSampleRateHz = static_cast<std::uint32_t>(
          (static_cast<std::uint64_t>(stats.sampleCount) * 1000U) /
          stats.elapsedMs);
    }
    end();
    heap_caps_free(block);
    block = nullptr;
    stats.freeHeapAfter = ESP.getFreeHeap();
    stats.freePsramAfter = ESP.getFreePsram();
  };

  while (stats.sampleCount < targetSamples) {
    const auto remaining = targetSamples - stats.sampleCount;
    const auto requested = static_cast<std::size_t>(std::min<std::uint64_t>(
        remaining, config_.blockSamples));
    if (!M5.Mic.record(block, requested, config_.sampleRateHz, false)) {
      ++stats.recordFailures;
      finishCapture();
      return false;
    }

    observeRecordingState();
    const auto waitStartedAt = millis();
    while (M5.Mic.isRecording() != 0) {
      observeRecordingState();
      observeResources();
      if (millis() - waitStartedAt >= kMicResearchBlockTimeoutMs) {
        ++stats.recordTimeouts;
        finishCapture();
        return false;
      }
      M5.delay(1);
    }
    stats.waitBusyMs += millis() - waitStartedAt;

    // record() queues the block to M5Unified's capture task. Waiting for both
    // slots to become free makes the samples safe to inspect before reuse.
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
      sum += static_cast<long double>(wide);
      sumSquares += static_cast<long double>(wide) * wide;
      if (hasPrevious && ((previous < 0 && sample >= 0) ||
                          (previous >= 0 && sample < 0))) {
        ++stats.zeroCrossings;
      }
      previous = sample;
      hasPrevious = true;
    }
    stats.sampleCount += requested;
    ++stats.blocksCaptured;
    observeResources();
  }

  stats.meanMilli = static_cast<std::int32_t>(
      (sum * 1000.0L) / static_cast<long double>(stats.sampleCount));
  const auto meanSquare =
      sumSquares / static_cast<long double>(stats.sampleCount);
  stats.rmsMilli = static_cast<std::uint32_t>(
      std::sqrt(meanSquare) * 1000.0L);
  finishCapture();

  const auto tolerance = std::max<std::uint32_t>(
      1U, stats.sampleRateHz / 20U);
  const auto rateDifference =
      stats.effectiveSampleRateHz > stats.sampleRateHz
          ? stats.effectiveSampleRateHz - stats.sampleRateHz
          : stats.sampleRateHz - stats.effectiveSampleRateHz;
  stats.sampleRateStable =
      stats.effectiveSampleRateHz != 0 && rateDifference <= tolerance &&
      stats.queueFullObservations == 0 && stats.recordFailures == 0 &&
      stats.recordTimeouts == 0;
  return true;
#else
  (void)stats;
  return false;
#endif
}

}  // namespace jinggua::hardware

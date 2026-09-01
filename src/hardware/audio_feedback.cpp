#include "jinggua/hardware/audio_feedback.h"

#include <cstdint>

#if defined(ARDUINO)
#include <M5Unified.h>
#endif

namespace jinggua::hardware {
namespace {

constexpr std::uint8_t kSpeakerChannel = 0;
constexpr std::uint8_t kSpeakerVolume = 48;

constexpr float kStartFrequencyHz = 260.0F;
constexpr std::uint32_t kStartDurationMs = 24;
constexpr float kCastFrequencyHz = 440.0F;
constexpr std::uint32_t kCastDurationMs = 28;
constexpr float kCompleteFirstFrequencyHz = 520.0F;
constexpr std::uint32_t kCompleteFirstDurationMs = 22;
constexpr float kCompleteSecondFrequencyHz = 720.0F;
constexpr std::uint32_t kCompleteSecondDurationMs = 36;
constexpr float kErrorFrequencyHz = 180.0F;
constexpr std::uint32_t kErrorDurationMs = 32;

#if defined(ARDUINO)
bool queueTone(float frequencyHz, std::uint32_t durationMs,
               bool stopCurrent) noexcept {
  // Keep one virtual channel and drop a request when both M5Unified slots are
  // occupied. The newest interaction never creates an unbounded audio queue.
  if (M5.Speaker.isPlaying(kSpeakerChannel) >= 2U) {
    return false;
  }
  return M5.Speaker.tone(frequencyHz, durationMs, kSpeakerChannel,
                         stopCurrent);
}
#endif

}  // namespace

bool StickS3AudioController::begin() noexcept {
#if defined(ARDUINO)
  // Display::begin() has already called M5.begin() and selected the StickS3
  // codec pins. Speaker_Class remains lazy: no task or I2S TX channel is
  // started until the first accepted cue.
  ready_ = M5.Speaker.isEnabled();
  M5.Speaker.setVolume(soundEnabled_ ? kSpeakerVolume : 0);
#else
  ready_ = false;
#endif
  return ready_;
}

void StickS3AudioController::play(application::SoundCue cue) noexcept {
  if (!ready_ || !soundEnabled_) {
    return;
  }

#if defined(ARDUINO)
  switch (cue) {
    case application::SoundCue::Start:
      static_cast<void>(queueTone(kStartFrequencyHz, kStartDurationMs, true));
      break;
    case application::SoundCue::Cast:
      static_cast<void>(queueTone(kCastFrequencyHz, kCastDurationMs, true));
      break;
    case application::SoundCue::Complete:
      if (queueTone(kCompleteFirstFrequencyHz, kCompleteFirstDurationMs,
                    true)) {
        // The second note is queued only behind the first accepted note. If a
        // previous sound already occupies both slots, the cue is coalesced.
        static_cast<void>(queueTone(kCompleteSecondFrequencyHz,
                                    kCompleteSecondDurationMs, false));
      }
      break;
    case application::SoundCue::Error:
      static_cast<void>(queueTone(kErrorFrequencyHz, kErrorDurationMs, true));
      break;
  }
#else
  (void)cue;
#endif
}

void StickS3AudioController::setEnabled(bool enabled) noexcept {
  if (soundEnabled_ == enabled) {
    return;
  }
  soundEnabled_ = enabled;

#if defined(ARDUINO)
  if (ready_) {
    if (!enabled) {
      M5.Speaker.stop(kSpeakerChannel);
    }
    M5.Speaker.setVolume(enabled ? kSpeakerVolume : 0);
  }
#endif
}

}  // namespace jinggua::hardware

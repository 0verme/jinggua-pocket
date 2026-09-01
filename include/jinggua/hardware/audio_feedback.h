#pragma once

#include "jinggua/application/audio_controller.h"

namespace jinggua::hardware {

// StickS3 audio adapter backed by M5Unified's asynchronous Speaker_Class.
// M5Unified configures the codec and I2S pins during Display::begin(); this
// class only enables the lazy speaker task when a cue is actually played.
class StickS3AudioController final : public application::AudioController {
 public:
  bool begin() noexcept;
  bool isReady() const noexcept { return ready_; }

  void play(application::SoundCue cue) noexcept override;
  void setEnabled(bool enabled) noexcept override;
  bool enabled() const noexcept override { return soundEnabled_; }

 private:
  bool ready_{false};
  bool soundEnabled_{true};
};

}  // namespace jinggua::hardware

#pragma once

namespace jinggua::application {

enum class SoundCue {
  Start,
  Cast,
  Complete,
  Error,
};

// Audio output port. Application code emits semantic cues only; the hardware
// adapter decides how to synthesize and schedule each cue.
class AudioController {
 public:
  virtual ~AudioController() = default;

  virtual void play(SoundCue cue) noexcept = 0;
  virtual void setEnabled(bool enabled) noexcept = 0;
  virtual bool enabled() const noexcept = 0;
};

}  // namespace jinggua::application

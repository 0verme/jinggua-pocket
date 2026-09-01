#include "jinggua/application/state_machine.h"

#include <cstddef>
#include <cstdint>

#include "jinggua/domain/history_record.h"

namespace jinggua::application {

const char* appStateName(AppState state) noexcept {
  switch (state) {
    case AppState::Boot:
      return "BOOT";
    case AppState::Welcome:
      return "WELCOME";
    case AppState::Prepare:
      return "PREPARE";
    case AppState::Casting:
      return "CASTING";
    case AppState::LineResult:
      return "LINE_RESULT";
    case AppState::HexagramResult:
      return "HEXAGRAM_RESULT";
    case AppState::TransformedResult:
      return "TRANSFORMED_RESULT";
    case AppState::ResetConfirm:
      return "RESET_CONFIRM";
    case AppState::History:
      return "HISTORY";
    case AppState::Settings:
      return "SETTINGS";
    case AppState::WifiConnecting:
      return "WIFI_CONNECTING";
    case AppState::WifiConnected:
      return "WIFI_CONNECTED";
    case AppState::WifiFailed:
      return "WIFI_FAILED";
    case AppState::WifiTimeout:
      return "WIFI_TIMEOUT";
  }
  return "UNKNOWN";
}

StateMachine::StateMachine(DivinationSession& session,
                           HistoryStore* history) noexcept
    : session_(session), history_(history) {}

StateMachine::StateMachine(DivinationSession& session, WifiController& wifi,
                           HistoryStore* history) noexcept
    : session_(session), history_(history), wifi_(&wifi) {}

void StateMachine::setAudioController(AudioController& audio) noexcept {
  audio_ = &audio;
  audio_->setEnabled(soundEnabled_);
}

void StateMachine::setSoundEnabled(bool enabled) noexcept {
  if (soundEnabled_ == enabled) {
    return;
  }
  soundEnabled_ = enabled;
  if (audio_ != nullptr) {
    audio_->setEnabled(enabled);
  }
  dirty_ = true;
}

void StateMachine::begin() noexcept {
  if (state_ == AppState::Boot) {
    transitionTo(AppState::Welcome);
  }
}

void StateMachine::handleInput(InputEvent event) noexcept {
  if (event == InputEvent::None || lineAnimationActive_) {
    return;
  }

  switch (state_) {
    case AppState::Boot:
      begin();
      break;
    case AppState::Welcome:
      if (event == InputEvent::PrimaryClick) {
        transitionTo(AppState::Prepare);
      } else if (event == InputEvent::SecondaryClick) {
        transitionTo(wifi_ != nullptr ? AppState::Settings : AppState::History);
      } else if (event == InputEvent::LongPress) {
        // Keep the Wi-Fi shortcut on B while exposing history on the long
        // press path when both optional features are available.
        transitionTo(AppState::History);
      }
      break;
    case AppState::Prepare:
      if (event == InputEvent::PrimaryClick) {
        session_.reset();
        transitionTo(AppState::Casting);
        playSound(SoundCue::Start);
      }
      break;
    case AppState::Casting:
      if (event == InputEvent::PrimaryClick || event == InputEvent::Shake) {
        castLine();
      }
      break;
    case AppState::LineResult:
      if (event == InputEvent::Shake && !session_.isComplete()) {
        // Shake is an acknowledgement and the next cast in one gesture, so
        // six shakes can complete a session without requiring button input.
        castLine();
      } else if (event == InputEvent::PrimaryClick) {
        transitionTo(session_.isComplete() ? AppState::HexagramResult
                                            : AppState::Casting);
      }
      break;
    case AppState::HexagramResult:
      if (event == InputEvent::PrimaryClick) {
        handleResultAction();
      }
      break;
    case AppState::TransformedResult:
      if (event == InputEvent::PrimaryClick) {
        resetReturnState_ = AppState::TransformedResult;
        transitionTo(AppState::ResetConfirm);
      }
      break;
    case AppState::ResetConfirm:
      if (event == InputEvent::PrimaryClick || event == InputEvent::LongPress) {
        session_.reset();
        transitionTo(AppState::Prepare);
      } else if (event == InputEvent::SecondaryClick) {
        transitionTo(resetReturnState_);
      }
      break;
    case AppState::History:
      if (event == InputEvent::PrimaryClick) {
        moveHistoryCursor(-1);  // A = older
      } else if (event == InputEvent::SecondaryClick) {
        moveHistoryCursor(1);  // B = newer
      } else if (event == InputEvent::LongPress) {
        transitionTo(AppState::Welcome);
      }
      break;
    case AppState::Settings:
      if (event == InputEvent::PrimaryClick) {
        // Only the user can trigger a connection. If no credentials are
        // configured the controller refuses and we show a failure screen.
        if (wifi_->enable()) {
          transitionTo(AppState::WifiConnecting);
        } else {
          transitionTo(AppState::WifiFailed);
        }
      } else if (event == InputEvent::SecondaryClick) {
        transitionTo(AppState::Welcome);
      } else if (event == InputEvent::LongPress) {
        // Keep the existing Wi-Fi settings flow on A/B and use the existing
        // long-press gesture for the small runtime-only sound toggle.
        toggleSoundEnabled();
      }
      break;
    case AppState::WifiConnecting:
      if (event == InputEvent::SecondaryClick) {
        wifi_->disable();
        transitionTo(AppState::Settings);
      }
      break;
    case AppState::WifiConnected:
      if (event == InputEvent::PrimaryClick) {
        wifi_->disable();
        transitionTo(AppState::Settings);
      }
      break;
    case AppState::WifiFailed:
      if (event == InputEvent::PrimaryClick) {
        if (wifi_->enable()) {
          transitionTo(AppState::WifiConnecting);
        }
      } else if (event == InputEvent::SecondaryClick) {
        transitionTo(AppState::Settings);
      }
      break;
    case AppState::WifiTimeout:
      if (event == InputEvent::PrimaryClick) {
        if (wifi_->enable()) {
          transitionTo(AppState::WifiConnecting);
        }
      } else if (event == InputEvent::SecondaryClick) {
        transitionTo(AppState::Settings);
      }
      break;
  }
}

bool StateMachine::sleepAllowed() const noexcept {
  if (lineAnimationActive_) {
    return false;
  }

  switch (state_) {
    case AppState::ResetConfirm:
    case AppState::WifiConnecting:
    case AppState::WifiConnected:
      return false;
    default:
      return true;
  }
}

void StateMachine::update(std::uint32_t nowMs) noexcept {
  if (wifi_ == nullptr || state_ != AppState::WifiConnecting) {
    return;
  }
  switch (wifi_->update(nowMs)) {
    case WifiState::Connected:
      transitionTo(AppState::WifiConnected);
      break;
    case WifiState::Failed:
      transitionTo(AppState::WifiFailed);
      break;
    case WifiState::Timeout:
      transitionTo(AppState::WifiTimeout);
      break;
    default:
      break;
  }
}

void StateMachine::finishLineAnimation() noexcept {
  if (!lineAnimationActive_) {
    return;
  }
  lineAnimationActive_ = false;
  dirty_ = true;
}

void StateMachine::castLine() noexcept {
  if (session_.castLine()) {
    lineAnimationActive_ = true;
    // The final line uses Complete instead of Cast, so one gesture produces
    // one cue and never plays both confirmations at the same time.
    playSound(session_.isComplete() ? SoundCue::Complete : SoundCue::Cast);
    persistIfComplete();
    transitionTo(AppState::LineResult);
    // The state does not change when Shake advances from one result to the
    // next, but the renderer still needs to show the new line.
    dirty_ = true;
  }
}

void StateMachine::playSound(SoundCue cue) noexcept {
  if (soundEnabled_ && audio_ != nullptr) {
    audio_->play(cue);
  }
}

void StateMachine::persistIfComplete() noexcept {
  if (history_ == nullptr || !session_.isComplete()) {
    return;
  }
  const auto& result = session_.result();
  if (!result.has_value()) {
    return;
  }
  auto record = domain::makeHistoryRecord(*result);
  if (record.has_value()) {
    history_->add(*record);
  }
}

void StateMachine::moveHistoryCursor(int delta) noexcept {
  if (history_ == nullptr || history_->count() == 0) {
    return;
  }
  const std::size_t count = history_->count();
  const std::int64_t current =
      static_cast<std::int64_t>(historyCursor_);
  const std::int64_t next = current + delta;
  if (next < 0) {
    historyCursor_ = 0;
  } else if (static_cast<std::size_t>(next) >= count) {
    historyCursor_ = count - 1;
  } else {
    historyCursor_ = static_cast<std::size_t>(next);
  }
  dirty_ = true;
}

void StateMachine::transitionTo(AppState next) noexcept {
  if (state_ == next) {
    return;
  }
  state_ = next;
  if (next == AppState::History) {
    // Start browsing at the newest record.
    historyCursor_ = (history_ != nullptr && history_->count() > 0)
                         ? history_->count() - 1
                         : 0;
  }
  if (next == AppState::WifiFailed || next == AppState::WifiTimeout) {
    // A failure screen is entered once per failed attempt; ordinary ignored
    // button input remains silent and cannot spam the error cue.
    playSound(SoundCue::Error);
  }
  dirty_ = true;
}

void StateMachine::handleResultAction() noexcept {
  const auto& result = session_.result();
  if (result.has_value() && result->hasMovingLines()) {
    transitionTo(AppState::TransformedResult);
    return;
  }

  resetReturnState_ = AppState::HexagramResult;
  transitionTo(AppState::ResetConfirm);
}

}  // namespace jinggua::application

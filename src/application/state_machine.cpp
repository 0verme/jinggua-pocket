#include "jinggua/application/state_machine.h"

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
                           WifiController& wifi) noexcept
    : session_(session), wifi_(wifi) {}

void StateMachine::begin() noexcept {
  if (state_ == AppState::Boot) {
    transitionTo(AppState::Welcome);
  }
}

void StateMachine::handleInput(InputEvent event) noexcept {
  if (event == InputEvent::None) {
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
        transitionTo(AppState::Settings);
      }
      break;
    case AppState::Prepare:
      if (event == InputEvent::PrimaryClick) {
        session_.reset();
        transitionTo(AppState::Casting);
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
    case AppState::Settings:
      if (event == InputEvent::PrimaryClick) {
        // Only the user can trigger a connection. If no credentials are
        // configured the controller refuses and we show a failure screen.
        if (wifi_.enable()) {
          transitionTo(AppState::WifiConnecting);
        } else {
          transitionTo(AppState::WifiFailed);
        }
      } else if (event == InputEvent::SecondaryClick) {
        transitionTo(AppState::Welcome);
      }
      break;
    case AppState::WifiConnecting:
      if (event == InputEvent::SecondaryClick) {
        wifi_.disable();
        transitionTo(AppState::Settings);
      }
      break;
    case AppState::WifiConnected:
      if (event == InputEvent::PrimaryClick) {
        wifi_.disable();
        transitionTo(AppState::Settings);
      }
      break;
    case AppState::WifiFailed:
      if (event == InputEvent::PrimaryClick) {
        if (wifi_.enable()) {
          transitionTo(AppState::WifiConnecting);
        }
      } else if (event == InputEvent::SecondaryClick) {
        transitionTo(AppState::Settings);
      }
      break;
    case AppState::WifiTimeout:
      if (event == InputEvent::PrimaryClick) {
        if (wifi_.enable()) {
          transitionTo(AppState::WifiConnecting);
        }
      } else if (event == InputEvent::SecondaryClick) {
        transitionTo(AppState::Settings);
      }
      break;
  }
}

void StateMachine::update(std::uint32_t nowMs) noexcept {
  if (state_ != AppState::WifiConnecting) {
    return;
  }
  switch (wifi_.update(nowMs)) {
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

void StateMachine::castLine() noexcept {
  if (session_.castLine()) {
    transitionTo(AppState::LineResult);
    // The state does not change when Shake advances from one result to the
    // next, but the renderer still needs to show the new line.
    dirty_ = true;
  }
}

void StateMachine::transitionTo(AppState next) noexcept {
  if (state_ == next) {
    return;
  }
  state_ = next;
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

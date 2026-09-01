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
  }
  return "UNKNOWN";
}

StateMachine::StateMachine(DivinationSession& session) noexcept
    : session_(session) {}

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

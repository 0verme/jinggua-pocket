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
  }
  return "UNKNOWN";
}

StateMachine::StateMachine(DivinationSession& session,
                           HistoryStore* history) noexcept
    : session_(session), history_(history) {}

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
        transitionTo(AppState::History);
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
    case AppState::History:
      if (event == InputEvent::PrimaryClick) {
        moveHistoryCursor(-1);  // A = 更早 (older)
      } else if (event == InputEvent::SecondaryClick) {
        moveHistoryCursor(1);  // B = 更新 (newer)
      } else if (event == InputEvent::LongPress) {
        transitionTo(AppState::Welcome);
      }
      break;
  }
}

void StateMachine::castLine() noexcept {
  if (session_.castLine()) {
    persistIfComplete();
    transitionTo(AppState::LineResult);
    // The state does not change when Shake advances from one result to the
    // next, but the renderer still needs to show the new line.
    dirty_ = true;
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
#include "jinggua/ui/renderer.h"

#include "jinggua/ui/history_screen.h"
#include "jinggua/ui/screens.h"

namespace jinggua::ui {

Renderer::Renderer(hardware::Display& display) noexcept : display_(display) {}

bool Renderer::update(const application::StateMachine& stateMachine,
                      std::uint32_t nowMs) noexcept {
  if (!stateMachine.isLineAnimationActive()) {
    if (coinAnimation_.state() != CoinAnimationState::Idle) {
      coinAnimation_.reset();
    }
    animationLinePosition_ = 0;
    return false;
  }

  const auto* line = stateMachine.session().latestLine();
  if (line == nullptr) {
    return false;
  }

  if (coinAnimation_.state() == CoinAnimationState::Idle ||
      animationLinePosition_ != line->position) {
    coinAnimation_.start(line->coins, nowMs);
    animationLinePosition_ = line->position;
    animationDirty_ = true;
  }

  const bool wasFinished = coinAnimation_.isFinished();
  if (coinAnimation_.update(nowMs)) {
    animationDirty_ = true;
  }
  return !wasFinished && coinAnimation_.isFinished();
}

void Renderer::render(const application::StateMachine& stateMachine) noexcept {
  switch (stateMachine.state()) {
    case application::AppState::Boot:
    case application::AppState::Welcome:
      renderWelcome(display_);
      break;
    case application::AppState::Prepare:
      renderPrepare(display_);
      break;
    case application::AppState::Casting:
      renderCasting(display_, stateMachine.session());
      break;
    case application::AppState::LineResult:
      renderLineResult(display_, stateMachine.session(),
                       stateMachine.isLineAnimationActive() ? &coinAnimation_
                                                             : nullptr);
      break;
    case application::AppState::HexagramResult:
      renderHexagramResult(display_, stateMachine.session());
      break;
    case application::AppState::TransformedResult:
      renderTransformedResult(display_, stateMachine.session());
      break;
    case application::AppState::ResetConfirm:
      renderResetConfirm(display_);
      break;
    case application::AppState::History:
      renderHistory(display_, stateMachine);
      break;
    case application::AppState::Settings:
      renderWifiSettings(display_, stateMachine.wifi(),
                        stateMachine.soundEnabled());
      break;
    case application::AppState::WifiConnecting:
      renderWifiConnecting(display_);
      break;
    case application::AppState::WifiConnected:
      renderWifiConnected(display_, stateMachine.wifi());
      break;
    case application::AppState::WifiFailed:
      renderWifiFailed(display_, stateMachine.wifi());
      break;
    case application::AppState::WifiTimeout:
      renderWifiTimeout(display_);
      break;
    case application::AppState::Uploading:
      renderUploading(display_);
      break;
    case application::AppState::UploadSuccess:
      renderUploadSuccess(display_);
      break;
    case application::AppState::UploadFailed:
      renderUploadFailed(display_);
      break;
  }
}

}  // namespace jinggua::ui

#include "jinggua/ui/renderer.h"

#include "jinggua/ui/screens.h"

namespace jinggua::ui {

Renderer::Renderer(hardware::Display& display) noexcept : display_(display) {}

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
      renderLineResult(display_, stateMachine.session());
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
    case application::AppState::Settings:
      renderWifiSettings(display_, stateMachine.wifi());
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
  }
}

}  // namespace jinggua::ui

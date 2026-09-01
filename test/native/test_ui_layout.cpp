#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "test_framework.h"
#include "test_support.h"

#include "jinggua/application/divination_session.h"
#include "jinggua/application/ring_history_store.h"
#include "jinggua/application/state_machine.h"
#include "jinggua/hardware/display.h"
#include "jinggua/ui/coin_animation.h"
#include "jinggua/ui/layout.h"
#include "jinggua/ui/renderer.h"
#include "jinggua/ui/screens.h"

namespace {

std::vector<jinggua::domain::CoinSide> allMovingCoins() {
  std::vector<jinggua::domain::CoinSide> sequence;
  sequence.reserve(18);
  for (std::size_t index = 0; index < 6; ++index) {
    sequence.push_back(jinggua::domain::CoinSide::Back);
    sequence.push_back(jinggua::domain::CoinSide::Back);
    sequence.push_back(jinggua::domain::CoinSide::Back);
  }
  return sequence;
}

std::vector<jinggua::domain::CoinSide> allStaticCoins() {
  std::vector<jinggua::domain::CoinSide> sequence;
  sequence.reserve(18);
  for (std::size_t index = 0; index < 6; ++index) {
    sequence.push_back(jinggua::domain::CoinSide::Front);
    sequence.push_back(jinggua::domain::CoinSide::Back);
    sequence.push_back(jinggua::domain::CoinSide::Back);
  }
  return sequence;
}

}  // namespace

void runUiLayoutTests(TestRunner& runner) {
  using jinggua::application::AppState;
  using jinggua::application::DivinationSession;
  using jinggua::application::InputEvent;
  using jinggua::application::RingHistoryStore;
  using jinggua::application::StateMachine;
  using jinggua::hardware::Display;
  using jinggua::ui::CoinAnimation;
  using jinggua::ui::Renderer;

  EXPECT_EQ(runner, jinggua::ui::layout::kScreenWidth, 135);
  EXPECT_EQ(runner, jinggua::ui::layout::kScreenHeight, 240);
  EXPECT(runner, jinggua::ui::layout::isPocketLayoutSafe());
  EXPECT_EQ(runner, jinggua::ui::layout::horizontalMargin(135), 10);
  EXPECT_EQ(runner, jinggua::ui::layout::footerY(240), 216);
  EXPECT_EQ(runner, jinggua::ui::layout::footerSecondaryY(240), 196);
  EXPECT_EQ(runner, jinggua::ui::layout::coinCenterX(135, 0), 31);
  EXPECT_EQ(runner, jinggua::ui::layout::coinCenterX(135, 1), 67);
  EXPECT_EQ(runner, jinggua::ui::layout::coinCenterX(135, 2), 103);
  EXPECT_EQ(runner, jinggua::ui::layout::hexagramLineWidth(135), 88);
  EXPECT_EQ(runner, jinggua::ui::layout::hexagramLeft(135), 13);
  EXPECT_EQ(runner, jinggua::ui::layout::kHexagramLastY, 191);
  EXPECT_EQ(runner, jinggua::ui::layout::kHexagramMarkerLastY, 200);
  EXPECT_EQ(runner, jinggua::ui::typography::kPrimaryLineHeight, 32);
  EXPECT_EQ(runner, jinggua::ui::typography::kSecondaryLineHeight, 16);

  Display display;
  EXPECT(runner, display.begin());
  EXPECT_EQ(runner, display.width(), 135);
  EXPECT_EQ(runner, display.height(), 240);

  // Renderer coverage for the complete divination path, including the
  // active and finished CoinAnimation presentation states.
  SequenceRandomProvider random(allMovingCoins());
  DivinationSession session(random);
  FakeWifiController wifi;
  StateMachine stateMachine(session, wifi);
  Renderer renderer(display);

  renderer.render(stateMachine);  // Boot
  stateMachine.begin();
  renderer.render(stateMachine);  // Welcome
  stateMachine.handleInput(InputEvent::PrimaryClick);
  renderer.render(stateMachine);  // Prepare
  stateMachine.handleInput(InputEvent::PrimaryClick);
  renderer.render(stateMachine);  // Casting

  std::uint32_t nowMs = 1000;
  for (std::size_t index = 0; index < 6; ++index) {
    stateMachine.handleInput(InputEvent::PrimaryClick);
    EXPECT_EQ(runner, stateMachine.state(), AppState::LineResult);
    EXPECT(runner, stateMachine.isLineAnimationActive());
    renderer.update(stateMachine, nowMs);
    renderer.render(stateMachine);  // CoinAnimation active

    EXPECT(runner, renderer.update(
                       stateMachine, nowMs + CoinAnimation::kDurationMs));
    stateMachine.finishLineAnimation();
    renderer.render(stateMachine);  // CoinAnimation finished/result

    if (index < 5) {
      stateMachine.handleInput(InputEvent::PrimaryClick);
      renderer.render(stateMachine);  // next Casting
    }
    nowMs += CoinAnimation::kDurationMs + 20;
  }

  stateMachine.handleInput(InputEvent::PrimaryClick);
  EXPECT_EQ(runner, stateMachine.state(), AppState::HexagramResult);
  renderer.render(stateMachine);
  stateMachine.handleInput(InputEvent::PrimaryClick);
  EXPECT_EQ(runner, stateMachine.state(), AppState::TransformedResult);
  renderer.render(stateMachine);
  stateMachine.handleInput(InputEvent::PrimaryClick);
  EXPECT_EQ(runner, stateMachine.state(), AppState::ResetConfirm);
  renderer.render(stateMachine);

  // A no-moving result never needs an empty transformed screen.  The direct
  // render call covers the defensive fallback used by stale callers.
  SequenceRandomProvider staticRandom(allStaticCoins());
  DivinationSession staticSession(staticRandom);
  for (std::size_t index = 0; index < 6; ++index) {
    EXPECT(runner, staticSession.castLine());
  }
  jinggua::ui::renderTransformedResult(display, staticSession);

  // Renderer coverage for empty/non-empty History and every Wi-Fi state.
  SequenceRandomProvider historyRandom(allMovingCoins());
  DivinationSession historySession(historyRandom);
  InMemorySlotStorage storage(4);
  RingHistoryStore history(storage);
  EXPECT(runner, history.begin());
  StateMachine historyStateMachine(historySession, wifi, &history);
  Renderer historyRenderer(display);
  historyStateMachine.begin();
  historyStateMachine.handleInput(InputEvent::LongPress);
  EXPECT_EQ(runner, historyStateMachine.state(), AppState::History);
  historyRenderer.render(historyStateMachine);  // empty History

  jinggua::domain::HistoryRecord record;
  record.originalNumber = 1;
  record.movingMask = 0x02;
  record.changedNumber = 2;
  record.lineTotals = {6, 7, 7, 7, 7, 7};
  EXPECT(runner, history.add(record));
  historyRenderer.render(historyStateMachine);  // changed History

  jinggua::domain::HistoryRecord staticRecord;
  staticRecord.originalNumber = 3;
  staticRecord.lineTotals = {7, 8, 7, 8, 7, 8};
  EXPECT(runner, history.add(staticRecord));
  historyStateMachine.handleInput(InputEvent::SecondaryClick);
  historyRenderer.render(historyStateMachine);  // no-moving History

  historyStateMachine.handleInput(InputEvent::LongPress);
  EXPECT_EQ(runner, historyStateMachine.state(), AppState::Welcome);
  historyStateMachine.handleInput(InputEvent::SecondaryClick);
  EXPECT_EQ(runner, historyStateMachine.state(), AppState::Settings);
  historyRenderer.render(historyStateMachine);

  historyStateMachine.handleInput(InputEvent::PrimaryClick);
  EXPECT_EQ(runner, historyStateMachine.state(), AppState::WifiConnecting);
  historyRenderer.render(historyStateMachine);

  wifi.setState(jinggua::application::WifiState::Connected);
  historyStateMachine.update(1);
  EXPECT_EQ(runner, historyStateMachine.state(), AppState::WifiConnected);
  historyRenderer.render(historyStateMachine);

  historyStateMachine.handleInput(InputEvent::PrimaryClick);
  EXPECT_EQ(runner, historyStateMachine.state(), AppState::Settings);
  historyRenderer.render(historyStateMachine);
  historyStateMachine.handleInput(InputEvent::PrimaryClick);
  wifi.setState(jinggua::application::WifiState::Failed);
  historyStateMachine.update(2);
  EXPECT_EQ(runner, historyStateMachine.state(), AppState::WifiFailed);
  historyRenderer.render(historyStateMachine);

  historyStateMachine.handleInput(InputEvent::PrimaryClick);
  wifi.setState(jinggua::application::WifiState::Timeout);
  historyStateMachine.update(3);
  EXPECT_EQ(runner, historyStateMachine.state(), AppState::WifiTimeout);
  historyRenderer.render(historyStateMachine);
}

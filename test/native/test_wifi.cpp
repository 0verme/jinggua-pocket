#include "test_framework.h"
#include "test_support.h"

#include "jinggua/application/state_machine.h"
#include "jinggua/application/wifi_controller.h"

namespace {

using jinggua::application::AppState;
using jinggua::application::DivinationSession;
using jinggua::application::InputEvent;
using jinggua::application::StateMachine;
using jinggua::application::WifiState;

void assertAtWelcome(TestRunner& runner, const StateMachine& stateMachine) {
  EXPECT_EQ(runner, stateMachine.state(), AppState::Welcome);
  EXPECT_EQ(runner, stateMachine.wifi().state(), WifiState::Off);
}

}  // namespace

void runWifiTests(TestRunner& runner) {
  // ---- A. Offline-first: boot and a full divination never touch Wi-Fi ----
  {
    std::vector<jinggua::domain::CoinSide> sequence(18,
                                                    jinggua::domain::CoinSide::Back);
    SequenceRandomProvider random(std::move(sequence));
    DivinationSession session(random);
    FakeWifiController wifi;
    StateMachine stateMachine(session, wifi);
    stateMachine.begin();
    assertAtWelcome(runner, stateMachine);
    EXPECT_EQ(runner, wifi.enableCount(), 0);

    stateMachine.handleInput(InputEvent::PrimaryClick);  // -> Prepare
    stateMachine.handleInput(InputEvent::PrimaryClick);  // -> Casting
    for (std::size_t index = 0; index < 6; ++index) {
      stateMachine.handleInput(InputEvent::PrimaryClick);  // cast line
      stateMachine.finishLineAnimation();
      stateMachine.handleInput(InputEvent::PrimaryClick);  // advance
    }
    EXPECT(runner, session.isComplete());
    EXPECT_EQ(runner, wifi.enableCount(), 0);
    EXPECT_EQ(runner, wifi.disableCount(), 0);
    EXPECT_EQ(runner, wifi.state(), WifiState::Off);
  }

  // ---- B. User-triggered flow: OFF -> CONNECTING -> CONNECTED ----
  {
    std::vector<jinggua::domain::CoinSide> sequence(18,
                                                    jinggua::domain::CoinSide::Back);
    SequenceRandomProvider random(std::move(sequence));
    DivinationSession session(random);
    FakeWifiController wifi;
    StateMachine stateMachine(session, wifi);
    stateMachine.begin();

    stateMachine.handleInput(InputEvent::SecondaryClick);  // Welcome -> Settings
    EXPECT_EQ(runner, stateMachine.state(), AppState::Settings);
    stateMachine.handleInput(InputEvent::PrimaryClick);  // Settings -> connect
    EXPECT_EQ(runner, stateMachine.state(), AppState::WifiConnecting);
    EXPECT(runner, !stateMachine.sleepAllowed());
    EXPECT_EQ(runner, wifi.state(), WifiState::Connecting);
    EXPECT_EQ(runner, wifi.enableCount(), 1);

    wifi.setState(WifiState::Connected);
    stateMachine.update(1000);
    EXPECT_EQ(runner, stateMachine.state(), AppState::WifiConnected);
    EXPECT(runner, !stateMachine.sleepAllowed());
    EXPECT_EQ(runner, wifi.state(), WifiState::Connected);
  }

  // ---- C. Wrong credential / unavailable AP: CONNECTING -> FAILED ----
  //     Local divination still works afterwards.
  {
    std::vector<jinggua::domain::CoinSide> sequence(18,
                                                    jinggua::domain::CoinSide::Back);
    SequenceRandomProvider random(std::move(sequence));
    DivinationSession session(random);
    FakeWifiController wifi;
    StateMachine stateMachine(session, wifi);
    stateMachine.begin();

    stateMachine.handleInput(InputEvent::SecondaryClick);  // -> Settings
    stateMachine.handleInput(InputEvent::PrimaryClick);    // -> Connecting
    wifi.setState(WifiState::Failed);
    stateMachine.update(1000);
    EXPECT_EQ(runner, stateMachine.state(), AppState::WifiFailed);

    // Go back and complete a divination without any Wi-Fi interaction.
    stateMachine.handleInput(InputEvent::SecondaryClick);  // Failed -> Settings
    EXPECT_EQ(runner, stateMachine.state(), AppState::Settings);
    stateMachine.handleInput(InputEvent::SecondaryClick);  // Settings -> Welcome
    stateMachine.handleInput(InputEvent::PrimaryClick);    // -> Prepare
    stateMachine.handleInput(InputEvent::PrimaryClick);    // -> Casting
    for (std::size_t index = 0; index < 6; ++index) {
      stateMachine.handleInput(InputEvent::PrimaryClick);
      stateMachine.finishLineAnimation();
      stateMachine.handleInput(InputEvent::PrimaryClick);
    }
    EXPECT(runner, session.isComplete());
    EXPECT_EQ(runner, wifi.state(), WifiState::Failed);
  }

  // ---- D. Explicit close: CONNECTED -> OFF ----
  {
    std::vector<jinggua::domain::CoinSide> sequence(18,
                                                    jinggua::domain::CoinSide::Back);
    SequenceRandomProvider random(std::move(sequence));
    DivinationSession session(random);
    FakeWifiController wifi;
    StateMachine stateMachine(session, wifi);
    stateMachine.begin();

    stateMachine.handleInput(InputEvent::SecondaryClick);  // -> Settings
    stateMachine.handleInput(InputEvent::PrimaryClick);    // -> Connecting
    wifi.setState(WifiState::Connected);
    stateMachine.update(0);
    EXPECT_EQ(runner, stateMachine.state(), AppState::WifiConnected);

    stateMachine.handleInput(InputEvent::PrimaryClick);  // user disables
    EXPECT_EQ(runner, stateMachine.state(), AppState::Settings);
    EXPECT_EQ(runner, wifi.state(), WifiState::Off);
    EXPECT_EQ(runner, wifi.disableCount(), 1);
  }

  // ---- E. Timeout: CONNECTING -> TIMEOUT, retry reconnects ----
  {
    std::vector<jinggua::domain::CoinSide> sequence(18,
                                                    jinggua::domain::CoinSide::Back);
    SequenceRandomProvider random(std::move(sequence));
    DivinationSession session(random);
    FakeWifiController wifi;
    StateMachine stateMachine(session, wifi);
    stateMachine.begin();

    stateMachine.handleInput(InputEvent::SecondaryClick);
    stateMachine.handleInput(InputEvent::PrimaryClick);  // -> Connecting
    wifi.setState(WifiState::Timeout);
    stateMachine.update(20000);
    EXPECT_EQ(runner, stateMachine.state(), AppState::WifiTimeout);

    stateMachine.handleInput(InputEvent::PrimaryClick);  // retry
    EXPECT_EQ(runner, stateMachine.state(), AppState::WifiConnecting);
    EXPECT_EQ(runner, wifi.state(), WifiState::Connecting);
    EXPECT_EQ(runner, wifi.enableCount(), 2);
  }

  // ---- E2. Connecting cancelled by user -> OFF, back to Settings ----
  {
    std::vector<jinggua::domain::CoinSide> sequence(18,
                                                    jinggua::domain::CoinSide::Back);
    SequenceRandomProvider random(std::move(sequence));
    DivinationSession session(random);
    FakeWifiController wifi;
    StateMachine stateMachine(session, wifi);
    stateMachine.begin();

    stateMachine.handleInput(InputEvent::SecondaryClick);  // -> Settings
    stateMachine.handleInput(InputEvent::PrimaryClick);    // -> Connecting
    stateMachine.handleInput(InputEvent::SecondaryClick);  // cancel
    EXPECT_EQ(runner, stateMachine.state(), AppState::Settings);
    EXPECT_EQ(runner, wifi.state(), WifiState::Off);
    EXPECT_EQ(runner, wifi.disableCount(), 1);
  }

  // ---- F. Unconfigured controller: Settings connect goes straight to Failed ----
  {
    std::vector<jinggua::domain::CoinSide> sequence(18,
                                                    jinggua::domain::CoinSide::Back);
    SequenceRandomProvider random(std::move(sequence));
    DivinationSession session(random);
    UnconfiguredFakeWifiController wifi;
    StateMachine stateMachine(session, wifi);
    stateMachine.begin();

    stateMachine.handleInput(InputEvent::SecondaryClick);  // -> Settings
    stateMachine.handleInput(InputEvent::PrimaryClick);    // enable() refused
    EXPECT_EQ(runner, stateMachine.state(), AppState::WifiFailed);
    EXPECT_EQ(runner, wifi.state(), WifiState::Failed);
  }

  // ---- G. Full flow stays offline: Wi-Fi state never affects divination ----
  {
    std::vector<jinggua::domain::CoinSide> sequence(18,
                                                    jinggua::domain::CoinSide::Back);
    SequenceRandomProvider random(std::move(sequence));
    DivinationSession session(random);
    FakeWifiController wifi;
    StateMachine stateMachine(session, wifi);
    stateMachine.begin();

    // Interleave Wi-Fi navigation and divination freely.
    stateMachine.handleInput(InputEvent::SecondaryClick);  // -> Settings
    stateMachine.handleInput(InputEvent::SecondaryClick);  // -> Welcome
    stateMachine.handleInput(InputEvent::PrimaryClick);    // -> Prepare
    stateMachine.handleInput(InputEvent::PrimaryClick);    // -> Casting
    wifi.setState(WifiState::Connected);                   // Wi-Fi "on" meanwhile
    for (std::size_t index = 0; index < 6; ++index) {
      stateMachine.handleInput(InputEvent::PrimaryClick);  // cast line
      stateMachine.finishLineAnimation();
      stateMachine.handleInput(InputEvent::PrimaryClick);  // advance
    }
    EXPECT(runner, session.isComplete());
    EXPECT_EQ(runner, stateMachine.state(), AppState::HexagramResult);
    // Divination remains functional regardless of Wi-Fi state.
    EXPECT_EQ(runner, wifi.state(), WifiState::Connected);
  }
}

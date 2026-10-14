#include "test_framework.h"
#include "test_support.h"

#include "jinggua/application/state_machine.h"
#include "jinggua/application/ring_history_store.h"

namespace {

std::vector<jinggua::domain::CoinSide> sixLineSequence() {
  using jinggua::domain::CoinSide;
  std::vector<CoinSide> sequence;
  sequence.reserve(18);
  for (std::size_t index = 0; index < 6; ++index) {
    sequence.push_back(CoinSide::Front);
    sequence.push_back(CoinSide::Back);
    sequence.push_back(CoinSide::Back);
  }
  return sequence;
}

bool sameLine(const jinggua::domain::Yao& left,
              const jinggua::domain::Yao& right) noexcept {
  if (left.position != right.position || left.type != right.type ||
      left.yinYang != right.yinYang || left.moving != right.moving ||
      left.transformedYinYang != right.transformedYinYang ||
      left.coins.total != right.coins.total) {
    return false;
  }
  for (std::size_t index = 0; index < left.coins.coins.size(); ++index) {
    if (left.coins.coins[index] != right.coins.coins[index]) {
      return false;
    }
  }
  return true;
}

void enterCasting(jinggua::application::StateMachine& stateMachine) {
  using jinggua::application::InputEvent;
  stateMachine.begin();
  stateMachine.handleInput(InputEvent::PrimaryClick);
  stateMachine.handleInput(InputEvent::PrimaryClick);
}

}  // namespace

void runStateMachineTests(TestRunner& runner) {
  std::vector<jinggua::domain::CoinSide> sequence;
  sequence.reserve(18);
  for (std::size_t index = 0; index < 6; ++index) {
    sequence.push_back(jinggua::domain::CoinSide::Front);
    sequence.push_back(jinggua::domain::CoinSide::Back);
    sequence.push_back(jinggua::domain::CoinSide::Back);
  }
  SequenceRandomProvider random(std::move(sequence));
  FakeWifiController wifi;
  jinggua::application::DivinationSession session(random);
  jinggua::application::StateMachine stateMachine(session, wifi);

  EXPECT_EQ(runner, stateMachine.state(),
            jinggua::application::AppState::Boot);
  stateMachine.begin();
  EXPECT_EQ(runner, stateMachine.state(),
            jinggua::application::AppState::Welcome);

  stateMachine.handleInput(jinggua::application::InputEvent::PrimaryClick);
  EXPECT_EQ(runner, stateMachine.state(),
            jinggua::application::AppState::Prepare);
  stateMachine.handleInput(jinggua::application::InputEvent::PrimaryClick);
  EXPECT_EQ(runner, stateMachine.state(),
            jinggua::application::AppState::Casting);

  for (std::size_t index = 0; index < 6; ++index) {
    stateMachine.handleInput(jinggua::application::InputEvent::PrimaryClick);
    EXPECT_EQ(runner, stateMachine.state(),
              jinggua::application::AppState::LineResult);
    EXPECT(runner, stateMachine.isLineAnimationActive());
    EXPECT_EQ(runner, session.lineCount(), index + 1);
    EXPECT(runner, !stateMachine.sleepAllowed());

    // Input arriving during the presentation cannot create or acknowledge a
    // second line.
    stateMachine.handleInput(jinggua::application::InputEvent::PrimaryClick);
    EXPECT_EQ(runner, session.lineCount(), index + 1);
    EXPECT(runner, stateMachine.isLineAnimationActive());

    stateMachine.finishLineAnimation();
    EXPECT(runner, !stateMachine.isLineAnimationActive());
    EXPECT(runner, stateMachine.sleepAllowed());
    stateMachine.handleInput(jinggua::application::InputEvent::PrimaryClick);
    if (index < 5) {
      EXPECT_EQ(runner, stateMachine.state(),
                jinggua::application::AppState::Casting);
    } else {
      EXPECT_EQ(runner, stateMachine.state(),
                jinggua::application::AppState::HexagramResult);
    }
  }

  EXPECT(runner, session.result().has_value());
  EXPECT(runner, !session.result()->hasMovingLines());
  stateMachine.handleInput(jinggua::application::InputEvent::PrimaryClick);
  EXPECT_EQ(runner, stateMachine.state(),
            jinggua::application::AppState::ResetConfirm);
  EXPECT(runner, !stateMachine.sleepAllowed());
  stateMachine.handleInput(jinggua::application::InputEvent::SecondaryClick);
  EXPECT_EQ(runner, stateMachine.state(),
            jinggua::application::AppState::HexagramResult);
  stateMachine.handleInput(jinggua::application::InputEvent::PrimaryClick);
  stateMachine.handleInput(jinggua::application::InputEvent::PrimaryClick);
  EXPECT_EQ(runner, stateMachine.state(),
            jinggua::application::AppState::Prepare);
  EXPECT_EQ(runner, session.lineCount(), static_cast<std::size_t>(0));

  // Shake input can advance directly from one line result to the next line
  // result, so six gestures complete a session without button acknowledgements.
  std::vector<jinggua::domain::CoinSide> shakeSequence;
  shakeSequence.reserve(18);
  for (std::size_t index = 0; index < 6; ++index) {
    shakeSequence.push_back(jinggua::domain::CoinSide::Front);
    shakeSequence.push_back(jinggua::domain::CoinSide::Back);
    shakeSequence.push_back(jinggua::domain::CoinSide::Back);
  }
  SequenceRandomProvider shakeRandom(std::move(shakeSequence));
  FakeWifiController shakeWifi;
  jinggua::application::DivinationSession shakeSession(shakeRandom);
  jinggua::application::StateMachine shakeStateMachine(shakeSession, shakeWifi);
  shakeStateMachine.begin();
  shakeStateMachine.handleInput(jinggua::application::InputEvent::PrimaryClick);
  shakeStateMachine.handleInput(jinggua::application::InputEvent::PrimaryClick);

  for (std::size_t index = 0; index < 6; ++index) {
    shakeStateMachine.handleInput(jinggua::application::InputEvent::Shake);
    EXPECT_EQ(runner, shakeSession.lineCount(), index + 1);
    EXPECT_EQ(runner, shakeStateMachine.state(),
              jinggua::application::AppState::LineResult);
    EXPECT(runner, shakeStateMachine.isLineAnimationActive());

    shakeStateMachine.handleInput(jinggua::application::InputEvent::Shake);
    EXPECT_EQ(runner, shakeSession.lineCount(), index + 1);
    shakeStateMachine.finishLineAnimation();
    EXPECT(runner, !shakeStateMachine.isLineAnimationActive());
  }
  EXPECT(runner, shakeSession.isComplete());
  EXPECT_EQ(runner, shakeRandom.consumed(), static_cast<std::size_t>(18));

  // A seventh gesture after completion cannot add another line.
  shakeStateMachine.handleInput(jinggua::application::InputEvent::Shake);
  EXPECT_EQ(runner, shakeSession.lineCount(), static_cast<std::size_t>(6));

  // Long-press A completes any partial session without replacing existing
  // lines. Each subsequent line waits for the previous animation and result
  // presentation, including across uint32_t millis() rollover.
  constexpr std::array<std::size_t, 6> kStartingProgress{0, 1, 2, 3, 4, 5};
  for (const auto startingLines : kStartingProgress) {
    SequenceRandomProvider autoRandom(sixLineSequence());
    jinggua::application::DivinationSession autoSession(autoRandom);
    FakeWifiController autoWifi;
    InMemorySlotStorage storage(4);
    jinggua::application::RingHistoryStore history(storage);
    EXPECT(runner, history.begin());
    FakeAudioController audio;
    jinggua::application::StateMachine autoStateMachine(autoSession, autoWifi,
                                                         &history);
    autoStateMachine.setAudioController(audio);
    enterCasting(autoStateMachine);

    for (std::size_t index = 0; index < startingLines; ++index) {
      autoStateMachine.handleInput(
          jinggua::application::InputEvent::PrimaryClick);
      autoStateMachine.finishLineAnimation();
      if (index + 1 < startingLines) {
        autoStateMachine.handleInput(
            jinggua::application::InputEvent::PrimaryClick);
      }
    }
    const auto existingLines = autoSession.lines();
    if (startingLines == 0) {
      // The B/legacy LongPress keeps its existing behavior and cannot start
      // auto-completion while the device is in Casting.
      autoStateMachine.handleInput(
          jinggua::application::InputEvent::LongPress);
      EXPECT_EQ(runner, autoSession.lineCount(), static_cast<std::size_t>(0));
      EXPECT(runner, !autoStateMachine.isAutoCompleting());
    }

    autoStateMachine.handleInput(
        jinggua::application::InputEvent::PrimaryLongPress);
    EXPECT(runner, autoStateMachine.isAutoCompleting());
    EXPECT(runner, !autoStateMachine.sleepAllowed());
    EXPECT(runner, autoStateMachine.isLineAnimationActive());
    EXPECT_EQ(runner, autoSession.lineCount(), startingLines + 1);
    for (std::size_t index = 0; index < startingLines; ++index) {
      EXPECT(runner, sameLine(autoSession.lines()[index], existingLines[index]));
    }

    std::uint32_t nowMs = UINT32_MAX - 100U;
    while (autoSession.lineCount() <
           jinggua::application::DivinationSession::kLineCount) {
      const auto lineCountBefore = autoSession.lineCount();
      autoStateMachine.handleInput(
          jinggua::application::InputEvent::PrimaryClick);
      autoStateMachine.handleInput(
          jinggua::application::InputEvent::SecondaryClick);
      autoStateMachine.handleInput(jinggua::application::InputEvent::Shake);
      autoStateMachine.handleInput(
          jinggua::application::InputEvent::PrimaryLongPress);
      autoStateMachine.update(nowMs);
      EXPECT_EQ(runner, autoSession.lineCount(), lineCountBefore);
      EXPECT(runner, autoStateMachine.isLineAnimationActive());

      autoStateMachine.finishLineAnimation();
      EXPECT(runner, !autoStateMachine.isLineAnimationActive());
      EXPECT(runner, !autoStateMachine.sleepAllowed());
      autoStateMachine.update(nowMs);  // begin displaying the settled result
      autoStateMachine.handleInput(jinggua::application::InputEvent::Shake);
      autoStateMachine.update(
          nowMs +
          jinggua::application::StateMachine::kAutoLineResultDisplayMs - 1U);
      EXPECT_EQ(runner, autoSession.lineCount(), lineCountBefore);
      autoStateMachine.update(
          nowMs +
          jinggua::application::StateMachine::kAutoLineResultDisplayMs);
      EXPECT_EQ(runner, autoSession.lineCount(), lineCountBefore + 1);
      EXPECT(runner, autoStateMachine.isLineAnimationActive());
      for (std::size_t index = 0; index < startingLines; ++index) {
        EXPECT(runner,
               sameLine(autoSession.lines()[index], existingLines[index]));
      }
      nowMs += jinggua::application::StateMachine::kAutoLineResultDisplayMs;
    }

    // The sixth coin animation finishes before the app leaves LineResult.
    EXPECT(runner, autoStateMachine.isLineAnimationActive());
    autoStateMachine.finishLineAnimation();
    autoStateMachine.update(nowMs);
    EXPECT_EQ(runner, autoStateMachine.state(),
              jinggua::application::AppState::HexagramResult);
    EXPECT(runner, !autoStateMachine.isAutoCompleting());
    EXPECT(runner, autoStateMachine.sleepAllowed());
    EXPECT(runner, autoSession.result().has_value());
    EXPECT_EQ(runner, autoSession.lineCount(), static_cast<std::size_t>(6));
    EXPECT_EQ(runner, autoRandom.consumed(), static_cast<std::size_t>(18));
    EXPECT_EQ(runner, history.count(), static_cast<std::size_t>(1));
    EXPECT_EQ(runner, audio.count(jinggua::application::SoundCue::Start),
              static_cast<std::size_t>(1));
    EXPECT_EQ(runner, audio.count(jinggua::application::SoundCue::Cast),
              static_cast<std::size_t>(5));
    EXPECT_EQ(runner, audio.count(jinggua::application::SoundCue::Complete),
              static_cast<std::size_t>(1));
    for (std::size_t index = 0; index < 6; ++index) {
      EXPECT_EQ(runner, autoSession.lines()[index].position,
                static_cast<std::uint8_t>(index + 1));
    }
    autoStateMachine.handleInput(
        jinggua::application::InputEvent::PrimaryLongPress);
    autoStateMachine.handleInput(jinggua::application::InputEvent::Shake);
    EXPECT_EQ(runner, autoSession.lineCount(), static_cast<std::size_t>(6));
    EXPECT_EQ(runner, history.count(), static_cast<std::size_t>(1));
  }

  // A long press received during an existing animation is latched once, but
  // cannot change its coin target until the animation is finished.
  {
    SequenceRandomProvider autoRandom(sixLineSequence());
    jinggua::application::DivinationSession autoSession(autoRandom);
    jinggua::application::StateMachine autoStateMachine(autoSession);
    enterCasting(autoStateMachine);
    autoStateMachine.handleInput(
        jinggua::application::InputEvent::PrimaryClick);
    autoStateMachine.handleInput(
        jinggua::application::InputEvent::PrimaryLongPress);
    EXPECT(runner, autoStateMachine.isAutoCompleting());
    EXPECT_EQ(runner, autoSession.lineCount(), static_cast<std::size_t>(1));
    autoStateMachine.handleInput(jinggua::application::InputEvent::Shake);
    EXPECT_EQ(runner, autoSession.lineCount(), static_cast<std::size_t>(1));
    autoStateMachine.finishLineAnimation();
    constexpr std::uint32_t kStartMs = 500U;
    autoStateMachine.update(kStartMs);
    autoStateMachine.update(
        kStartMs +
        jinggua::application::StateMachine::kAutoLineResultDisplayMs);
    EXPECT_EQ(runner, autoSession.lineCount(), static_cast<std::size_t>(2));
    EXPECT(runner, autoStateMachine.isLineAnimationActive());
  }

  // A's new distinct long-press event remains compatible with legacy long
  // press actions on non-casting screens.
  {
    SequenceRandomProvider compatRandom(sixLineSequence());
    jinggua::application::DivinationSession compatSession(compatRandom);
    FakeWifiController compatWifi;
    FakeAudioController compatAudio;
    jinggua::application::StateMachine compatStateMachine(compatSession,
                                                           compatWifi);
    compatStateMachine.setAudioController(compatAudio);
    compatStateMachine.begin();
    compatStateMachine.handleInput(
        jinggua::application::InputEvent::PrimaryLongPress);
    EXPECT_EQ(runner, compatStateMachine.state(),
              jinggua::application::AppState::History);
    compatStateMachine.handleInput(
        jinggua::application::InputEvent::PrimaryLongPress);
    EXPECT_EQ(runner, compatStateMachine.state(),
              jinggua::application::AppState::Welcome);
    compatStateMachine.handleInput(
        jinggua::application::InputEvent::SecondaryClick);
    EXPECT_EQ(runner, compatStateMachine.state(),
              jinggua::application::AppState::Settings);
    compatStateMachine.handleInput(
        jinggua::application::InputEvent::PrimaryLongPress);
    EXPECT(runner, !compatStateMachine.soundEnabled());
    compatWifi.setState(jinggua::application::WifiState::Connected);
    compatStateMachine.handleInput(
        jinggua::application::InputEvent::PrimaryClick);
    EXPECT_EQ(runner, compatStateMachine.state(),
              jinggua::application::AppState::WifiConnected);
    compatStateMachine.handleInput(
        jinggua::application::InputEvent::PrimaryLongPress);
    EXPECT_EQ(runner, compatStateMachine.state(),
              jinggua::application::AppState::OtaPrompt);

    compatStateMachine.handleInput(
        jinggua::application::InputEvent::SecondaryClick);
    compatStateMachine.handleInput(
        jinggua::application::InputEvent::PrimaryClick);
    compatStateMachine.handleInput(
        jinggua::application::InputEvent::PrimaryClick);
    for (std::size_t index = 0; index < 6; ++index) {
      compatStateMachine.handleInput(
          jinggua::application::InputEvent::PrimaryClick);
      compatStateMachine.finishLineAnimation();
      if (index < 5) {
        compatStateMachine.handleInput(
            jinggua::application::InputEvent::PrimaryClick);
      }
    }
    compatStateMachine.handleInput(
        jinggua::application::InputEvent::PrimaryClick);
    EXPECT_EQ(runner, compatStateMachine.state(),
              jinggua::application::AppState::HexagramResult);
    compatStateMachine.handleInput(
        jinggua::application::InputEvent::PrimaryClick);
    EXPECT_EQ(runner, compatStateMachine.state(),
              jinggua::application::AppState::ResetConfirm);
    compatStateMachine.handleInput(
        jinggua::application::InputEvent::PrimaryLongPress);
    EXPECT_EQ(runner, compatStateMachine.state(),
              jinggua::application::AppState::Prepare);
    EXPECT_EQ(runner, compatSession.lineCount(), static_cast<std::size_t>(0));
  }
}

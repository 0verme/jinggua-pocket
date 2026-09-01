#include <cstddef>
#include <utility>
#include <vector>

#include "test_framework.h"
#include "test_support.h"

#include "jinggua/application/state_machine.h"

namespace {

using jinggua::application::AppState;
using jinggua::application::DivinationSession;
using jinggua::application::InputEvent;
using jinggua::application::SoundCue;
using jinggua::application::StateMachine;

}  // namespace

void runAudioFeedbackTests(TestRunner& runner) {
  // A. Starting a fresh session emits exactly one Start cue.
  {
    SequenceRandomProvider random(
        std::vector<jinggua::domain::CoinSide>(18,
                                               jinggua::domain::CoinSide::Back));
    FakeWifiController wifi;
    FakeAudioController audio;
    DivinationSession session(random);
    StateMachine stateMachine(session, wifi);
    stateMachine.setAudioController(audio);
    stateMachine.begin();
    stateMachine.handleInput(InputEvent::PrimaryClick);  // -> Prepare
    stateMachine.handleInput(InputEvent::PrimaryClick);  // -> Casting

    EXPECT_EQ(runner, audio.count(SoundCue::Start), static_cast<std::size_t>(1));
    EXPECT_EQ(runner, audio.count(SoundCue::Cast), static_cast<std::size_t>(0));
    EXPECT_EQ(runner, audio.count(SoundCue::Complete),
              static_cast<std::size_t>(0));
  }

  // B. One accepted line emits one Cast cue and starts the animation.
  {
    SequenceRandomProvider random(
        std::vector<jinggua::domain::CoinSide>(18,
                                               jinggua::domain::CoinSide::Back));
    FakeWifiController wifi;
    FakeAudioController audio;
    DivinationSession session(random);
    StateMachine stateMachine(session, wifi);
    stateMachine.setAudioController(audio);
    stateMachine.begin();
    stateMachine.handleInput(InputEvent::PrimaryClick);
    stateMachine.handleInput(InputEvent::PrimaryClick);
    stateMachine.handleInput(InputEvent::PrimaryClick);

    EXPECT_EQ(runner, audio.count(SoundCue::Start), static_cast<std::size_t>(1));
    EXPECT_EQ(runner, audio.count(SoundCue::Cast), static_cast<std::size_t>(1));
    EXPECT_EQ(runner, audio.count(SoundCue::Complete),
              static_cast<std::size_t>(0));
    EXPECT(runner, stateMachine.isLineAnimationActive());
  }

  // C. The sixth line replaces Cast with one Complete cue; the two cues are
  // never emitted for the same gesture.
  {
    SequenceRandomProvider random(
        std::vector<jinggua::domain::CoinSide>(18,
                                               jinggua::domain::CoinSide::Back));
    FakeWifiController wifi;
    FakeAudioController audio;
    DivinationSession session(random);
    StateMachine stateMachine(session, wifi);
    stateMachine.setAudioController(audio);
    stateMachine.begin();
    stateMachine.handleInput(InputEvent::PrimaryClick);
    stateMachine.handleInput(InputEvent::PrimaryClick);

    for (std::size_t index = 0; index < 6; ++index) {
      stateMachine.handleInput(InputEvent::PrimaryClick);
      stateMachine.finishLineAnimation();
      if (index < 5) {
        stateMachine.handleInput(InputEvent::PrimaryClick);
      }
    }

    EXPECT(runner, session.isComplete());
    EXPECT_EQ(runner, audio.count(SoundCue::Start), static_cast<std::size_t>(1));
    EXPECT_EQ(runner, audio.count(SoundCue::Cast), static_cast<std::size_t>(5));
    EXPECT_EQ(runner, audio.count(SoundCue::Complete),
              static_cast<std::size_t>(1));
    EXPECT_EQ(runner, audio.playbackCount(), static_cast<std::size_t>(7));
    EXPECT_EQ(runner, stateMachine.state(), AppState::LineResult);
  }

  // D. Mute suppresses every hardware playback and remains a runtime toggle.
  {
    SequenceRandomProvider random(
        std::vector<jinggua::domain::CoinSide>(18,
                                               jinggua::domain::CoinSide::Back));
    FakeWifiController wifi;
    FakeAudioController audio;
    DivinationSession session(random);
    StateMachine stateMachine(session, wifi);
    stateMachine.setAudioController(audio);
    stateMachine.setSoundEnabled(false);
    stateMachine.begin();
    stateMachine.handleInput(InputEvent::PrimaryClick);
    stateMachine.handleInput(InputEvent::PrimaryClick);
    stateMachine.handleInput(InputEvent::PrimaryClick);

    EXPECT(runner, !stateMachine.soundEnabled());
    EXPECT(runner, !audio.enabled());
    EXPECT_EQ(runner, audio.playbackCount(), static_cast<std::size_t>(0));

    stateMachine.setSoundEnabled(true);
    EXPECT(runner, stateMachine.soundEnabled());
    EXPECT(runner, audio.enabled());
  }

  // E. Invalid button input is silent; a failed Wi-Fi attempt emits one Error
  // cue and repeated input on the same failure screen does not repeat it.
  {
    SequenceRandomProvider random(
        std::vector<jinggua::domain::CoinSide>(18,
                                               jinggua::domain::CoinSide::Back));
    FakeWifiController wifi;
    FakeAudioController audio;
    DivinationSession session(random);
    StateMachine stateMachine(session, wifi);
    stateMachine.setAudioController(audio);
    stateMachine.begin();
    stateMachine.handleInput(InputEvent::PrimaryClick);
    stateMachine.handleInput(InputEvent::PrimaryClick);
    stateMachine.handleInput(InputEvent::PrimaryClick);
    for (std::size_t index = 0; index < 10; ++index) {
      stateMachine.handleInput(InputEvent::SecondaryClick);
    }
    EXPECT_EQ(runner, audio.count(SoundCue::Error), static_cast<std::size_t>(0));
  }

  {
    SequenceRandomProvider random(
        std::vector<jinggua::domain::CoinSide>(18,
                                               jinggua::domain::CoinSide::Back));
    FakeWifiController wifi;
    FakeAudioController audio;
    DivinationSession session(random);
    StateMachine stateMachine(session, wifi);
    stateMachine.setAudioController(audio);
    stateMachine.begin();
    stateMachine.handleInput(InputEvent::SecondaryClick);  // -> Settings
    stateMachine.handleInput(InputEvent::PrimaryClick);    // -> Connecting
    wifi.setState(jinggua::application::WifiState::Failed);
    stateMachine.update(1000);
    EXPECT_EQ(runner, audio.count(SoundCue::Error), static_cast<std::size_t>(1));
    for (std::size_t index = 0; index < 10; ++index) {
      stateMachine.handleInput(InputEvent::PrimaryClick);
    }
    EXPECT_EQ(runner, audio.count(SoundCue::Error), static_cast<std::size_t>(1));
  }

  // F. Rapid input during an animation is coalesced by the state machine, so
  // it cannot create an audio queue flood or a second line.
  {
    SequenceRandomProvider random(
        std::vector<jinggua::domain::CoinSide>(18,
                                               jinggua::domain::CoinSide::Back));
    FakeWifiController wifi;
    FakeAudioController audio;
    DivinationSession session(random);
    StateMachine stateMachine(session, wifi);
    stateMachine.setAudioController(audio);
    stateMachine.begin();
    stateMachine.handleInput(InputEvent::PrimaryClick);
    stateMachine.handleInput(InputEvent::PrimaryClick);
    stateMachine.handleInput(InputEvent::PrimaryClick);
    for (std::size_t index = 0; index < 100; ++index) {
      stateMachine.handleInput(InputEvent::PrimaryClick);
      stateMachine.handleInput(InputEvent::Shake);
    }

    EXPECT_EQ(runner, session.lineCount(), static_cast<std::size_t>(1));
    EXPECT_EQ(runner, audio.count(SoundCue::Cast), static_cast<std::size_t>(1));
    EXPECT_EQ(runner, audio.playbackCount(), static_cast<std::size_t>(2));
  }
}

#include "test_framework.h"
#include "test_support.h"

#include "jinggua/application/state_machine.h"

void runStateMachineTests(TestRunner& runner) {
  std::vector<jinggua::domain::CoinSide> sequence;
  sequence.reserve(18);
  for (std::size_t index = 0; index < 6; ++index) {
    sequence.push_back(jinggua::domain::CoinSide::Front);
    sequence.push_back(jinggua::domain::CoinSide::Back);
    sequence.push_back(jinggua::domain::CoinSide::Back);
  }
  SequenceRandomProvider random(std::move(sequence));
  jinggua::application::DivinationSession session(random);
  jinggua::application::StateMachine stateMachine(session);

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

    // Input arriving during the presentation cannot create or acknowledge a
    // second line.
    stateMachine.handleInput(jinggua::application::InputEvent::PrimaryClick);
    EXPECT_EQ(runner, session.lineCount(), index + 1);
    EXPECT(runner, stateMachine.isLineAnimationActive());

    stateMachine.finishLineAnimation();
    EXPECT(runner, !stateMachine.isLineAnimationActive());
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
  jinggua::application::DivinationSession shakeSession(shakeRandom);
  jinggua::application::StateMachine shakeStateMachine(shakeSession);
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
}

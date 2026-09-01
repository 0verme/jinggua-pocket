#include "test_framework.h"
#include "test_support.h"

#include "jinggua/application/ring_history_store.h"
#include "jinggua/application/state_machine.h"
#include "jinggua/domain/history_record.h"

using jinggua::application::AppState;
using jinggua::application::DivinationSession;
using jinggua::application::InputEvent;
using jinggua::application::RingHistoryStore;
using jinggua::application::StateMachine;
using jinggua::domain::HistoryRecord;

void runHistoryApplicationTests(TestRunner& runner) {
  // -----------------------------------------------------------------------
  // A completed hexagram (6 lines) persists exactly one history record.
  // -----------------------------------------------------------------------
  {
    std::vector<jinggua::domain::CoinSide> sequence;
    sequence.reserve(18);
    for (std::size_t index = 0; index < 6; ++index) {
      sequence.push_back(jinggua::domain::CoinSide::Front);
      sequence.push_back(jinggua::domain::CoinSide::Back);
      sequence.push_back(jinggua::domain::CoinSide::Back);
    }
    SequenceRandomProvider random(std::move(sequence));
    DivinationSession session(random);

    InMemorySlotStorage storage(8);
    RingHistoryStore history(storage);
    EXPECT(runner, history.begin());
    StateMachine stateMachine(session, &history);

    stateMachine.begin();
    stateMachine.handleInput(InputEvent::PrimaryClick);  // Welcome -> Prepare
    stateMachine.handleInput(InputEvent::PrimaryClick);  // Prepare -> Casting

    // Cast six lines (PrimaryClick per line, plus acknowledgement clicks).
    for (std::size_t index = 0; index < 6; ++index) {
      stateMachine.handleInput(InputEvent::PrimaryClick);  // cast
      stateMachine.finishLineAnimation();
      if (index < 5) {
        stateMachine.handleInput(InputEvent::PrimaryClick);  // ack -> Casting
      }
    }
    EXPECT(runner, session.isComplete());
    EXPECT_EQ(runner, history.count(), static_cast<std::size_t>(1));

    // The persisted record matches the session result.
    HistoryRecord record;
    EXPECT(runner, history.get(0, record));
    EXPECT_EQ(runner, record.localRecordId, static_cast<std::uint32_t>(1));
    EXPECT(runner, record.originalNumber >= 1);
    EXPECT(runner, record.originalNumber <= 64);
    EXPECT_EQ(runner, record.changedNumber, static_cast<std::uint8_t>(0));

    // Persist on the NEXT completion → count becomes 2 (not 3).
    // From LineResult(complete): PrimaryClick → HexagramResult, then
    // PrimaryClick → ResetConfirm, then PrimaryClick → Prepare (resets).
    stateMachine.handleInput(InputEvent::PrimaryClick);  // LineResult -> HexagramResult
    stateMachine.handleInput(InputEvent::PrimaryClick);  // HexagramResult -> ResetConfirm
    stateMachine.handleInput(InputEvent::PrimaryClick);  // ResetConfirm -> Prepare (resets)
    EXPECT_EQ(runner, history.count(), static_cast<std::size_t>(1));

    // Reset happened; start a new divination from Prepare.
    stateMachine.handleInput(InputEvent::PrimaryClick);  // Prepare -> Casting
    for (std::size_t index = 0; index < 6; ++index) {
      stateMachine.handleInput(InputEvent::PrimaryClick);  // cast
      stateMachine.finishLineAnimation();
      if (index < 5) {
        stateMachine.handleInput(InputEvent::PrimaryClick);  // ack
      }
    }
    EXPECT_EQ(runner, history.count(), static_cast<std::size_t>(2));
  }

  // -----------------------------------------------------------------------
  // An incomplete hexagram (fewer than 6 lines) does NOT persist anything.
  // -----------------------------------------------------------------------
  {
    std::vector<jinggua::domain::CoinSide> sequence;
    sequence.reserve(18);
    for (std::size_t index = 0; index < 6; ++index) {
      sequence.push_back(jinggua::domain::CoinSide::Front);
      sequence.push_back(jinggua::domain::CoinSide::Back);
      sequence.push_back(jinggua::domain::CoinSide::Back);
    }
    SequenceRandomProvider random(std::move(sequence));
    DivinationSession session(random);

    InMemorySlotStorage storage(8);
    RingHistoryStore history(storage);
    EXPECT(runner, history.begin());
    StateMachine stateMachine(session, &history);

    stateMachine.begin();
    stateMachine.handleInput(InputEvent::PrimaryClick);  // Welcome -> Prepare
    stateMachine.handleInput(InputEvent::PrimaryClick);  // Prepare -> Casting

    // Only 3 lines.
    for (std::size_t index = 0; index < 3; ++index) {
      stateMachine.handleInput(InputEvent::PrimaryClick);  // cast
      stateMachine.finishLineAnimation();
      stateMachine.handleInput(InputEvent::PrimaryClick);  // ack -> Casting
    }
    EXPECT(runner, !session.isComplete());
    EXPECT_EQ(runner, history.count(), static_cast<std::size_t>(0));
  }

  // -----------------------------------------------------------------------
  // History browsing: Welcome + SecondaryClick enters History; cursor starts
  // at the newest record; A/B move older/newer; LongPress returns.
  // -----------------------------------------------------------------------
  {
    std::vector<jinggua::domain::CoinSide> sequence;
    sequence.reserve(18);
    for (std::size_t index = 0; index < 6; ++index) {
      sequence.push_back(jinggua::domain::CoinSide::Front);
      sequence.push_back(jinggua::domain::CoinSide::Back);
      sequence.push_back(jinggua::domain::CoinSide::Back);
    }
    SequenceRandomProvider random(std::move(sequence));
    DivinationSession session(random);

    InMemorySlotStorage storage(8);
    RingHistoryStore history(storage);
    EXPECT(runner, history.begin());

    // Pre-seed two records directly.
    HistoryRecord rec1;
    rec1.localRecordId = 0;
    rec1.originalNumber = 1;
    rec1.lineTotals = {7, 7, 7, 7, 7, 7};
    HistoryRecord rec2;
    rec2.localRecordId = 0;
    rec2.originalNumber = 2;
    rec2.lineTotals = {7, 7, 7, 7, 7, 7};
    EXPECT(runner, history.add(rec1));
    EXPECT(runner, history.add(rec2));
    EXPECT_EQ(runner, history.count(), static_cast<std::size_t>(2));

    StateMachine stateMachine(session, &history);
    stateMachine.begin();
    stateMachine.handleInput(InputEvent::SecondaryClick);  // Welcome -> History
    EXPECT_EQ(runner, stateMachine.state(), AppState::History);

    // Cursor starts at the newest record (index 1 = record 2).
    EXPECT_EQ(runner, stateMachine.historyCursor(), static_cast<std::size_t>(1));

    // SecondaryClick (newer) at the newest record clamps to the same index.
    stateMachine.handleInput(InputEvent::SecondaryClick);
    EXPECT_EQ(runner, stateMachine.historyCursor(), static_cast<std::size_t>(1));

    // PrimaryClick (older) moves to index 0.
    stateMachine.handleInput(InputEvent::PrimaryClick);
    EXPECT_EQ(runner, stateMachine.historyCursor(), static_cast<std::size_t>(0));

    // PrimaryClick at the oldest record clamps to 0.
    stateMachine.handleInput(InputEvent::PrimaryClick);
    EXPECT_EQ(runner, stateMachine.historyCursor(), static_cast<std::size_t>(0));

    // LongPress returns to Welcome.
    stateMachine.handleInput(InputEvent::LongPress);
    EXPECT_EQ(runner, stateMachine.state(), AppState::Welcome);
  }

  // -----------------------------------------------------------------------
  // StateMachine without history store (nullptr) still works: no persistence
  // and History screen shows an empty store.
  // -----------------------------------------------------------------------
  {
    std::vector<jinggua::domain::CoinSide> sequence;
    sequence.reserve(18);
    for (std::size_t index = 0; index < 6; ++index) {
      sequence.push_back(jinggua::domain::CoinSide::Front);
      sequence.push_back(jinggua::domain::CoinSide::Back);
      sequence.push_back(jinggua::domain::CoinSide::Back);
    }
    SequenceRandomProvider random(std::move(sequence));
    DivinationSession session(random);
    StateMachine stateMachine(session);  // history = nullptr

    stateMachine.begin();
    stateMachine.handleInput(InputEvent::SecondaryClick);  // -> History
    EXPECT_EQ(runner, stateMachine.state(), AppState::History);
    EXPECT(runner, stateMachine.history() == nullptr);
    EXPECT_EQ(runner, stateMachine.historyCursor(), static_cast<std::size_t>(0));
  }
}

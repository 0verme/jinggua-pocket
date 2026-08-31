#include <iostream>

#include "test_framework.h"

void runCoinTests(TestRunner& runner);
void runDivinationFlowTests(TestRunner& runner);
void runYaoTests(TestRunner& runner);
void runHexagramTests(TestRunner& runner);
void runImuTests(TestRunner& runner);
void runMicrophoneResearchTests(TestRunner& runner);
void runTransformTests(TestRunner& runner);
void runOrderingTests(TestRunner& runner);
void runStateMachineTests(TestRunner& runner);
void runHistoryRecordTests(TestRunner& runner);
void runHistoryIndexTests(TestRunner& runner);
void runRingHistoryStoreTests(TestRunner& runner);
void runHistoryApplicationTests(TestRunner& runner);

int main() {
  TestRunner runner;
  runCoinTests(runner);
  runDivinationFlowTests(runner);
  runYaoTests(runner);
  runHexagramTests(runner);
  runImuTests(runner);
  runMicrophoneResearchTests(runner);
  runTransformTests(runner);
  runOrderingTests(runner);
  runStateMachineTests(runner);
  runHistoryRecordTests(runner);
  runHistoryIndexTests(runner);
  runRingHistoryStoreTests(runner);
  runHistoryApplicationTests(runner);

  std::cout << "jinggua native tests: " << runner.assertions()
            << " assertions, " << runner.failures() << " failures\n";
  return runner.result();
}

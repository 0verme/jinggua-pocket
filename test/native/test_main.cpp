#include <iostream>

#include "test_framework.h"

void runCoinTests(TestRunner& runner);
void runYaoTests(TestRunner& runner);
void runHexagramTests(TestRunner& runner);
void runImuTests(TestRunner& runner);
void runTransformTests(TestRunner& runner);
void runOrderingTests(TestRunner& runner);
void runStateMachineTests(TestRunner& runner);

int main() {
  TestRunner runner;
  runCoinTests(runner);
  runYaoTests(runner);
  runHexagramTests(runner);
  runImuTests(runner);
  runTransformTests(runner);
  runOrderingTests(runner);
  runStateMachineTests(runner);

  std::cout << "jinggua native tests: " << runner.assertions()
            << " assertions, " << runner.failures() << " failures\n";
  return runner.result();
}

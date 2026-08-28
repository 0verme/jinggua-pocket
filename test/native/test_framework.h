#pragma once

#include <iostream>
#include <string>
#include <utility>

class TestRunner {
 public:
  void expect(bool condition, const char* expression, const char* file,
              int line) {
    ++assertions_;
    if (!condition) {
      ++failures_;
      std::cerr << file << ":" << line << ": expected " << expression
                << '\n';
    }
  }

  template <typename Actual, typename Expected>
  void expectEqual(const Actual& actual, const Expected& expected,
                   const char* actualExpression,
                   const char* expectedExpression, const char* file,
                   int line) {
    expect(actual == expected, actualExpression, file, line);
    if (actual != expected) {
      std::cerr << "  actual expression: " << actualExpression
                << ", expected expression: " << expectedExpression << '\n';
    }
  }

  int result() const { return failures_ == 0 ? 0 : 1; }
  int failures() const { return failures_; }
  int assertions() const { return assertions_; }

 private:
  int failures_{0};
  int assertions_{0};
};

#define EXPECT(runner, expression) \
  (runner).expect((expression), #expression, __FILE__, __LINE__)
#define EXPECT_EQ(runner, actual, expected) \
  (runner).expectEqual((actual), (expected), #actual, #expected, __FILE__, \
                       __LINE__)

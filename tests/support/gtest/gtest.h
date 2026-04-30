#pragma once

#include <cmath>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace testing {

struct TestRecord {
  std::string suiteName;
  std::string testName;
  std::function<void()> function;
};

namespace detail {

inline std::vector<TestRecord> &registry() {
  static std::vector<TestRecord> tests;
  return tests;
}

struct TestContext {
  int failures {0};
};

inline TestContext *&currentContext() {
  static TestContext *context = nullptr;
  return context;
}

inline bool registerTest(std::string suiteName, std::string testName, std::function<void()> function) {
  registry().push_back(TestRecord {std::move(suiteName), std::move(testName), std::move(function)});
  return true;
}

[[noreturn]] inline void throwFatalFailure(const std::string &message) { throw std::runtime_error(message); }

inline void recordFailure(const char *file,
                          const int line,
                          const std::string &message,
                          const bool fatal) {
  if (currentContext() != nullptr) {
    currentContext()->failures += 1;
  }

  std::cerr << file << ":" << line << ": failure: " << message << std::endl;
  if (fatal) {
    throwFatalFailure(message);
  }
}

template <typename Left, typename Right>
inline void expectEqual(const Left &left,
                        const Right &right,
                        const char *leftExpression,
                        const char *rightExpression,
                        const char *file,
                        const int line,
                        const bool fatal) {
  if (!(left == right)) {
    std::ostringstream stream;
    stream << leftExpression << " == " << rightExpression << " failed";
    recordFailure(file, line, stream.str(), fatal);
  }
}

template <typename Value>
inline void expectTrue(const Value &value,
                       const char *expression,
                       const char *file,
                       const int line,
                       const bool fatal) {
  if (!static_cast<bool>(value)) {
    std::ostringstream stream;
    stream << expression << " is false";
    recordFailure(file, line, stream.str(), fatal);
  }
}

template <typename Left, typename Right, typename Tolerance>
inline void expectNear(const Left &left,
                       const Right &right,
                       const Tolerance &tolerance,
                       const char *leftExpression,
                       const char *rightExpression,
                       const char *file,
                       const int line,
                       const bool fatal) {
  if (std::fabs(static_cast<double>(left) - static_cast<double>(right)) >
      static_cast<double>(tolerance)) {
    std::ostringstream stream;
    stream << leftExpression << " ~= " << rightExpression << " failed";
    recordFailure(file, line, stream.str(), fatal);
  }
}

} // namespace detail

inline void InitGoogleTest(int *, char **) {}

inline int RUN_ALL_TESTS() {
  int failedTests = 0;

  for (const auto &test : detail::registry()) {
    detail::TestContext context;
    detail::currentContext() = &context;

    try {
      test.function();
    } catch (const std::exception &) {
    }

    if (context.failures == 0) {
      std::cout << "[  PASSED  ] " << test.suiteName << "." << test.testName << std::endl;
    } else {
      std::cout << "[  FAILED  ] " << test.suiteName << "." << test.testName << " ("
                << context.failures << " failure(s))" << std::endl;
      failedTests += 1;
    }
  }

  detail::currentContext() = nullptr;
  std::cout << "[ SUMMARY ] Total: " << detail::registry().size() << ", Failed: " << failedTests
            << std::endl;
  return failedTests == 0 ? 0 : 1;
}

} // namespace testing

#define TEST(suite_name, test_name)                                                                  \
  static void suite_name##_##test_name##_impl();                                                     \
  static const bool suite_name##_##test_name##_registered =                                          \
      ::testing::detail::registerTest(#suite_name, #test_name, suite_name##_##test_name##_impl);    \
  static void suite_name##_##test_name##_impl()

#define EXPECT_EQ(left, right)                                                                       \
  ::testing::detail::expectEqual((left), (right), #left, #right, __FILE__, __LINE__, false)

#define ASSERT_EQ(left, right)                                                                       \
  ::testing::detail::expectEqual((left), (right), #left, #right, __FILE__, __LINE__, true)

#define EXPECT_TRUE(expression)                                                                      \
  ::testing::detail::expectTrue((expression), #expression, __FILE__, __LINE__, false)

#define ASSERT_TRUE(expression)                                                                      \
  ::testing::detail::expectTrue((expression), #expression, __FILE__, __LINE__, true)

#define EXPECT_NEAR(left, right, tolerance)                                                          \
  ::testing::detail::expectNear((left), (right), (tolerance), #left, #right, __FILE__, __LINE__,    \
                                false)

#define ASSERT_NEAR(left, right, tolerance)                                                          \
  ::testing::detail::expectNear((left), (right), (tolerance), #left, #right, __FILE__, __LINE__,    \
                                true)


#pragma once

#include <string>
#include <utility>

template <typename T>
struct Result {
  bool ok {false};
  std::string message;
  T value {};

  explicit operator bool() const { return ok; }

  static Result<T> success(T resultValue, std::string resultMessage = {}) {
    return Result<T> {true, std::move(resultMessage), std::move(resultValue)};
  }

  static Result<T> failure(std::string resultMessage) {
    return Result<T> {false, std::move(resultMessage), T {}};
  }
};

template <>
struct Result<void> {
  bool ok {false};
  std::string message;

  explicit operator bool() const { return ok; }

  static Result<void> success(std::string resultMessage = {}) {
    return Result<void> {true, std::move(resultMessage)};
  }

  static Result<void> failure(std::string resultMessage) {
    return Result<void> {false, std::move(resultMessage)};
  }
};

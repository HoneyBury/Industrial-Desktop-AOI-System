#pragma once

#include <string>
#include <utility>

enum class ErrorCode {
  None = 0,

  // IO
  FileNotFound,
  FileReadError,
  FileWriteError,

  // Data
  JsonParseError,
  InvalidFormat,

  // Hardware
  CameraNotAvailable,
  LaserNotReady,
  MotionNotHomed,
  TransportTimeout,

  // Network
  NetworkError,

  // Calibration
  CalibrationFailed,
  InsufficientData,

  // General
  Unknown,
};

template <typename T>
struct Result {
  bool ok {false};
  ErrorCode code {ErrorCode::None};
  std::string message;
  T value {};

  explicit operator bool() const { return ok; }

  static Result<T> success(T resultValue, std::string resultMessage = {}) {
    return Result<T> {true, ErrorCode::None, std::move(resultMessage), std::move(resultValue)};
  }

  static Result<T> failure(ErrorCode errorCode, std::string resultMessage) {
    return Result<T> {false, errorCode, std::move(resultMessage), T {}};
  }

  // Backward-compatible overload without error code.
  static Result<T> failure(std::string resultMessage) {
    return Result<T> {false, ErrorCode::Unknown, std::move(resultMessage), T {}};
  }
};

template <>
struct Result<void> {
  bool ok {false};
  ErrorCode code {ErrorCode::None};
  std::string message;

  explicit operator bool() const { return ok; }

  static Result<void> success(std::string resultMessage = {}) {
    return Result<void> {true, ErrorCode::None, std::move(resultMessage)};
  }

  static Result<void> failure(ErrorCode errorCode, std::string resultMessage) {
    return Result<void> {false, errorCode, std::move(resultMessage)};
  }

  // Backward-compatible overload without error code.
  static Result<void> failure(std::string resultMessage) {
    return Result<void> {false, ErrorCode::Unknown, std::move(resultMessage)};
  }
};

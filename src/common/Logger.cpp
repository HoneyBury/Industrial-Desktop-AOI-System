#include "common/Logger.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace {

std::mutex &logMutex() {
  static std::mutex mutex;
  return mutex;
}

std::string &logFilePath() {
  static std::string path;
  return path;
}

LogLevel &minLevel() {
  static LogLevel level {LogLevel::Debug};
  return level;
}

std::ofstream &logFileStream() {
  static std::ofstream stream;
  return stream;
}

const char *levelToString(const LogLevel level) {
  switch (level) {
  case LogLevel::Debug:   return "DEBUG";
  case LogLevel::Info:    return "INFO";
  case LogLevel::Warning: return "WARN";
  case LogLevel::Error:   return "ERROR";
  }
  return "UNKNOWN";
}

std::string timestamp() {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);

  std::tm localTime {};
#if defined(_WIN32)
  localtime_s(&localTime, &time);
#else
  localtime_r(&time, &localTime);
#endif

  std::ostringstream stream;
  stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
  return stream.str();
}

} // namespace

void Logger::setLogFile(const std::string &filePath) {
  std::scoped_lock lock(logMutex());
  auto &fs = logFileStream();
  if (fs.is_open()) {
    fs.close();
  }
  logFilePath() = filePath;
  if (!filePath.empty()) {
    fs.open(filePath, std::ios::out | std::ios::app);
  }
}

void Logger::setMinLevel(const LogLevel level) {
  minLevel() = level;
}

void Logger::log(const LogLevel level, const std::string &message) {
  if (level < minLevel()) {
    return;
  }

  const std::string formatted =
      "[" + timestamp() + "][" + levelToString(level) + "] " + message;

  std::scoped_lock lock(logMutex());

  auto &consoleStream = level == LogLevel::Error ? std::cerr : std::cout;
  consoleStream << formatted << std::endl;

  auto &fs = logFileStream();
  if (fs.is_open()) {
    fs << formatted << std::endl;
  }
}

void Logger::debug(const std::string &message)   { log(LogLevel::Debug, message); }
void Logger::info(const std::string &message)    { log(LogLevel::Info, message); }
void Logger::warning(const std::string &message) { log(LogLevel::Warning, message); }
void Logger::error(const std::string &message)   { log(LogLevel::Error, message); }

#pragma once

#include <string>

enum class LogLevel { Debug, Info, Warning, Error };

class Logger {
public:
  static void setLogFile(const std::string &filePath);
  static void setMinLevel(LogLevel level);

  static void log(LogLevel level, const std::string &message);
  static void debug(const std::string &message);
  static void info(const std::string &message);
  static void warning(const std::string &message);
  static void error(const std::string &message);
};

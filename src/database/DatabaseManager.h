#pragma once

#include "common/Result.h"

#include <string>

class DatabaseManager {
public:
  Result<void> open(const std::string &databasePath);
  void close();
  bool isOpen() const;
  std::string databasePath() const;

private:
  std::string databasePath_;
  bool open_ {false};
};


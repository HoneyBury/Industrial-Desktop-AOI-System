#include "database/DatabaseManager.h"

Result<void> DatabaseManager::open(const std::string &databasePath) {
  databasePath_ = databasePath;
  open_ = !databasePath.empty();
  if (!open_) {
    return Result<void>::failure("Database path is empty.");
  }

  return Result<void>::success("Database opened in bootstrap mode.");
}

void DatabaseManager::close() { open_ = false; }

bool DatabaseManager::isOpen() const { return open_; }

std::string DatabaseManager::databasePath() const { return databasePath_; }


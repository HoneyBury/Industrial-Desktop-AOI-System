#include "database/DatabaseManager.h"

#include <sstream>

#ifdef AOI_HAS_SQLITE
#include <sqlite3.h>
#endif

namespace {

std::string currentTimestamp() {
  const auto now = std::time(nullptr);
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", std::localtime(&now));
  return {buffer};
}

std::string escapeSql(const std::string &input) {
  std::string output;
  output.reserve(input.size() * 2);
  for (const char c : input) {
    if (c == '\'') {
      output += "''";
    } else {
      output += c;
    }
  }
  return output;
}

} // namespace

Result<void> DatabaseManager::open(const std::string &databasePath) {
#ifdef AOI_HAS_SQLITE
  if (databasePath.empty()) {
    return Result<void>::failure("Database path is empty.");
  }

  // Close any previously open database.
  if (db_ != nullptr) {
    sqlite3_close(db_);
    db_ = nullptr;
  }

  const int rc = sqlite3_open(databasePath.c_str(), &db_);
  if (rc != SQLITE_OK) {
    const std::string err = db_ != nullptr ? sqlite3_errmsg(db_) : "unknown error";
    if (db_ != nullptr) {
      sqlite3_close(db_);
      db_ = nullptr;
    }
    return Result<void>::failure("Failed to open database: " + err);
  }

  // Enable WAL mode for better concurrent read performance.
  sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
  sqlite3_exec(db_, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);

  databasePath_ = databasePath;
  open_ = true;

  return createSchemaIfNeeded();
#else
  databasePath_ = databasePath;
  open_ = !databasePath.empty();
  if (!open_) {
    return Result<void>::failure("Database path is empty.");
  }
  return Result<void>::success("Database opened in bootstrap mode.");
#endif
}

void DatabaseManager::close() {
#ifdef AOI_HAS_SQLITE
  if (db_ != nullptr) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
#endif
  open_ = false;
}

bool DatabaseManager::isOpen() const { return open_; }

std::string DatabaseManager::databasePath() const { return databasePath_; }

Result<void> DatabaseManager::createSchemaIfNeeded() {
#ifdef AOI_HAS_SQLITE
  if (db_ == nullptr) {
    return Result<void>::failure("Database handle is null.");
  }

  const char *schema = R"SQL(
    CREATE TABLE IF NOT EXISTS inspection_results (
      id          INTEGER PRIMARY KEY AUTOINCREMENT,
      board_id    TEXT    NOT NULL,
      timestamp   TEXT    NOT NULL,
      program_name TEXT   DEFAULT '',
      image_path  TEXT    DEFAULT '',
      ai_label    TEXT    DEFAULT '',
      ai_confidence REAL  DEFAULT 0.0,
      final_decision TEXT DEFAULT 'NG',
      marks_count INTEGER DEFAULT 0,
      rois_count  INTEGER DEFAULT 0,
      details_json TEXT  DEFAULT '{}'
    );

    CREATE TABLE IF NOT EXISTS calibration_history (
      id               INTEGER PRIMARY KEY AUTOINCREMENT,
      timestamp        TEXT    NOT NULL,
      calibration_type TEXT    NOT NULL,
      fx               REAL    DEFAULT 0.0,
      fy               REAL    DEFAULT 0.0,
      cx               REAL    DEFAULT 0.0,
      cy               REAL    DEFAULT 0.0,
      pixel_scale_x    REAL    DEFAULT 0.0,
      pixel_scale_y    REAL    DEFAULT 0.0,
      origin_x         REAL    DEFAULT 0.0,
      origin_y         REAL    DEFAULT 0.0,
      origin_r         REAL    DEFAULT 0.0,
      laser_offset_dx  REAL    DEFAULT 0.0,
      laser_offset_dy  REAL    DEFAULT 0.0,
      notes            TEXT    DEFAULT ''
    );

    CREATE TABLE IF NOT EXISTS board_records (
      id           INTEGER PRIMARY KEY AUTOINCREMENT,
      board_id     TEXT    NOT NULL UNIQUE,
      program_name TEXT    DEFAULT '',
      status       TEXT    DEFAULT 'pending',
      created_at   TEXT    NOT NULL,
      updated_at   TEXT    NOT NULL
    );

    CREATE INDEX IF NOT EXISTS idx_inspection_board_id ON inspection_results(board_id);
    CREATE INDEX IF NOT EXISTS idx_inspection_timestamp ON inspection_results(timestamp);
    CREATE INDEX IF NOT EXISTS idx_calibration_type ON calibration_history(calibration_type);
  )SQL";

  char *errMsg = nullptr;
  const int rc = sqlite3_exec(db_, schema, nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    const std::string err = errMsg != nullptr ? errMsg : "unknown error";
    sqlite3_free(errMsg);
    return Result<void>::failure("Failed to create database schema: " + err);
  }

  return Result<void>::success("Database schema ensured at " + databasePath_);
#else
  return Result<void>::success("Database schema skipped (no SQLite).");
#endif
}

Result<void> DatabaseManager::insertInspectionResult(const InspectionRecord &record) {
#ifdef AOI_HAS_SQLITE
  if (db_ == nullptr) {
    return Result<void>::failure("Database is not open.");
  }

  const std::string timestamp = record.timestamp.empty() ? currentTimestamp() : record.timestamp;

  std::ostringstream sql;
  sql << "INSERT INTO inspection_results "
         "(board_id, timestamp, program_name, image_path, ai_label, ai_confidence, "
         "final_decision, marks_count, rois_count, details_json) VALUES ('"
      << escapeSql(record.boardId) << "', '"
      << escapeSql(timestamp) << "', '"
      << escapeSql(record.programName) << "', '"
      << escapeSql(record.imagePath) << "', '"
      << escapeSql(record.aiLabel) << "', "
      << record.aiConfidence << ", '"
      << escapeSql(record.finalDecision) << "', "
      << record.marksCount << ", "
      << record.roisCount << ", '"
      << escapeSql(record.detailsJson) << "');";

  char *errMsg = nullptr;
  const int rc = sqlite3_exec(db_, sql.str().c_str(), nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    const std::string err = errMsg != nullptr ? errMsg : "unknown error";
    sqlite3_free(errMsg);
    return Result<void>::failure("Failed to insert inspection result: " + err);
  }

  return Result<void>::success("Inspection result saved.");
#else
  return Result<void>::success("Inspection result logged (stub).");
#endif
}

Result<std::vector<InspectionRecord>> DatabaseManager::queryInspectionResults(int limit) const {
  std::vector<InspectionRecord> results;

#ifdef AOI_HAS_SQLITE
  if (db_ == nullptr) {
    return Result<std::vector<InspectionRecord>>::failure("Database is not open.");
  }

  std::ostringstream sql;
  sql << "SELECT board_id, timestamp, program_name, image_path, ai_label, ai_confidence, "
         "final_decision, marks_count, rois_count, details_json "
         "FROM inspection_results ORDER BY id DESC LIMIT "
      << limit << ";";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql.str().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    return Result<std::vector<InspectionRecord>>::failure("Query failed: " +
                                                          std::string(sqlite3_errmsg(db_)));
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    InspectionRecord rec;
    rec.boardId = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    rec.timestamp = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    rec.programName = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    rec.imagePath = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
    rec.aiLabel = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
    rec.aiConfidence = sqlite3_column_double(stmt, 5);
    rec.finalDecision = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
    rec.marksCount = sqlite3_column_int(stmt, 7);
    rec.roisCount = sqlite3_column_int(stmt, 8);
    rec.detailsJson = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));
    results.push_back(rec);
  }

  sqlite3_finalize(stmt);
  return Result<std::vector<InspectionRecord>>::success(
      results, "Query returned " + std::to_string(results.size()) + " record(s).");
#else
  return Result<std::vector<InspectionRecord>>::success(results, "Query returned 0 records (stub).");
#endif
}

Result<void> DatabaseManager::insertCalibrationRecord(const CalibrationRecord &record) {
#ifdef AOI_HAS_SQLITE
  if (db_ == nullptr) {
    return Result<void>::failure("Database is not open.");
  }

  const std::string timestamp = record.timestamp.empty() ? currentTimestamp() : record.timestamp;

  std::ostringstream sql;
  sql << "INSERT INTO calibration_history "
         "(timestamp, calibration_type, fx, fy, cx, cy, pixel_scale_x, pixel_scale_y, "
         "origin_x, origin_y, origin_r, laser_offset_dx, laser_offset_dy, notes) VALUES ('"
      << escapeSql(timestamp) << "', '"
      << escapeSql(record.calibrationType) << "', "
      << record.fx << ", " << record.fy << ", " << record.cx << ", " << record.cy << ", "
      << record.pixelScaleX << ", " << record.pixelScaleY << ", "
      << record.originX << ", " << record.originY << ", " << record.originR << ", "
      << record.laserOffsetDx << ", " << record.laserOffsetDy << ", '"
      << escapeSql(record.notes) << "');";

  char *errMsg = nullptr;
  const int rc = sqlite3_exec(db_, sql.str().c_str(), nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    const std::string err = errMsg != nullptr ? errMsg : "unknown error";
    sqlite3_free(errMsg);
    return Result<void>::failure("Failed to insert calibration record: " + err);
  }

  return Result<void>::success("Calibration record saved.");
#else
  return Result<void>::success("Calibration record logged (stub).");
#endif
}

Result<std::vector<CalibrationRecord>> DatabaseManager::queryCalibrationHistory(int limit) const {
  std::vector<CalibrationRecord> results;

#ifdef AOI_HAS_SQLITE
  if (db_ == nullptr) {
    return Result<std::vector<CalibrationRecord>>::failure("Database is not open.");
  }

  std::ostringstream sql;
  sql << "SELECT timestamp, calibration_type, fx, fy, cx, cy, pixel_scale_x, pixel_scale_y, "
         "origin_x, origin_y, origin_r, laser_offset_dx, laser_offset_dy, notes "
         "FROM calibration_history ORDER BY id DESC LIMIT "
      << limit << ";";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql.str().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    return Result<std::vector<CalibrationRecord>>::failure("Query failed: " +
                                                          std::string(sqlite3_errmsg(db_)));
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    CalibrationRecord rec;
    rec.timestamp = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    rec.calibrationType = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    rec.fx = sqlite3_column_double(stmt, 2);
    rec.fy = sqlite3_column_double(stmt, 3);
    rec.cx = sqlite3_column_double(stmt, 4);
    rec.cy = sqlite3_column_double(stmt, 5);
    rec.pixelScaleX = sqlite3_column_double(stmt, 6);
    rec.pixelScaleY = sqlite3_column_double(stmt, 7);
    rec.originX = sqlite3_column_double(stmt, 8);
    rec.originY = sqlite3_column_double(stmt, 9);
    rec.originR = sqlite3_column_double(stmt, 10);
    rec.laserOffsetDx = sqlite3_column_double(stmt, 11);
    rec.laserOffsetDy = sqlite3_column_double(stmt, 12);
    rec.notes = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 13));
    results.push_back(rec);
  }

  sqlite3_finalize(stmt);
  return Result<std::vector<CalibrationRecord>>::success(
      results, "Query returned " + std::to_string(results.size()) + " record(s).");
#else
  return Result<std::vector<CalibrationRecord>>::success(results, "Query returned 0 records (stub).");
#endif
}

Result<int> DatabaseManager::countBoardResults(const std::string &decision,
                                               const std::string &sinceTimestamp) const {
#ifdef AOI_HAS_SQLITE
  if (db_ == nullptr) {
    return Result<int>::failure("Database is not open.");
  }

  std::ostringstream sql;
  sql << "SELECT COUNT(*) FROM inspection_results WHERE final_decision = '"
      << escapeSql(decision) << "'";
  if (!sinceTimestamp.empty()) {
    sql << " AND timestamp >= '" << escapeSql(sinceTimestamp) << "'";
  }
  sql << ";";

  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql.str().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    return Result<int>::failure("Count query failed: " + std::string(sqlite3_errmsg(db_)));
  }

  int count = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    count = sqlite3_column_int(stmt, 0);
  }

  sqlite3_finalize(stmt);
  return Result<int>::success(count, "Count query returned " + std::to_string(count));
#else
  return Result<int>::success(0, "Count query returned 0 (stub).");
#endif
}

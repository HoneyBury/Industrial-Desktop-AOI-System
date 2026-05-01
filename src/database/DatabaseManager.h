#pragma once

#include "common/Result.h"

#include <ctime>
#include <string>
#include <vector>

struct InspectionRecord {
  std::string boardId;
  std::string timestamp;
  std::string programName;
  std::string imagePath;
  std::string aiLabel;
  double aiConfidence {0.0};
  std::string finalDecision; // "OK" or "NG"
  int marksCount {0};
  int roisCount {0};
  std::string detailsJson;
};

struct CalibrationRecord {
  std::string timestamp;
  std::string calibrationType;
  double fx {0.0};
  double fy {0.0};
  double cx {0.0};
  double cy {0.0};
  double pixelScaleX {0.0};
  double pixelScaleY {0.0};
  double originX {0.0};
  double originY {0.0};
  double originR {0.0};
  double laserOffsetDx {0.0};
  double laserOffsetDy {0.0};
  std::string notes;
};

class DatabaseManager {
public:
  Result<void> open(const std::string &databasePath);
  void close();
  bool isOpen() const;
  std::string databasePath() const;

  // Inspection result persistence.
  Result<void> insertInspectionResult(const InspectionRecord &record);
  Result<std::vector<InspectionRecord>> queryInspectionResults(int limit = 100) const;

  // Calibration history.
  Result<void> insertCalibrationRecord(const CalibrationRecord &record);
  Result<std::vector<CalibrationRecord>> queryCalibrationHistory(int limit = 50) const;

  // Production statistics query.
  Result<int> countBoardResults(const std::string &decision, const std::string &sinceTimestamp = "") const;

private:
  Result<void> createSchemaIfNeeded();

  std::string databasePath_;
  bool open_ {false};

#ifdef AOI_HAS_SQLITE
  struct sqlite3 *db_ {nullptr};
#endif
};

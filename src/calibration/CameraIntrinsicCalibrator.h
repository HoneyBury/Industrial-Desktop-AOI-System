#pragma once

#include "calibration/CalibrationTypes.h"
#include "common/Result.h"

#include <string>

namespace calibration {

class CameraIntrinsicCalibrator {
public:
  Result<CameraIntrinsicCalibration> calibrateFromChessboard(const std::string &datasetDirectory) const;
  Result<void> save(const CameraIntrinsicCalibration &data, const std::string &filePath) const;
  Result<CameraIntrinsicCalibration> load(const std::string &filePath) const;
};

} // namespace calibration

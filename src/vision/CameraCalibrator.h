#pragma once

#include "common/Result.h"

#include <string>

struct CameraCalibrationData {
  double fx {0.0};
  double fy {0.0};
  double cx {0.0};
  double cy {0.0};
  double pixelToMillimeterX {0.01};
  double pixelToMillimeterY {0.01};
};

class CameraCalibrator {
public:
  Result<CameraCalibrationData> calibrateFromChessboard(const std::string &datasetDirectory) const;
  Result<void> save(const CameraCalibrationData &data, const std::string &filePath) const;
  Result<CameraCalibrationData> load(const std::string &filePath) const;
};


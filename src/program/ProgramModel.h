#pragma once

#include "vision/CameraCalibrator.h"
#include "vision/MarkDetector.h"
#include "vision/RoiDetector.h"

#include <string>
#include <vector>

struct ProgramModel {
  std::string name;
  std::string aiModelPath;
  std::string calibrationFilePath;
  std::string codeRegionName;
  std::vector<MarkPoint> marks;
  std::vector<RoiRegion> rois;
  CameraCalibrationData calibrationData;
};


#include "vision/RoiDetector.h"

Result<std::vector<RoiRegion>> RoiDetector::detectByThreshold(const std::string &imagePath) const {
  return Result<std::vector<RoiRegion>>::success(
      {{20.0, 20.0, 120.0, 60.0}}, "Threshold ROI detection completed for " + imagePath);
}

Result<std::vector<RoiRegion>> RoiDetector::detectByTemplate(const std::string &imagePath) const {
  return Result<std::vector<RoiRegion>>::success(
      {{32.0, 44.0, 100.0, 40.0}}, "Template ROI detection completed for " + imagePath);
}


#include "vision/RoiDetector.h"

Result<std::vector<RoiRegion>> RoiDetector::detectByThreshold(const std::string &imagePath) const {
  return Result<std::vector<RoiRegion>>::success(
      {{"Threshold-ROI", 20.0, 20.0, 120.0, 60.0, 0.0, 0.78, RoiShape::Rectangle, true}},
      "Threshold ROI detection completed for " + imagePath);
}

Result<std::vector<RoiRegion>> RoiDetector::detectByTemplate(const std::string &imagePath) const {
  return Result<std::vector<RoiRegion>>::success(
      {{"Template-ROI", 32.0, 44.0, 100.0, 100.0, 0.0, 0.82, RoiShape::Circle, true}},
      "Template ROI detection completed for " + imagePath);
}

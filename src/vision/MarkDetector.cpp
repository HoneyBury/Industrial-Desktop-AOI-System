#include "vision/MarkDetector.h"

Result<std::vector<MarkPoint>> MarkDetector::detectTemplateMarks(const std::string &imagePath) const {
  return Result<std::vector<MarkPoint>>::success(
      {{100.0, 80.0, 0.95}, {240.0, 82.0, 0.93}},
      "Stub mark detection completed for " + imagePath);
}

Result<std::vector<MarkPoint>> MarkDetector::detectCircularMarks(const std::string &imagePath) const {
  return Result<std::vector<MarkPoint>>::success(
      {{120.0, 120.0, 0.91}}, "Stub circle detection completed for " + imagePath);
}


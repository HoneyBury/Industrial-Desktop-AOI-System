#include "vision/MarkDetector.h"

Result<std::vector<MarkPoint>> MarkDetector::detectTemplateMarks(const std::string &imagePath) const {
  return Result<std::vector<MarkPoint>>::success(
      {{"Mark-A", 100.0, 80.0, 52.0, 46.0, 0.0, 0.92, 0.82, 0.89, 16, "#ff4d4f",
        MarkShape::Diamond, MarkAlgorithm::ColorBrushTemplate, true},
       {"Mark-B", 240.0, 82.0, 48.0, 48.0, 0.0, 0.90, 0.80, 0.87, 14, "#f97316",
        MarkShape::Cross, MarkAlgorithm::ColorBrushTemplate, true}},
      "Stub mark detection completed for " + imagePath);
}

Result<std::vector<MarkPoint>> MarkDetector::detectCircularMarks(const std::string &imagePath) const {
  return Result<std::vector<MarkPoint>>::success(
      {{"Circle-Mark", 120.0, 120.0, 42.0, 42.0, 0.0, 0.91, 0.80, 0.88, 12, "#ffffff",
        MarkShape::Circle, MarkAlgorithm::BinaryGeometry, true}},
      "Stub circle detection completed for " + imagePath);
}

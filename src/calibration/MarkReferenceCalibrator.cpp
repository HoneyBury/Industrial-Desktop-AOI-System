#include "calibration/MarkReferenceCalibrator.h"

namespace calibration {

Result<std::vector<MarkReferenceRecord>> MarkReferenceCalibrator::recordReferences(
    const std::vector<MarkPoint> &marks, const PixelScaleCalibration &pixelScale) const {
  if (marks.empty()) {
    return Result<std::vector<MarkReferenceRecord>>::failure("No mark definitions available for reference recording.");
  }

  std::vector<MarkReferenceRecord> references;
  references.reserve(marks.size());
  for (const auto &mark : marks) {
    references.push_back(MarkReferenceRecord {
        mark.name,
        PixelPoint {mark.x, mark.y},
        MillimeterPoint {mark.x * pixelScale.pixelToMillimeterX, mark.y * pixelScale.pixelToMillimeterY},
        mark.enabled,
    });
  }

  return Result<std::vector<MarkReferenceRecord>>::success(references, "Mark reference recording completed.");
}

} // namespace calibration

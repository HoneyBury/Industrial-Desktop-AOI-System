#pragma once

#include "calibration/CalibrationTypes.h"
#include "common/Result.h"
#include "vision/MarkDetector.h"

#include <vector>

namespace calibration {

class MarkReferenceCalibrator {
public:
  Result<std::vector<MarkReferenceRecord>> recordReferences(const std::vector<MarkPoint> &marks,
                                                            const PixelScaleCalibration &pixelScale) const;
};

} // namespace calibration

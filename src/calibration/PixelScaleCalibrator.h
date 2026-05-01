#pragma once

#include "calibration/CalibrationTypes.h"
#include "common/Result.h"

namespace calibration {

class PixelScaleCalibrator {
public:
  Result<PixelScaleCalibration> calibrateFromKnownDistance(double pixelDistanceX, double pixelDistanceY,
                                                           double physicalDistanceXmm,
                                                           double physicalDistanceYmm) const;
};

} // namespace calibration

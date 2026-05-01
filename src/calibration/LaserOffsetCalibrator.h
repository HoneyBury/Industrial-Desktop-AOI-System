#pragma once

#include "calibration/CalibrationTypes.h"
#include "common/Result.h"

namespace calibration {

struct LaserOffsetCalibrationInput {
  PixelPoint laserCrossPixel;
  PixelPoint opticalCenterPixel;
  PixelScaleCalibration pixelScale;
};

struct LaserOffsetCalibrationResult {
  LaserOffsetCalibration calibration;
  PixelPoint pixelOffset;
  MillimeterPoint millimeterOffset;
};

class LaserOffsetCalibrator {
public:
  Result<LaserOffsetCalibrationResult> calibrate(const LaserOffsetCalibrationInput &input) const;
};

} // namespace calibration

#pragma once

#include "calibration/CalibrationTypes.h"
#include "common/Result.h"

namespace calibration {

struct OriginCalibrationInput {
  PixelPoint measuredReferencePixel;
  PixelPoint opticalCenterPixel;
  PixelScaleCalibration pixelScale;
  MechanicalPose currentMachinePose;
};

struct OriginCalibrationResult {
  OriginCalibration calibration;
  MechanicalPose correctedPose;
  PixelPoint pixelOffset;
  MillimeterPoint millimeterOffset;
};

class OriginCalibrator {
public:
  Result<OriginCalibrationResult> calibrate(const OriginCalibrationInput &input) const;
};

} // namespace calibration

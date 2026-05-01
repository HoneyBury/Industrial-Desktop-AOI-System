#include "calibration/PixelScaleCalibrator.h"

#include <cmath>

namespace calibration {

Result<PixelScaleCalibration> PixelScaleCalibrator::calibrateFromKnownDistance(
    const double pixelDistanceX, const double pixelDistanceY, const double physicalDistanceXmm,
    const double physicalDistanceYmm) const {
  if (std::abs(pixelDistanceX) < 1e-9 || std::abs(pixelDistanceY) < 1e-9) {
    return Result<PixelScaleCalibration>::failure("Pixel distance must be non-zero.");
  }

  PixelScaleCalibration calibration;
  calibration.calibrated = true;
  calibration.pixelToMillimeterX = physicalDistanceXmm / pixelDistanceX;
  calibration.pixelToMillimeterY = physicalDistanceYmm / pixelDistanceY;
  return Result<PixelScaleCalibration>::success(calibration, "Pixel scale calibration completed.");
}

} // namespace calibration

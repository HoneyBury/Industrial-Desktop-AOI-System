#include "calibration/LaserOffsetCalibrator.h"

#include "coordinate/CoordinateTransformer.h"

namespace calibration {

Result<LaserOffsetCalibrationResult> LaserOffsetCalibrator::calibrate(
    const LaserOffsetCalibrationInput &input) const {
  CoordinateTransformer transformer(input.pixelScale.pixelToMillimeterX, input.pixelScale.pixelToMillimeterY);

  const PixelPoint pixelOffset {input.laserCrossPixel.x - input.opticalCenterPixel.x,
                                input.laserCrossPixel.y - input.opticalCenterPixel.y};
  const MillimeterPoint millimeterOffset = transformer.pixelToMillimeter(pixelOffset);

  LaserOffsetCalibrationResult result;
  result.pixelOffset = pixelOffset;
  result.millimeterOffset = millimeterOffset;
  result.calibration.calibrated = true;
  result.calibration.cameraToLaserDxMm = millimeterOffset.x;
  result.calibration.cameraToLaserDyMm = millimeterOffset.y;
  return Result<LaserOffsetCalibrationResult>::success(result, "Laser offset calibration completed.");
}

} // namespace calibration

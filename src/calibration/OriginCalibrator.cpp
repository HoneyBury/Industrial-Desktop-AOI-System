#include "calibration/OriginCalibrator.h"

#include "coordinate/CoordinateTransformer.h"

namespace calibration {

Result<OriginCalibrationResult> OriginCalibrator::calibrate(const OriginCalibrationInput &input) const {
  CoordinateTransformer transformer(input.pixelScale.pixelToMillimeterX, input.pixelScale.pixelToMillimeterY);

  const PixelPoint pixelOffset {input.measuredReferencePixel.x - input.opticalCenterPixel.x,
                                input.measuredReferencePixel.y - input.opticalCenterPixel.y};
  const MillimeterPoint millimeterOffset = transformer.pixelToMillimeter(pixelOffset);

  OriginCalibrationResult result;
  result.pixelOffset = pixelOffset;
  result.millimeterOffset = millimeterOffset;
  result.correctedPose = input.currentMachinePose;
  result.correctedPose.x -= millimeterOffset.x;
  result.correctedPose.y -= millimeterOffset.y;
  result.calibration.calibrated = true;
  result.calibration.imageReferencePixel = input.measuredReferencePixel;
  result.calibration.machineReferencePose = result.correctedPose;
  return Result<OriginCalibrationResult>::success(result, "Origin calibration completed.");
}

} // namespace calibration

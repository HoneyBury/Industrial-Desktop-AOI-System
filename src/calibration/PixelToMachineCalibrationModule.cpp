#include "calibration/PixelToMachineCalibrationModule.h"

namespace calibration {

void PixelToMachineCalibrationModule::setPixelScale(const PixelScaleCalibration &scale) {
  pixelScale_ = scale;
}

void PixelToMachineCalibrationModule::inferFromFov(double fovWidthMm, double fovHeightMm,
                                                    int imageWidthPx, int imageHeightPx) {
  if (imageWidthPx > 0) {
    pixelScale_.pixelToMillimeterX = fovWidthMm / static_cast<double>(imageWidthPx);
  }
  if (imageHeightPx > 0) {
    pixelScale_.pixelToMillimeterY = fovHeightMm / static_cast<double>(imageHeightPx);
  }
  pixelScale_.calibrated = true;
}

MillimeterPoint PixelToMachineCalibrationModule::pixelOffsetToMm(double dxPx, double dyPx) const {
  return {dxPx * pixelScale_.pixelToMillimeterX,
          dyPx * pixelScale_.pixelToMillimeterY};
}

PixelPoint PixelToMachineCalibrationModule::mmToPixelOffset(double dxMm, double dyMm) const {
  if (pixelScale_.pixelToMillimeterX <= 0.0 || pixelScale_.pixelToMillimeterY <= 0.0) {
    return {0.0, 0.0};
  }
  return {dxMm / pixelScale_.pixelToMillimeterX,
          dyMm / pixelScale_.pixelToMillimeterY};
}

} // namespace calibration

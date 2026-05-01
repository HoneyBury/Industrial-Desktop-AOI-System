#include "calibration/LaserOffsetCalibrationModule.h"

namespace calibration {

void LaserOffsetCalibrationModule::setPixelScale(const PixelScaleCalibration &scale) {
  pixelScale_ = scale;
}

void LaserOffsetCalibrationModule::recordCameraPoint(const MechanicalPose &pose) {
  cameraReferencePose_ = pose;
  hasCameraPoint_ = true;
}

void LaserOffsetCalibrationModule::recordLaserPoint(const MechanicalPose &pose) {
  laserReferencePose_ = pose;
  hasLaserPoint_ = true;
}

LaserOffsetCalibration LaserOffsetCalibrationModule::computeOffset() {
  if (!hasCameraPoint_ || !hasLaserPoint_) {
    return LaserOffsetCalibration{};
  }

  LaserOffsetCalibration result;
  result.cameraToLaserDxMm = laserReferencePose_.x - cameraReferencePose_.x;
  result.cameraToLaserDyMm = laserReferencePose_.y - cameraReferencePose_.y;
  result.calibrated = true;

  computedCalibration_ = result;
  hasComputedResult_ = true;
  hasUnappliedResult_ = true;

  return result;
}

LaserOffsetCalibration LaserOffsetCalibrationModule::apply() {
  if (!hasComputedResult_) {
    return LaserOffsetCalibration{};
  }
  hasUnappliedResult_ = false;
  return computedCalibration_;
}

void LaserOffsetCalibrationModule::clear() {
  hasCameraPoint_ = false;
  hasLaserPoint_ = false;
  hasComputedResult_ = false;
  hasUnappliedResult_ = false;
  cameraReferencePose_ = MechanicalPose{};
  laserReferencePose_ = MechanicalPose{};
  computedCalibration_ = LaserOffsetCalibration{};
}

} // namespace calibration

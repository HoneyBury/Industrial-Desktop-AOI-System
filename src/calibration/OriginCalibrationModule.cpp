#include "calibration/OriginCalibrationModule.h"

namespace calibration {

void OriginCalibrationModule::setBoardDefinition(double boardLengthMm, double boardWidthMm) {
  boardLengthMm_ = boardLengthMm;
  boardWidthMm_ = boardWidthMm;
}

void OriginCalibrationModule::setReferencePose(const MechanicalPose &pose) {
  referenceMachinePose_ = pose;
  hasReference_ = true;
  hasUnappliedChanges_ = true;
}

void OriginCalibrationModule::setReferencePixel(double px, double py) {
  referencePixelPx_ = px;
  referencePixelPy_ = py;
}

MechanicalPose OriginCalibrationModule::calculateLogicalOrigin() const {
  if (!hasReference_) {
    return referenceMachinePose_;
  }
  MechanicalPose origin = referenceMachinePose_;
  origin.x -= boardLengthMm_;
  origin.y -= boardWidthMm_;
  return origin;
}

OriginCalibration OriginCalibrationModule::apply() {
  OriginCalibration cal;
  if (!hasReference_) {
    return cal;
  }
  cal.machineReferencePose = referenceMachinePose_;
  cal.imageReferencePixel = {referencePixelPx_, referencePixelPy_};
  cal.calibrated = true;
  hasUnappliedChanges_ = false;
  return cal;
}

void OriginCalibrationModule::reset() {
  hasReference_ = false;
  hasUnappliedChanges_ = false;
  referenceMachinePose_ = MechanicalPose{};
  referencePixelPx_ = 0.0;
  referencePixelPy_ = 0.0;
}

} // namespace calibration

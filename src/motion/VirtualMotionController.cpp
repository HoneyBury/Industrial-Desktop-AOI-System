#include "motion/VirtualMotionController.h"

namespace {

std::unordered_map<MotionAxis, double> defaultAxes() {
  return {
      {MotionAxis::X, 0.0},
      {MotionAxis::Y, 0.0},
      {MotionAxis::Z, 0.0},
      {MotionAxis::R, 0.0},
  };
}

} // namespace

VirtualMotionController::VirtualMotionController() : axisPositions_(defaultAxes()) {}

bool VirtualMotionController::home(const MotionAxis axis) {
  if (emergencyStopped_) {
    return false;
  }

  axisPositions_[axis] = 0.0;
  return true;
}

bool VirtualMotionController::moveAbsolute(const MotionAxis axis, const double targetPosition) {
  if (emergencyStopped_) {
    return false;
  }

  axisPositions_[axis] = targetPosition;
  return true;
}

bool VirtualMotionController::moveRelative(const MotionAxis axis, const double delta) {
  if (emergencyStopped_) {
    return false;
  }

  axisPositions_[axis] += delta;
  return true;
}

void VirtualMotionController::emergencyStop() { emergencyStopped_ = true; }

bool VirtualMotionController::isStopped() const { return emergencyStopped_; }

std::optional<double> VirtualMotionController::position(const MotionAxis axis) const {
  const auto iterator = axisPositions_.find(axis);
  if (iterator == axisPositions_.end()) {
    return std::nullopt;
  }

  return iterator->second;
}


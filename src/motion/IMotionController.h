#pragma once

#include "motion/MotionAxis.h"

#include <optional>

class IMotionController {
public:
  virtual ~IMotionController() = default;

  virtual bool home(MotionAxis axis) = 0;
  virtual bool moveAbsolute(MotionAxis axis, double targetPosition) = 0;
  virtual bool moveRelative(MotionAxis axis, double delta) = 0;
  virtual void emergencyStop() = 0;
  virtual void resetEmergencyStop() = 0;
  virtual bool isStopped() const = 0;
  virtual std::optional<double> position(MotionAxis axis) const = 0;
};

#pragma once

#include "motion/IMotionController.h"

#include <unordered_map>

class VirtualMotionController final : public IMotionController {
public:
  VirtualMotionController();

  bool home(MotionAxis axis) override;
  bool moveAbsolute(MotionAxis axis, double targetPosition) override;
  bool moveRelative(MotionAxis axis, double delta) override;
  void emergencyStop() override;
  bool isStopped() const override;
  std::optional<double> position(MotionAxis axis) const override;

private:
  std::unordered_map<MotionAxis, double> axisPositions_;
  bool emergencyStopped_ {false};
};


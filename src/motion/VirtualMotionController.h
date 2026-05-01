#pragma once

#include "motion/IMotionController.h"

#include <unordered_map>

/// 虚拟轴模型 — 包含位置、目标、速度、加速度、限位和状态
struct VirtualAxis {
  double currentPos = 0.0;
  double targetPos = 0.0;
  double speed = 100.0;          // mm/s
  double acceleration = 500.0;   // mm/s²
  double softLimitMin = -500.0;
  double softLimitMax = 500.0;
  AxisState state = AxisState::Idle;
};

class VirtualMotionController final : public IMotionController {
public:
  VirtualMotionController();

  // ── 新 API 实现 ──
  bool initialize() override;
  bool homeAll() override;
  bool homeAxis(MotionAxis axis) override;
  bool moveAbs(MotionAxis axis, double position, double speed = 100.0) override;
  bool moveRel(MotionAxis axis, double distance, double speed = 100.0) override;
  bool stopAxis(MotionAxis axis) override;
  bool stopAll() override;
  void emergencyStop() override;
  bool resetAlarm() override;
  void resetEmergencyStop() override;
  bool isStopped() const override;
  double getAxisPosition(MotionAxis axis) const override;
  AxisState getAxisState(MotionAxis axis) const override;
  void setAxisDoneCallback(AxisDoneCallback callback) override;
  void tick(double deltaSec) override;

private:
  std::unordered_map<MotionAxis, VirtualAxis> axes_;
  AxisDoneCallback axisDoneCallback_;
  bool alarm_ {false};
  bool emergencyStopped_ {false};
};

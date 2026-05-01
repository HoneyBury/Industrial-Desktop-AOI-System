#include "motion/VirtualMotionController.h"

#include <algorithm>
#include <cmath>

namespace {

std::unordered_map<MotionAxis, VirtualAxis> defaultAxes() {
  return {
      {MotionAxis::Conveyor, VirtualAxis{}},
      {MotionAxis::Stopper, VirtualAxis{}},
      {MotionAxis::CameraX, VirtualAxis{}},
      {MotionAxis::CameraY, VirtualAxis{}},
      {MotionAxis::LaserX, VirtualAxis{}},
      {MotionAxis::LaserY, VirtualAxis{}},
      {MotionAxis::Z, VirtualAxis{}},
      {MotionAxis::R, VirtualAxis{}},
  };
}

/// 梯形加减速：给定当前位置、目标、速度、加速度和时间步长，返回到达的新位置
double trapezoidStep(double current, double target, double speed, double accel, double dt) {
  const double distance = target - current;
  if (std::abs(distance) < 1e-6) {
    return target;
  }

  const double dir = (distance > 0) ? 1.0 : -1.0;

  // 加速到最大速度所需距离：v²/(2a)
  const double accelDist = (speed * speed) / (2.0 * accel);

  // 简化：匀速段处理
  const double maxStep = speed * dt;
  const double step = std::min(maxStep, std::abs(distance));

  return current + dir * step;
}

} // namespace

VirtualMotionController::VirtualMotionController() : axes_(defaultAxes()) {}

bool VirtualMotionController::initialize() {
  axes_ = defaultAxes();
  alarm_ = false;
  emergencyStopped_ = false;
  return true;
}

bool VirtualMotionController::homeAll() {
  if (emergencyStopped_) return false;

  for (auto &pair : axes_) {
    pair.second.currentPos = 0.0;
    pair.second.targetPos = 0.0;
    pair.second.state = AxisState::Homing;
  }
  return true;
}

bool VirtualMotionController::homeAxis(const MotionAxis axis) {
  if (emergencyStopped_) return false;

  auto it = axes_.find(axis);
  if (it == axes_.end()) return false;

  it->second.currentPos = 0.0;
  it->second.targetPos = 0.0;
  it->second.state = AxisState::Done;
  return true;
}

bool VirtualMotionController::moveAbs(const MotionAxis axis, const double position, const double speed) {
  if (emergencyStopped_ || alarm_) return false;

  auto it = axes_.find(axis);
  if (it == axes_.end()) return false;

  // 软限位检查
  if (position < it->second.softLimitMin || position > it->second.softLimitMax) {
    it->second.state = AxisState::Alarm;
    alarm_ = true;
    return false;
  }

  it->second.targetPos = position;
  it->second.speed = speed;
  it->second.state = AxisState::Moving;
  return true;
}

bool VirtualMotionController::moveRel(const MotionAxis axis, const double distance, const double speed) {
  const double current = getAxisPosition(axis);
  return moveAbs(axis, current + distance, speed);
}

bool VirtualMotionController::stopAxis(const MotionAxis axis) {
  auto it = axes_.find(axis);
  if (it == axes_.end()) return false;

  it->second.targetPos = it->second.currentPos;
  it->second.state = AxisState::Idle;
  return true;
}

bool VirtualMotionController::stopAll() {
  for (auto &pair : axes_) {
    pair.second.targetPos = pair.second.currentPos;
    pair.second.state = AxisState::Idle;
  }
  return true;
}

void VirtualMotionController::emergencyStop() {
  emergencyStopped_ = true;
  for (auto &pair : axes_) {
    pair.second.state = AxisState::EmergencyStopped;
    pair.second.targetPos = pair.second.currentPos;
  }
}

bool VirtualMotionController::resetAlarm() {
  if (emergencyStopped_) return false;

  alarm_ = false;
  for (auto &pair : axes_) {
    if (pair.second.state == AxisState::Alarm) {
      pair.second.state = AxisState::Idle;
    }
  }
  return true;
}

void VirtualMotionController::resetEmergencyStop() {
  emergencyStopped_ = false;
  alarm_ = false;
  for (auto &pair : axes_) {
    pair.second.state = AxisState::Idle;
  }
}

bool VirtualMotionController::isStopped() const { return emergencyStopped_; }

double VirtualMotionController::getAxisPosition(const MotionAxis axis) const {
  const auto it = axes_.find(axis);
  if (it == axes_.end()) return 0.0;
  return it->second.currentPos;
}

AxisState VirtualMotionController::getAxisState(const MotionAxis axis) const {
  const auto it = axes_.find(axis);
  if (it == axes_.end()) return AxisState::Idle;
  return it->second.state;
}

void VirtualMotionController::setAxisDoneCallback(AxisDoneCallback callback) {
  axisDoneCallback_ = std::move(callback);
}

void VirtualMotionController::tick(const double deltaSec) {
  if (emergencyStopped_) return;

  for (auto &pair : axes_) {
    auto &axis = pair.second;
    if (axis.state != AxisState::Moving && axis.state != AxisState::Homing) {
      continue;
    }

    const double before = axis.currentPos;
    axis.currentPos = trapezoidStep(axis.currentPos, axis.targetPos, axis.speed, axis.acceleration, deltaSec);

    // 检查是否到达目标
    if (std::abs(axis.currentPos - axis.targetPos) < 1e-3) {
      axis.currentPos = axis.targetPos;
      axis.state = AxisState::Done;
      if (axisDoneCallback_) {
        axisDoneCallback_(pair.first);
      }
    }
  }
}

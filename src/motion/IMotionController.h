#pragma once

#include "motion/MotionAxis.h"

#include <functional>
#include <optional>

/// 工业运动控制器抽象接口。
///
/// 分为两组：
/// 1. 新 API — 面向状态机/流程引擎，包含速度、轴状态、回调、tick
/// 2. 旧 API — 向后兼容现有调用方，基于新 API 提供默认实现
class IMotionController {
public:
  virtual ~IMotionController() = default;

  // ── 新 API ──────────────────────────────────────────────

  virtual bool initialize() = 0;
  virtual bool homeAll() = 0;
  virtual bool homeAxis(MotionAxis axis) = 0;

  /// @param speed mm/s，默认 100.0
  virtual bool moveAbs(MotionAxis axis, double position, double speed = 100.0) = 0;
  virtual bool moveRel(MotionAxis axis, double distance, double speed = 100.0) = 0;

  virtual bool stopAxis(MotionAxis axis) = 0;
  virtual bool stopAll() = 0;

  virtual void emergencyStop() = 0;
  virtual bool resetAlarm() = 0;
  virtual void resetEmergencyStop() = 0;

  virtual bool isStopped() const = 0;

  virtual double getAxisPosition(MotionAxis axis) const = 0;
  virtual AxisState getAxisState(MotionAxis axis) const = 0;

  // 轴运动完成回调
  using AxisDoneCallback = std::function<void(MotionAxis)>;
  virtual void setAxisDoneCallback(AxisDoneCallback callback) = 0;

  // 定时更新（由外部 Tick 驱动，一般 60 Hz）
  virtual void tick(double deltaSec) = 0;

  // ── 向后兼容旧 API ─────────────────────────────────────

  /// @deprecated 使用 homeAxis() 替代
  bool home(MotionAxis axis) { return homeAxis(axis); }

  /// @deprecated 使用 moveAbs() 替代
  bool moveAbsolute(MotionAxis axis, double targetPosition) { return moveAbs(axis, targetPosition); }

  /// @deprecated 使用 moveRel() 替代
  bool moveRelative(MotionAxis axis, double delta) { return moveRel(axis, delta); }

  /// @deprecated 使用 getAxisPosition() 替代
  std::optional<double> position(MotionAxis axis) const {
    return std::optional<double>(getAxisPosition(axis));
  }
};

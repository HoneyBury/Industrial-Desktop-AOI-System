#pragma once

#include "calibration/CalibrationTypes.h"

namespace calibration {

/// 镭射偏移校正模块 — 独立于 Qt 的业务逻辑
///
/// 职责：
///   - 管理镭射偏移校正的完整状态
///   - 记录相机参考点和镭射参考点
///   - 计算偏移量 cameraToLaserDx/Dy
///   - 提供 apply / clear 接口
///
/// 使用流程：
///   1. setPixelScale(scale)
///   2. recordCameraPoint(pose)
///   3. recordLaserPoint(pose)
///   4. computeOffset() → LaserOffsetCalibration
///   5. apply() → LaserOffsetCalibration
///   6. clear()
class LaserOffsetCalibrationModule {
public:
  void setPixelScale(const PixelScaleCalibration &scale);

  void recordCameraPoint(const MechanicalPose &pose);
  void recordLaserPoint(const MechanicalPose &pose);

  [[nodiscard]] bool hasCameraPoint() const { return hasCameraPoint_; }
  [[nodiscard]] bool hasLaserPoint() const { return hasLaserPoint_; }
  [[nodiscard]] bool hasComputedResult() const { return hasComputedResult_; }
  [[nodiscard]] bool hasUnappliedResult() const { return hasUnappliedResult_; }

  [[nodiscard]] MechanicalPose cameraReferencePose() const { return cameraReferencePose_; }
  [[nodiscard]] MechanicalPose laserReferencePose() const { return laserReferencePose_; }
  [[nodiscard]] LaserOffsetCalibration computedResult() const { return computedCalibration_; }

  /// 计算机械偏移量 cameraToLaserDx/Dy
  [[nodiscard]] LaserOffsetCalibration computeOffset();

  /// 标记已应用，返回结果
  [[nodiscard]] LaserOffsetCalibration apply();

  /// 清除所有状态
  void clear();

private:
  PixelScaleCalibration pixelScale_;

  bool hasCameraPoint_ {false};
  bool hasLaserPoint_ {false};
  bool hasComputedResult_ {false};
  bool hasUnappliedResult_ {false};
  MechanicalPose cameraReferencePose_;
  MechanicalPose laserReferencePose_;
  LaserOffsetCalibration computedCalibration_;
};

} // namespace calibration

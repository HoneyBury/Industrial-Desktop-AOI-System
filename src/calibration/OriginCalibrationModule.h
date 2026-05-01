#pragma once

#include "calibration/CalibrationTypes.h"

namespace calibration {

/// 原点校正模块 — 独立于 Qt 的业务逻辑
///
/// 职责：
///   - 管理原点校正的完整状态（参考位、逻辑原点、未应用变更）
///   - 提供参考位到逻辑原点的换算
///   - 提供 apply / reset 接口
///
/// 使用流程：
///   1. setBoardDefinition(length, width)
///   2. setReferencePose / setReferencePixel
///   3. calculateLogicalOrigin()
///   4. apply() → OriginCalibration
///   5. reset() / hasUnappliedChanges()
class OriginCalibrationModule {
public:
  void setBoardDefinition(double boardLengthMm, double boardWidthMm);
  void setReferencePose(const MechanicalPose &pose);
  void setReferencePixel(double px, double py);

  [[nodiscard]] bool hasReference() const { return hasReference_; }
  [[nodiscard]] bool hasUnappliedChanges() const { return hasUnappliedChanges_; }

  [[nodiscard]] MechanicalPose referenceMachinePose() const { return referenceMachinePose_; }
  [[nodiscard]] double referencePixelPx() const { return referencePixelPx_; }
  [[nodiscard]] double referencePixelPy() const { return referencePixelPy_; }

  /// 参考位 → 逻辑原点（参考位 - 板尺寸）
  [[nodiscard]] MechanicalPose calculateLogicalOrigin() const;

  /// 应用校正，返回 OriginCalibration 并标记已应用
  [[nodiscard]] OriginCalibration apply();

  /// 重置本次状态
  void reset();

private:
  double boardLengthMm_ {0.0};
  double boardWidthMm_ {0.0};

  bool hasReference_ {false};
  bool hasUnappliedChanges_ {false};
  MechanicalPose referenceMachinePose_;
  double referencePixelPx_ {0.0};
  double referencePixelPy_ {0.0};
};

} // namespace calibration

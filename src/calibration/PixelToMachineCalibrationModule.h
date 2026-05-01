#pragma once

#include "calibration/CalibrationTypes.h"

namespace calibration {

/// 像素到机械坐标换算模块
///
/// 职责：
///   - 管理 pixelToMmX / pixelToMmY 参数
///   - 提供像素偏移 → 机械移动量的换算
///   - 支持从 FOV 物理尺寸和图像分辨率自动推算默认比例
class PixelToMachineCalibrationModule {
public:
  void setPixelScale(const PixelScaleCalibration &scale);
  [[nodiscard]] PixelScaleCalibration pixelScale() const { return pixelScale_; }

  /// 从 FOV 物理尺寸和图像分辨率推算像素比例
  void inferFromFov(double fovWidthMm, double fovHeightMm,
                    int imageWidthPx, int imageHeightPx);

  /// 像素偏移 → 机械移动量 (mm)
  [[nodiscard]] MillimeterPoint pixelOffsetToMm(double dxPx, double dyPx) const;

  /// 机械移动量 → 像素偏移
  [[nodiscard]] PixelPoint mmToPixelOffset(double dxMm, double dyMm) const;

  [[nodiscard]] bool isCalibrated() const {
    return pixelScale_.calibrated && pixelScale_.pixelToMillimeterX > 0.0;
  }

private:
  PixelScaleCalibration pixelScale_;
};

} // namespace calibration

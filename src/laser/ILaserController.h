#pragma once

#include <string>
#include <vector>

#ifdef AOI_HAS_OPENCV
#include <opencv2/core/mat.hpp>
#endif

struct LaserMarkingParams {
  double x {0.0};
  double y {0.0};
  double powerPercent {80.0};
  double frequencyKhz {20.0};
  double pulseWidthUs {10.0};
  int repeatCount {1};
};

/// 工业激光控制器抽象接口。
///
/// 功能包括三大类：
/// 1. 标准打标（executeMark）        — 向后兼容现有流程
/// 2. 视觉辅助标记（markCross/markQRCode/markText） — 虚拟镭射可视化
/// 3. 设备状态（急停/复位/就绪）
class ILaserController {
public:
  virtual ~ILaserController() = default;

  // ── 初始化和设备状态 ──────────────────────────

  virtual bool initialize() = 0;
  virtual bool isReady() const = 0;

  // ── 标准打标（向后兼容） ──────────────────────

  virtual bool executeMark(const LaserMarkingParams &params) = 0;

  // ── 视觉辅助标记（Demo / 镭射可视化）──────────

  /// 在指定位置（mm）绘制十字标记，用于激光偏移校正
  virtual bool markCross(double xMm, double yMm, double sizeMm) = 0;

  /// 在指定位置（mm）生成 QR 码标记
  virtual bool markQRCode(double xMm, double yMm, const std::string &content, double sizeMm) = 0;

  /// 在指定位置（mm）绘制文字
  virtual bool markText(double xMm, double yMm, const std::string &text) = 0;

  // ── 安全 ────────────────────────────────────────

  virtual void emergencyStop() = 0;
  virtual void resetEmergencyStop() = 0;
  virtual bool isStopped() const = 0;

  // ── 输出 ────────────────────────────────────────

  virtual std::string lastMarkReport() const = 0;

#ifdef AOI_HAS_OPENCV
  /// 获取最后一次打标/标记后的输出图像
  virtual cv::Mat getLastMarkedImage() const = 0;
#endif
};

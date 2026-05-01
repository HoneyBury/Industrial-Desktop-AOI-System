#pragma once

#include "laser/ILaserController.h"

#include <string>

#ifdef AOI_HAS_OPENCV
#include <opencv2/core/mat.hpp>
#endif

class VirtualLaserController final : public ILaserController {
public:
  VirtualLaserController();

  // ── ILaserController 实现 ──
  bool initialize() override;
  bool isReady() const override;
  bool executeMark(const LaserMarkingParams &params) override;

  bool markCross(double xMm, double yMm, double sizeMm) override;
  bool markQRCode(double xMm, double yMm, const std::string &content, double sizeMm) override;
  bool markText(double xMm, double yMm, const std::string &text) override;

  void emergencyStop() override;
  void resetEmergencyStop() override;
  bool isStopped() const override;
  std::string lastMarkReport() const override;

#ifdef AOI_HAS_OPENCV
  cv::Mat getLastMarkedImage() const override;
#endif

  /// 设置用于可视化的背景图像
#ifdef AOI_HAS_OPENCV
  void setBackgroundImage(const cv::Mat &image);
#endif

private:
  bool ready_ {true};
  bool stopped_ {false};
  std::string lastReport_;

#ifdef AOI_HAS_OPENCV
  cv::Mat markedImage_;
#endif
};

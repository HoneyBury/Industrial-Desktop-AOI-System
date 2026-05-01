#pragma once

#ifdef AOI_HAS_OPENCV

#include "camera/ICameraProvider.h"

#include <opencv2/core/mat.hpp>

/// Demo 图片相机 — 从大尺寸测试板图片按当前虚拟相机位置裁剪视野。
///
/// 模拟真实相机随轴移动观察不同区域的效果：
/// - 加载一张全板大图（如 4000×3000 px）
/// - 根据 setCameraPosition() 设置的 X/Y，从大图裁剪固定大小窗口
/// - 可选添加噪声、偏移、旋转
class DemoImageCameraProvider final : public ICameraProvider {
public:
  DemoImageCameraProvider() = default;

  bool open() override;
  void close() override;
  bool isOpen() const override;
  cv::Mat capture() override;
  std::string providerName() const override;

  /// 设置当前相机在板面上的位置（mm），决定裁剪区域
  void setCameraPosition(double xMm, double yMm);

  /// 设置视野大小（像素）
  void setFovSize(int width, int height);

  /// 加载测试板大图
  bool loadBoardImage(const std::string &imagePath);

  /// @name 模拟噪声与偏差
  /// @{
  void setNoiseStdDev(double stdDev);
  void setOffset(double dxMm, double dyMm);
  void setRotationDeg(double degrees);
  /// @}

private:
  cv::Mat boardImage_;
  double cameraXMm_ = 0.0;
  double cameraYMm_ = 0.0;
  int fovWidth_ = 640;
  int fovHeight_ = 480;
  double pixelPerMm_ = 10.0; // 图像像素/mm 比例
  double noiseStdDev_ = 0.0;
  double offsetDxMm_ = 0.0;
  double offsetDyMm_ = 0.0;
  double rotationDeg_ = 0.0;
  bool open_ = false;
};

#endif

#ifdef AOI_HAS_OPENCV

#include "camera/DemoImageCameraProvider.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

bool DemoImageCameraProvider::open() {
  open_ = !boardImage_.empty();
  return open_;
}

void DemoImageCameraProvider::close() { open_ = false; }

bool DemoImageCameraProvider::isOpen() const { return open_; }

std::string DemoImageCameraProvider::providerName() const { return "DemoImageCamera"; }

bool DemoImageCameraProvider::loadBoardImage(const std::string &imagePath) {
  boardImage_ = cv::imread(imagePath, cv::IMREAD_COLOR);
  if (boardImage_.empty()) return false;

  // 根据图片尺寸估算 pixelPerMm：假设板宽约 400 mm
  pixelPerMm_ = static_cast<double>(boardImage_.cols) / 400.0;
  return true;
}

void DemoImageCameraProvider::setCameraPosition(const double xMm, const double yMm) {
  cameraXMm_ = xMm;
  cameraYMm_ = yMm;
}

void DemoImageCameraProvider::setFovSize(const int width, const int height) {
  fovWidth_ = width;
  fovHeight_ = height;
}

void DemoImageCameraProvider::setNoiseStdDev(const double stdDev) { noiseStdDev_ = stdDev; }

void DemoImageCameraProvider::setOffset(const double dxMm, const double dyMm) {
  offsetDxMm_ = dxMm;
  offsetDyMm_ = dyMm;
}

void DemoImageCameraProvider::setRotationDeg(const double degrees) { rotationDeg_ = degrees; }

cv::Mat DemoImageCameraProvider::capture() {
  if (boardImage_.empty()) return {};

  // 像素坐标
  const int px = static_cast<int>((cameraXMm_ + offsetDxMm_) * pixelPerMm_);
  const int py = static_cast<int>((cameraYMm_ + offsetDyMm_) * pixelPerMm_);

  cv::Rect roi(px, py, fovWidth_, fovHeight_);
  roi &= cv::Rect(0, 0, boardImage_.cols, boardImage_.rows);

  if (roi.width <= 0 || roi.height <= 0) return {};

  cv::Mat cropped = boardImage_(roi).clone();

  // 模拟旋转
  if (std::abs(rotationDeg_) > 0.01) {
    const cv::Point2f center(static_cast<float>(cropped.cols) / 2.0f,
                              static_cast<float>(cropped.rows) / 2.0f);
    const cv::Mat rotMat = cv::getRotationMatrix2D(center, rotationDeg_, 1.0);
    cv::warpAffine(cropped, cropped, rotMat, cropped.size());
  }

  // 模拟噪声
  if (noiseStdDev_ > 0.0) {
    cv::Mat noise(cropped.size(), CV_8UC3);
    cv::randn(noise, 0.0, noiseStdDev_);
    cropped += noise;
  }

  return cropped;
}

#endif

#pragma once

#ifdef AOI_HAS_OPENCV

#include "camera/ICameraProvider.h"

#include <opencv2/videoio.hpp>

/// Mac 笔记本自带摄像头，通过 OpenCV VideoCapture 接入。
class MacCameraProvider final : public ICameraProvider {
public:
  explicit MacCameraProvider(int deviceIndex = 0);
  ~MacCameraProvider() override;

  bool open() override;
  void close() override;
  bool isOpen() const override;
  cv::Mat capture() override;
  std::string providerName() const override;

private:
  int deviceIndex_;
  cv::VideoCapture capture_;
};

#endif

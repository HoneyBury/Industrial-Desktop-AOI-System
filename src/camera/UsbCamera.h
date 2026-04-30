#pragma once

#include "camera/ICamera.h"

#ifdef AOI_HAS_OPENCV
#include <opencv2/videoio.hpp>
#endif

class UsbCamera final : public ICamera {
public:
  void setPreferredFrameSize(int width, int height);
  bool open(int index) override;
  void close() override;
  bool isOpened() const override;
  CameraFrame grabFrame() override;
  std::string cameraName() const override;

private:
  int deviceIndex_ {-1};
  int preferredFrameWidth_ {640};
  int preferredFrameHeight_ {360};
#ifndef AOI_HAS_OPENCV
  std::size_t stubFrameCounter_ {0};
#endif
#ifdef AOI_HAS_OPENCV
  cv::VideoCapture capture_;
#endif
};

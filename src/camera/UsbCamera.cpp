#include "camera/UsbCamera.h"

#include "common/Logger.h"

#ifdef AOI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif

bool UsbCamera::open(const int index) {
  deviceIndex_ = index;

#ifdef AOI_HAS_OPENCV
  if (capture_.open(index)) {
    Logger::info("UsbCamera opened device index " + std::to_string(index));
    return true;
  }

  Logger::warning("UsbCamera failed to open device index " + std::to_string(index));
  return false;
#else
  Logger::warning("OpenCV unavailable, UsbCamera runs in stub mode.");
  return index >= 0;
#endif
}

void UsbCamera::close() {
#ifdef AOI_HAS_OPENCV
  if (capture_.isOpened()) {
    capture_.release();
  }
#endif
}

bool UsbCamera::isOpened() const {
#ifdef AOI_HAS_OPENCV
  return capture_.isOpened();
#else
  return deviceIndex_ >= 0;
#endif
}

CameraFrame UsbCamera::grabFrame() {
#ifdef AOI_HAS_OPENCV
  cv::Mat frame;
  if (!capture_.read(frame) || frame.empty()) {
    return {};
  }

  CameraFrame result;
  result.width = frame.cols;
  result.height = frame.rows;
  result.channels = frame.channels();
  result.data.assign(frame.datastart, frame.dataend);
  return result;
#else
  return {};
#endif
}

std::string UsbCamera::cameraName() const { return "Mac Webcam / USB Camera"; }


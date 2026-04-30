#include "camera/UsbCamera.h"

#include "common/Logger.h"

#include <cmath>

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
  // 无 OpenCV 时仅保留接口行为，方便 UI、流程和测试先联通。
  stubFrameCounter_ = 0;
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

  deviceIndex_ = -1;
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
  result.pixelFormat = frame.channels() == 1 ? CameraPixelFormat::Gray8 : CameraPixelFormat::Bgr24;
  result.data.assign(frame.datastart, frame.dataend);
  return result;
#else
  if (!isOpened()) {
    return {};
  }

  constexpr int width = 640;
  constexpr int height = 360;
  constexpr int channels = 3;

  CameraFrame frame;
  frame.width = width;
  frame.height = height;
  frame.channels = channels;
  frame.pixelFormat = CameraPixelFormat::Rgb24;
  frame.data.resize(static_cast<std::size_t>(width * height * channels));

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const std::size_t offset = static_cast<std::size_t>((y * width + x) * channels);
      const std::uint8_t red = static_cast<std::uint8_t>((x + static_cast<int>(stubFrameCounter_)) % 256);
      const std::uint8_t green =
          static_cast<std::uint8_t>((y * 2 + static_cast<int>(stubFrameCounter_ * 3)) % 256);
      const std::uint8_t blue = static_cast<std::uint8_t>(
          (128 + static_cast<int>(60.0 * std::sin((x + stubFrameCounter_) * 0.03))) % 256);
      frame.data[offset] = red;
      frame.data[offset + 1] = green;
      frame.data[offset + 2] = blue;
    }
  }

  stubFrameCounter_ += 1;
  return frame;
#endif
}

std::string UsbCamera::cameraName() const { return "Mac Webcam / USB Camera"; }

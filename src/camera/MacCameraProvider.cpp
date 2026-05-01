#ifdef AOI_HAS_OPENCV

#include "camera/MacCameraProvider.h"

MacCameraProvider::MacCameraProvider(const int deviceIndex) : deviceIndex_(deviceIndex) {}

MacCameraProvider::~MacCameraProvider() { close(); }

bool MacCameraProvider::open() {
  if (capture_.isOpened()) return true;
  return capture_.open(deviceIndex_);
}

void MacCameraProvider::close() {
  if (capture_.isOpened()) capture_.release();
}

bool MacCameraProvider::isOpen() const { return capture_.isOpened(); }

cv::Mat MacCameraProvider::capture() {
  cv::Mat frame;
  if (capture_.isOpened()) capture_ >> frame;
  return frame;
}

std::string MacCameraProvider::providerName() const { return "MacCamera"; }

#endif

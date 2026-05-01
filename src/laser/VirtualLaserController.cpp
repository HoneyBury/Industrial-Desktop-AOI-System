#include "laser/VirtualLaserController.h"

#include <sstream>

#ifdef AOI_HAS_OPENCV
#include <opencv2/imgproc.hpp>
#endif

VirtualLaserController::VirtualLaserController() = default;

bool VirtualLaserController::initialize() {
  ready_ = true;
  stopped_ = false;
  lastReport_ = "Virtual laser initialized.";
  return true;
}

bool VirtualLaserController::isReady() const { return ready_ && !stopped_; }

bool VirtualLaserController::executeMark(const LaserMarkingParams &params) {
  if (stopped_) {
    lastReport_ = "Laser is emergency-stopped; cannot fire.";
    return false;
  }

  std::ostringstream ss;
  ss << "Virtual laser fired at (" << params.x << ", " << params.y << ")"
     << " power=" << params.powerPercent << "%"
     << " freq=" << params.frequencyKhz << "kHz"
     << " pulse=" << params.pulseWidthUs << "us"
     << " repeat=" << params.repeatCount;
  lastReport_ = ss.str();
  return true;
}

bool VirtualLaserController::markCross(const double xMm, const double yMm, const double sizeMm) {
  if (stopped_) return false;

  std::ostringstream ss;
  ss << "Cross marked at (" << xMm << ", " << yMm << ") size=" << sizeMm << "mm";
  lastReport_ = ss.str();

#ifdef AOI_HAS_OPENCV
  if (!markedImage_.empty()) {
    const int cx = static_cast<int>(xMm * 10.0);
    const int cy = static_cast<int>(yMm * 10.0);
    const int half = static_cast<int>(sizeMm * 10.0 / 2.0);
    cv::line(markedImage_, cv::Point(cx - half, cy), cv::Point(cx + half, cy), cv::Scalar(0, 0, 255), 2);
    cv::line(markedImage_, cv::Point(cx, cy - half), cv::Point(cx, cy + half), cv::Scalar(0, 0, 255), 2);
  }
#endif
  return true;
}

bool VirtualLaserController::markQRCode(const double xMm, const double yMm,
                                          const std::string &content, const double sizeMm) {
  if (stopped_) return false;

  std::ostringstream ss;
  ss << "QR code marked at (" << xMm << ", " << yMm << ") content='" << content << "' size=" << sizeMm << "mm";
  lastReport_ = ss.str();

#ifdef AOI_HAS_OPENCV
  if (!markedImage_.empty()) {
    const int cx = static_cast<int>(xMm * 10.0);
    const int cy = static_cast<int>(yMm * 10.0);
    const int half = static_cast<int>(sizeMm * 10.0 / 2.0);
    // 简化：绘制一个矩形占位符代表 QR 码
    cv::rectangle(markedImage_, cv::Rect(cx - half, cy - half, half * 2, half * 2),
                  cv::Scalar(0, 255, 0), 2);
    cv::putText(markedImage_, content, cv::Point(cx - half, cy - half - 5),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
  }
#endif
  return true;
}

bool VirtualLaserController::markText(const double xMm, const double yMm, const std::string &text) {
  if (stopped_) return false;

  std::ostringstream ss;
  ss << "Text marked at (" << xMm << ", " << yMm << ") content='" << text << "'";
  lastReport_ = ss.str();

#ifdef AOI_HAS_OPENCV
  if (!markedImage_.empty()) {
    const int px = static_cast<int>(xMm * 10.0);
    const int py = static_cast<int>(yMm * 10.0);
    cv::putText(markedImage_, text, cv::Point(px, py),
                cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 1);
  }
#endif
  return true;
}

void VirtualLaserController::emergencyStop() {
  stopped_ = true;
  ready_ = false;
}

void VirtualLaserController::resetEmergencyStop() {
  stopped_ = false;
  ready_ = true;
}

bool VirtualLaserController::isStopped() const { return stopped_; }

std::string VirtualLaserController::lastMarkReport() const { return lastReport_; }

#ifdef AOI_HAS_OPENCV
cv::Mat VirtualLaserController::getLastMarkedImage() const { return markedImage_.clone(); }

void VirtualLaserController::setBackgroundImage(const cv::Mat &image) {
  if (!image.empty()) {
    markedImage_ = image.clone();
  }
}
#endif

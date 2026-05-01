#include "vision/MarkDetector.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#ifdef AOI_HAS_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#endif

namespace {

#ifdef AOI_HAS_OPENCV
std::vector<MarkPoint> detectByContour(const cv::Mat &gray, const std::string &prefix) {
  std::vector<MarkPoint> results;

  cv::Mat blurred;
  cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 1.2);

  cv::Mat binary;
  cv::adaptiveThreshold(blurred, binary, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                        cv::THRESH_BINARY_INV, 15, 6);

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  const double minArea = 120.0;
  const double maxArea = gray.rows * gray.cols * 0.35;
  int markIndex = 0;

  for (const auto &contour : contours) {
    const double area = cv::contourArea(contour);
    if (area < minArea || area > maxArea) {
      continue;
    }

    const cv::RotatedRect rotatedRect = cv::minAreaRect(contour);
    const double aspectRatio = std::min(rotatedRect.size.width, rotatedRect.size.height) /
                               std::max(rotatedRect.size.width, rotatedRect.size.height);
    if (aspectRatio < 0.28) {
      continue;
    }

    MarkPoint mark;
    mark.name = prefix + "-" + std::to_string(++markIndex);
    mark.x = rotatedRect.center.x;
    mark.y = rotatedRect.center.y;
    mark.width = rotatedRect.size.width;
    mark.height = rotatedRect.size.height;
    mark.rotation = rotatedRect.angle;
    mark.score = std::clamp(0.62 + aspectRatio * 0.28, 0.0, 1.0);
    mark.previewScore = mark.score;
    mark.algorithm = MarkAlgorithm::ColorBrushTemplate;
    mark.enabled = true;
    results.push_back(mark);
  }

  return results;
}

std::vector<MarkPoint> detectCircles(const cv::Mat &gray, const std::string &prefix) {
  std::vector<MarkPoint> results;

  cv::Mat blurred;
  cv::GaussianBlur(gray, blurred, cv::Size(7, 7), 1.8);

  std::vector<cv::Vec3f> circles;
  cv::HoughCircles(blurred, circles, cv::HOUGH_GRADIENT, 1.2, 40.0,
                   80.0, 35.0, 12, 180);

  int markIndex = 0;
  for (const auto &circle : circles) {
    MarkPoint mark;
    mark.name = prefix + "-" + std::to_string(++markIndex);
    mark.x = circle[0];
    mark.y = circle[1];
    mark.width = circle[2] * 2.0;
    mark.height = circle[2] * 2.0;
    mark.shape = MarkShape::Circle;
    mark.score = std::clamp(0.68 + circle[2] / 280.0, 0.0, 1.0);
    mark.previewScore = mark.score;
    mark.algorithm = MarkAlgorithm::BinaryGeometry;
    mark.enabled = true;
    results.push_back(mark);
  }

  return results;
}
#endif // AOI_HAS_OPENCV

} // namespace

Result<std::vector<MarkPoint>> MarkDetector::detectTemplateMarks(const std::string &imagePath) const {
#ifdef AOI_HAS_OPENCV
  const cv::Mat image = cv::imread(imagePath, cv::IMREAD_COLOR);
  if (image.empty()) {
    return Result<std::vector<MarkPoint>>::failure("Failed to load image: " + imagePath);
  }

  cv::Mat gray;
  cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

  auto marks = detectByContour(gray, "Mark");

  if (marks.empty()) {
    marks = detectCircles(gray, "Mark");
  }

  std::ostringstream ss;
  ss << "Template detection found " << marks.size() << " candidate(s) in " << imagePath;
  return Result<std::vector<MarkPoint>>::success(marks, ss.str());
#else
  return Result<std::vector<MarkPoint>>::success(
      {{"Mark-A", 100.0, 80.0, 52.0, 46.0, 0.0, 0.92, 0.82, 0.89, 16, "#ff4d4f",
        MarkShape::Diamond, MarkAlgorithm::ColorBrushTemplate, true},
       {"Mark-B", 240.0, 82.0, 48.0, 48.0, 0.0, 0.90, 0.80, 0.87, 14, "#f97316",
        MarkShape::Cross, MarkAlgorithm::ColorBrushTemplate, true}},
      "Stub mark detection completed for " + imagePath);
#endif
}

Result<std::vector<MarkPoint>> MarkDetector::detectCircularMarks(const std::string &imagePath) const {
#ifdef AOI_HAS_OPENCV
  const cv::Mat image = cv::imread(imagePath, cv::IMREAD_COLOR);
  if (image.empty()) {
    return Result<std::vector<MarkPoint>>::failure("Failed to load image: " + imagePath);
  }

  cv::Mat gray;
  cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

  auto marks = detectCircles(gray, "Circle");

  if (marks.empty()) {
    marks = detectByContour(gray, "Circle");
  }

  std::ostringstream ss;
  ss << "Circle detection found " << marks.size() << " candidate(s) in " << imagePath;
  return Result<std::vector<MarkPoint>>::success(marks, ss.str());
#else
  return Result<std::vector<MarkPoint>>::success(
      {{"Circle-Mark", 120.0, 120.0, 42.0, 42.0, 0.0, 0.91, 0.80, 0.88, 12, "#ffffff",
        MarkShape::Circle, MarkAlgorithm::BinaryGeometry, true}},
      "Stub circle detection completed for " + imagePath);
#endif
}

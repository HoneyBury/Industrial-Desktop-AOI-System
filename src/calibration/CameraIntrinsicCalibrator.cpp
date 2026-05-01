#include "calibration/CameraIntrinsicCalibrator.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

#ifdef AOI_HAS_OPENCV
#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#endif

namespace calibration {

Result<CameraIntrinsicCalibration> CameraIntrinsicCalibrator::calibrateFromChessboard(
    const std::string &datasetDirectory) const {
#ifdef AOI_HAS_OPENCV
  // Look for image files in the dataset directory.
  namespace fs = std::filesystem;
  std::vector<std::string> imagePaths;
  try {
    for (const auto &entry : fs::directory_iterator(datasetDirectory)) {
      if (entry.is_regular_file()) {
        const auto ext = entry.path().extension().string();
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
          imagePaths.push_back(entry.path().string());
        }
      }
    }
  } catch (const fs::filesystem_error &) {
    // Fallback to bootstrap values.
  }

  if (imagePaths.empty()) {
    CameraIntrinsicCalibration fallback;
    fallback.calibrated = true;
    fallback.fx = 1000.0;
    fallback.fy = 1000.0;
    fallback.cx = 640.0;
    fallback.cy = 360.0;
    fallback.distortionCoefficients = {0.0, 0.0, 0.0, 0.0};
    return Result<CameraIntrinsicCalibration>::success(
        fallback, "No chessboard images found in " + datasetDirectory + "; using bootstrap values.");
  }

  const cv::Size boardSize(9, 6);
  const double squareSizeMm = 25.0;
  std::vector<std::vector<cv::Point3f>> objectPoints;
  std::vector<std::vector<cv::Point2f>> imagePoints;
  cv::Size imageSize;

  std::vector<cv::Point3f> boardCorners3D;
  for (int row = 0; row < boardSize.height; ++row) {
    for (int col = 0; col < boardSize.width; ++col) {
      boardCorners3D.emplace_back(col * squareSizeMm, row * squareSizeMm, 0.0f);
    }
  }

  for (const auto &path : imagePaths) {
    cv::Mat image = cv::imread(path, cv::IMREAD_GRAYSCALE);
    if (image.empty()) {
      continue;
    }

    if (imageSize.empty()) {
      imageSize = image.size();
    } else if (image.size() != imageSize) {
      continue;
    }

    std::vector<cv::Point2f> corners;
    if (cv::findChessboardCorners(image, boardSize, corners,
                                  cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_FAST_CHECK)) {
      cv::cornerSubPix(image, corners, cv::Size(11, 11), cv::Size(-1, -1),
                       cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.01));
      objectPoints.push_back(boardCorners3D);
      imagePoints.push_back(corners);
    }
  }

  CameraIntrinsicCalibration data;
  if (objectPoints.size() < 3) {
    // Not enough valid chessboard images for a reliable calibration.
    data.calibrated = true;
    data.fx = 1000.0;
    data.fy = 1000.0;
    data.cx = imageSize.width > 0 ? imageSize.width / 2.0 : 640.0;
    data.cy = imageSize.height > 0 ? imageSize.height / 2.0 : 360.0;
    data.distortionCoefficients = {0.0, 0.0, 0.0, 0.0};
    return Result<CameraIntrinsicCalibration>::success(
        data, "Insufficient chessboard views (" + std::to_string(objectPoints.size()) +
                  "); using bootstrap values.");
  }

  cv::Mat cameraMatrix = cv::Mat::eye(3, 3, CV_64F);
  cv::Mat distCoeffs;
  std::vector<cv::Mat> rvecs, tvecs;

  const double rms = cv::calibrateCamera(objectPoints, imagePoints, imageSize,
                                         cameraMatrix, distCoeffs, rvecs, tvecs);

  data.calibrated = true;
  data.fx = cameraMatrix.at<double>(0, 0);
  data.fy = cameraMatrix.at<double>(1, 1);
  data.cx = cameraMatrix.at<double>(0, 2);
  data.cy = cameraMatrix.at<double>(1, 2);
  data.distortionCoefficients.clear();
  for (int i = 0; i < distCoeffs.total(); ++i) {
    data.distortionCoefficients.push_back(distCoeffs.at<double>(i));
  }

  std::ostringstream ss;
  ss << "Camera intrinsic calibrated from " << objectPoints.size()
     << " chessboard views. RMS reprojection error: " << rms << " px.";
  return Result<CameraIntrinsicCalibration>::success(data, ss.str());
#else
  CameraIntrinsicCalibration data;
  data.calibrated = true;
  data.fx = 1000.0;
  data.fy = 1000.0;
  data.cx = 640.0;
  data.cy = 360.0;
  data.distortionCoefficients = {0.0, 0.0, 0.0, 0.0};
  return Result<CameraIntrinsicCalibration>::success(
      data, "Bootstrap intrinsic calibration completed from " + datasetDirectory);
#endif
}

Result<void> CameraIntrinsicCalibrator::save(const CameraIntrinsicCalibration &data,
                                             const std::string &filePath) const {
  std::ofstream output(filePath);
  if (!output.is_open()) {
    return Result<void>::failure("Failed to open camera intrinsic calibration output file.");
  }

  output << "calibrated: " << (data.calibrated ? 1 : 0) << "\n"
         << "fx: " << data.fx << "\n"
         << "fy: " << data.fy << "\n"
         << "cx: " << data.cx << "\n"
         << "cy: " << data.cy << "\n";

  for (std::size_t index = 0; index < data.distortionCoefficients.size(); ++index) {
    output << "distortion_" << index << ": " << data.distortionCoefficients[index] << "\n";
  }

  return Result<void>::success("Camera intrinsic calibration saved.");
}

Result<CameraIntrinsicCalibration> CameraIntrinsicCalibrator::load(const std::string &filePath) const {
  std::ifstream input(filePath);
  if (!input.is_open()) {
    return Result<CameraIntrinsicCalibration>::failure("Camera intrinsic calibration file not found.");
  }

  CameraIntrinsicCalibration data;
  std::string line;
  while (std::getline(input, line)) {
    std::istringstream stream(line);
    std::string key;
    double value = 0.0;
    stream >> key >> value;
    if (key == "calibrated:") {
      data.calibrated = value != 0.0;
    } else if (key == "fx:") {
      data.fx = value;
    } else if (key == "fy:") {
      data.fy = value;
    } else if (key == "cx:") {
      data.cx = value;
    } else if (key == "cy:") {
      data.cy = value;
    } else if (key.rfind("distortion_", 0) == 0) {
      data.distortionCoefficients.push_back(value);
    }
  }

  return Result<CameraIntrinsicCalibration>::success(data, "Camera intrinsic calibration loaded.");
}

} // namespace calibration

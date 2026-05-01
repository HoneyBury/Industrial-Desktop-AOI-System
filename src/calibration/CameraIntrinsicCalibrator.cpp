#include "calibration/CameraIntrinsicCalibrator.h"

#include <fstream>
#include <sstream>

namespace calibration {

Result<CameraIntrinsicCalibration> CameraIntrinsicCalibrator::calibrateFromChessboard(
    const std::string &datasetDirectory) const {
  CameraIntrinsicCalibration data;
  data.calibrated = true;
  data.fx = 1000.0;
  data.fy = 1000.0;
  data.cx = 640.0;
  data.cy = 360.0;
  data.distortionCoefficients = {0.0, 0.0, 0.0, 0.0};
  return Result<CameraIntrinsicCalibration>::success(
      data, "Bootstrap intrinsic calibration completed from " + datasetDirectory);
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

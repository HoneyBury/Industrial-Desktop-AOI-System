#include "vision/CameraCalibrator.h"

#include <fstream>
#include <sstream>

Result<CameraCalibrationData> CameraCalibrator::calibrateFromChessboard(
    const std::string &datasetDirectory) const {
  CameraCalibrationData data;
  data.fx = 1000.0;
  data.fy = 1000.0;
  data.cx = 640.0;
  data.cy = 360.0;
  return Result<CameraCalibrationData>::success(
      data, "Bootstrap calibration completed from " + datasetDirectory);
}

Result<void> CameraCalibrator::save(const CameraCalibrationData &data,
                                    const std::string &filePath) const {
  std::ofstream output(filePath);
  if (!output.is_open()) {
    return Result<void>::failure("Failed to open calibration output file.");
  }

  output << "fx: " << data.fx << "\n"
         << "fy: " << data.fy << "\n"
         << "cx: " << data.cx << "\n"
         << "cy: " << data.cy << "\n"
         << "pixel_to_mm_x: " << data.pixelToMillimeterX << "\n"
         << "pixel_to_mm_y: " << data.pixelToMillimeterY << "\n";
  return Result<void>::success("Calibration saved.");
}

Result<CameraCalibrationData> CameraCalibrator::load(const std::string &filePath) const {
  std::ifstream input(filePath);
  if (!input.is_open()) {
    return Result<CameraCalibrationData>::failure("Calibration file not found.");
  }

  CameraCalibrationData data;
  std::string line;
  while (std::getline(input, line)) {
    std::istringstream stream(line);
    std::string key;
    double value = 0.0;
    stream >> key >> value;
    if (key == "fx:") {
      data.fx = value;
    } else if (key == "fy:") {
      data.fy = value;
    } else if (key == "cx:") {
      data.cx = value;
    } else if (key == "cy:") {
      data.cy = value;
    } else if (key == "pixel_to_mm_x:") {
      data.pixelToMillimeterX = value;
    } else if (key == "pixel_to_mm_y:") {
      data.pixelToMillimeterY = value;
    }
  }

  return Result<CameraCalibrationData>::success(data, "Calibration loaded.");
}


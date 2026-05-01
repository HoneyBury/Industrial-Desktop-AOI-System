#pragma once

#include "calibration/CalibrationTypes.h"
#include "vision/CameraCalibrator.h"
#include "vision/MarkDetector.h"
#include "vision/RoiDetector.h"

#include <string>
#include <string_view>
#include <vector>

enum class RoiDetectorType {
  Geometry,
  Color,
  Template,
  Ai,
  Code,
};

inline constexpr std::string_view toString(const RoiDetectorType detectorType) {
  switch (detectorType) {
  case RoiDetectorType::Geometry:
    return "geometry";
  case RoiDetectorType::Color:
    return "color";
  case RoiDetectorType::Template:
    return "template";
  case RoiDetectorType::Ai:
    return "ai";
  case RoiDetectorType::Code:
    return "code";
  }

  return "geometry";
}

inline RoiDetectorType roiDetectorTypeFromString(const std::string &value) {
  if (value == "color") {
    return RoiDetectorType::Color;
  }
  if (value == "template") {
    return RoiDetectorType::Template;
  }
  if (value == "ai") {
    return RoiDetectorType::Ai;
  }
  if (value == "code") {
    return RoiDetectorType::Code;
  }

  return RoiDetectorType::Geometry;
}

struct RoiDetectorConfig {
  std::string roiName;
  RoiDetectorType detectorType {RoiDetectorType::Geometry};
  double threshold {0.0};
  std::string templateImagePath;
  std::string aiModelPath;
  std::string parameterSummary;
  bool enabled {true};
};

struct ProgramRuntimeSummary {
  std::string templateCachePath;
  std::string latestTemplateMatchSummary;
  bool hasMarkCalibration {false};
  double markCalibrationOffsetXmm {0.0};
  double markCalibrationOffsetYmm {0.0};
  double markCalibrationRotationDegrees {0.0};
  bool hasOriginCalibration {false};
  MechanicalPose originCorrectedPose;
};

struct ProgramModel {
  std::string name;
  std::string filePath;
  std::string aiModelPath;
  std::string calibrationFilePath;
  std::string codeRegionName;
  CameraIntrinsicCalibration cameraIntrinsicCalibration;
  PixelScaleCalibration pixelScaleCalibration;
  OriginCalibration originCalibration;
  LaserOffsetCalibration laserOffsetCalibration;
  std::vector<MarkReferenceRecord> markReferences;
  std::vector<RoiDetectorConfig> roiDetectorConfigs;
  ProgramRuntimeSummary runtimeSummary;

  // Laser marking operational parameters.
  double laserPowerPercent {80.0};
  double laserFrequencyKhz {20.0};
  double laserPulseWidthUs {10.0};
  int laserRepeatCount {1};

  // Transitional compatibility fields.
  std::string templateCachePath;
  std::string latestTemplateMatchSummary;
  std::vector<MarkPoint> marks;
  std::vector<RoiRegion> rois;
  CameraCalibrationData calibrationData;
  bool hasMarkCalibration {false};
  double markCalibrationOffsetXmm {0.0};
  double markCalibrationOffsetYmm {0.0};
  double markCalibrationRotationDegrees {0.0};
  bool hasOriginCalibration {false};
  double originCorrectedX {0.0};
  double originCorrectedY {0.0};
  double originCorrectedZ {0.0};
  double originCorrectedR {0.0};
};

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

enum class ScanOrder {
  LeftToRight,
  TopToBottom,
};

inline constexpr std::string_view toString(const ScanOrder scanOrder) {
  switch (scanOrder) {
  case ScanOrder::LeftToRight:
    return "left_to_right";
  case ScanOrder::TopToBottom:
    return "top_to_bottom";
  }

  return "left_to_right";
}

inline ScanOrder scanOrderFromString(const std::string &value) {
  if (value == "top_to_bottom") {
    return ScanOrder::TopToBottom;
  }

  return ScanOrder::LeftToRight;
}

struct BoardDefinition {
  double boardLengthMm {260.0};
  double boardWidthMm {180.0};
  double railWidthMm {32.0};
};

struct ScanRecipe {
  double fovWidthMm {32.0};
  double fovHeightMm {24.0};
  ScanOrder scanOrder {ScanOrder::LeftToRight};
  bool enabled {true};
};

struct LaserPointTask {
  std::string name;
  double x {0.0};
  double y {0.0};
  std::string linkedRoiName;
  std::string expectedCodeText {"DEMO-CODE-001"};
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
  bool hasLaserOffsetCalibration {false};
  double laserOffsetDxMm {0.0};
  double laserOffsetDyMm {0.0};
  std::string wholeBoardImagePath;
  int scanTileRows {0};
  int scanTileColumns {0};
  std::string lastBoardScanSummary;
};

struct ProgramModel {
  std::string name;
  std::string filePath;
  std::string aiModelPath;
  std::string calibrationFilePath;
  std::string codeRegionName;
  BoardDefinition boardDefinition;
  ScanRecipe scanRecipe;
  CameraIntrinsicCalibration cameraIntrinsicCalibration;
  PixelScaleCalibration pixelScaleCalibration;
  OriginCalibration originCalibration;
  LaserOffsetCalibration laserOffsetCalibration;
  std::vector<MarkReferenceRecord> markReferences;
  std::vector<RoiDetectorConfig> roiDetectorConfigs;
  std::vector<LaserPointTask> laserPointTasks;
  ProgramRuntimeSummary runtimeSummary;

  // Laser marking operational parameters.
  double laserPowerPercent {80.0};
  double laserFrequencyKhz {20.0};
  double laserPulseWidthUs {10.0};
  int laserRepeatCount {1};

  // Active editor data (UI-facing).
  // These hold the spatial definitions edited on the workbench canvas.
  // `markReferences` and `roiDetectorConfigs` are the canonical calibrated
  // inspection targets derived from these.
  std::vector<MarkPoint> marks;
  std::vector<RoiRegion> rois;

  // Legacy calibration blob — retained for JSON round-trip compatibility
  // with programs saved before the calibration sub-module split.
  CameraCalibrationData calibrationData;
};

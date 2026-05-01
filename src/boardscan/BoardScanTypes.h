#pragma once

#include "common/Result.h"
#include "coordinate/CoordinateTransformer.h"
#include "program/ProgramModel.h"

#include <string>
#include <vector>

struct FovCapturePose {
  int index {0};
  int row {0};
  int column {0};
  MillimeterPoint productTopLeftMm;
  MillimeterPoint productCenterMm;
  MechanicalPose machinePose;
  double fovWidthMm {0.0};
  double fovHeightMm {0.0};
};

struct PlannedBoardScan {
  BoardDefinition boardDefinition;
  ScanRecipe scanRecipe;
  MechanicalPose originPose;
  int tileRows {0};
  int tileColumns {0};
  std::vector<FovCapturePose> poses;
};

struct CapturedFovTile {
  FovCapturePose pose;
  std::string imagePath;
};

struct BoardScanTilePlacement {
  FovCapturePose pose;
  int pixelX {0};
  int pixelY {0};
  int pixelWidth {0};
  int pixelHeight {0};
  std::string imagePath;
};

struct BoardScanMosaicLayout {
  int tileRows {0};
  int tileColumns {0};
  int tilePixelWidth {0};
  int tilePixelHeight {0};
  int mosaicPixelWidth {0};
  int mosaicPixelHeight {0};
  std::vector<BoardScanTilePlacement> placements;
};

struct BoardScanCaptureResult {
  std::string mosaicImagePath;
  int tileRows {0};
  int tileColumns {0};
  int capturedTileCount {0};
  std::string summary;
  std::vector<CapturedFovTile> capturedTiles;
};

using BoardScanPlanResult = Result<PlannedBoardScan>;
using BoardScanExecutionResult = Result<std::vector<CapturedFovTile>>;
using BoardScanLayoutResult = Result<BoardScanMosaicLayout>;
using BoardScanCaptureWorkflowResult = Result<BoardScanCaptureResult>;

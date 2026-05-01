#pragma once

#include "ai/AiInferencer.h"
#include "boardscan/BoardScanTypes.h"
#include "database/DatabaseManager.h"
#include "alignment/MarkAlignmentSolver.h"
#include "laser/ILaserController.h"
#include "motion/IMotionController.h"
#include "process/ProcessTypes.h"
#include "program/ProgramModel.h"
#include "transport/ITransportController.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

struct RoiInspectionResult {
  std::string roiName;
  RoiDetectorType detectorType {RoiDetectorType::Geometry};
  bool passed {true};
  double confidence {0.0};
  int candidateCount {0};
  std::string decodedText;
  std::string summary;
  std::vector<AiDetection> detections;
};

struct LaserPointExecutionResult {
  std::string taskName;
  std::string linkedRoiName;
  MillimeterPoint productPoint;
  MechanicalPose machinePose;
  bool laserExecuted {false};
  bool codeVerified {false};
  bool passed {false};
  std::string expectedCodeText;
  std::string decodedText;
  std::string summary;
};

struct WorkflowContext {
  std::string boardId;
  ProgramModel *program {nullptr};
  IMotionController *motionController {nullptr};
  ILaserController *laserController {nullptr};
  ITransportController *transportController {nullptr};
  DatabaseManager *databaseManager {nullptr};
  std::string currentImagePath;
  std::string captureDir; // temp directory for captured frames
  MechanicalPose currentMachinePose;
  std::vector<MarkPoint> measuredMarks;
  alignment::MarkAlignmentResult lastMarkAlignment;
  bool hasMarkAlignment {false};
  MechanicalPose preparedLaserPose;
  bool hasPreparedLaserPose {false};
  bool laserExecuted {false};
  std::string wholeBoardImagePath;
  int scanTileRows {0};
  int scanTileColumns {0};
  std::string boardScanSummary;
  std::vector<CapturedFovTile> capturedBoardTiles;
  std::vector<AiDetection> aiDetections;
  // Per-ROI captured images keyed by ROI name.
  std::unordered_map<std::string, std::string> capturedRoiImages;
  std::vector<RoiInspectionResult> roiInspectionResults;
  std::vector<LaserPointExecutionResult> laserPointResults;
  std::string inspectionDetailsJson;
  bool finalDecisionOk {true};
  std::vector<std::string> eventLog;
  bool reuseInjectedInputs {false};
  int nextStepIndex {0};
  std::vector<StepExecutionRecord> currentBoardRecords;
  bool boardReady {false};

  // Production statistics
  int boardIndex {0};
  int totalBoards {0};
  int okCount {0};
  int ngCount {0};

  // Step progress callback: (stepIndex, totalSteps, stepName, status)
  using StepProgressCallback = std::function<void(int, int, const std::string &, StepExecutionStatus)>;
  StepProgressCallback onStepProgress;

  // Board result callback: (boardIndex, ok, summary)
  using BoardResultCallback = std::function<void(int, bool, const std::string &)>;
  BoardResultCallback onBoardResult;

  // Log callback: (message)
  using LogCallback = std::function<void(const std::string &)>;
  LogCallback onLog;

  // Frame capture callback: saves current frame to disk, returns path
  using CaptureFrameCallback = std::function<std::string()>;
  CaptureFrameCallback captureFrame;

  // Whole-board scan callback: captures/stitches a complete board image.
  using WholeBoardScanCallback = std::function<BoardScanCaptureWorkflowResult()>;
  WholeBoardScanCallback captureWholeBoardScan;

  // Camera pose movement callback: used by workflow/business steps to move the
  // simulated or real camera axes as a single business action.
  using MoveCameraPoseCallback = std::function<bool(const MechanicalPose &, const std::string &)>;
  MoveCameraPoseCallback moveCameraPose;
};

#pragma once

#include "ai/AiInferencer.h"
#include "alignment/MarkAlignmentSolver.h"
#include "laser/ILaserController.h"
#include "motion/IMotionController.h"
#include "process/ProcessTypes.h"
#include "program/ProgramModel.h"

#include <functional>
#include <string>
#include <vector>

struct WorkflowContext {
  std::string boardId;
  ProgramModel *program {nullptr};
  IMotionController *motionController {nullptr};
  ILaserController *laserController {nullptr};
  std::string currentImagePath;
  std::string captureDir; // temp directory for captured frames
  MechanicalPose currentMachinePose;
  std::vector<MarkPoint> measuredMarks;
  alignment::MarkAlignmentResult lastMarkAlignment;
  bool hasMarkAlignment {false};
  MechanicalPose preparedLaserPose;
  bool hasPreparedLaserPose {false};
  bool laserExecuted {false};
  std::vector<AiDetection> aiDetections;
  bool finalDecisionOk {true};
  std::vector<std::string> eventLog;

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
};

#pragma once

#include "boardscan/BoardScanTypes.h"
#include "motion/IMotionController.h"

#include <functional>

class BoardScanExecutor {
public:
  using CaptureTileCallback = std::function<std::string(const FovCapturePose &)>;
  using MoveToPoseCallback = std::function<bool(const MechanicalPose &)>;

  [[nodiscard]] BoardScanExecutionResult execute(const PlannedBoardScan &plan,
                                                 IMotionController &motionController,
                                                 const CaptureTileCallback &captureTile,
                                                 const MoveToPoseCallback &moveToPose = {}) const;
};

#include "boardscan/BoardScanExecutor.h"

#include "motion/MotionAxis.h"

#include <sstream>

BoardScanExecutionResult BoardScanExecutor::execute(const PlannedBoardScan &plan,
                                                    IMotionController &motionController,
                                                    const CaptureTileCallback &captureTile) const {
  if (!captureTile) {
    return BoardScanExecutionResult::failure("Capture callback is missing.");
  }
  if (plan.poses.empty()) {
    return BoardScanExecutionResult::failure("Board scan plan is empty.");
  }

  std::vector<CapturedFovTile> tiles;
  tiles.reserve(plan.poses.size());

  for (const auto &pose : plan.poses) {
    if (!motionController.moveAbsolute(MotionAxis::CameraX, pose.machinePose.x) ||
        !motionController.moveAbsolute(MotionAxis::CameraY, pose.machinePose.y) ||
        !motionController.moveAbsolute(MotionAxis::Z, pose.machinePose.z) ||
        !motionController.moveAbsolute(MotionAxis::R, pose.machinePose.r)) {
      std::ostringstream stream;
      stream << "Failed to move to scan tile r" << pose.row << " c" << pose.column << ".";
      return BoardScanExecutionResult::failure(stream.str());
    }

    const std::string imagePath = captureTile(pose);
    if (imagePath.empty()) {
      std::ostringstream stream;
      stream << "Capture callback returned an empty image path at tile r" << pose.row << " c" << pose.column << ".";
      return BoardScanExecutionResult::failure(stream.str());
    }

    tiles.push_back(CapturedFovTile {pose, imagePath});
  }

  std::ostringstream summary;
  summary << "Captured " << tiles.size() << " scan tiles.";
  return BoardScanExecutionResult::success(std::move(tiles), summary.str());
}

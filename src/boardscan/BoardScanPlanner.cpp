#include "boardscan/BoardScanPlanner.h"

#include "coordinate/CoordinateTransformer.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace {

double tileStartCoordinate(const int index, const int tileCount, const double boardSpanMm, const double fovSpanMm) {
  if (tileCount <= 1 || boardSpanMm <= fovSpanMm) {
    return 0.0;
  }

  const double nominal = static_cast<double>(index) * fovSpanMm;
  return std::min(nominal, std::max(0.0, boardSpanMm - fovSpanMm));
}

} // namespace

BoardScanPlanResult BoardScanPlanner::plan(const BoardDefinition &boardDefinition,
                                           const ScanRecipe &scanRecipe,
                                           const MechanicalPose &originPose) const {
  if (!scanRecipe.enabled) {
    return BoardScanPlanResult::failure("Board scan is disabled in the scan recipe.");
  }
  if (boardDefinition.boardLengthMm <= 0.0 || boardDefinition.boardWidthMm <= 0.0) {
    return BoardScanPlanResult::failure("Board dimensions must be positive.");
  }
  if (scanRecipe.fovWidthMm <= 0.0 || scanRecipe.fovHeightMm <= 0.0) {
    return BoardScanPlanResult::failure("FOV dimensions must be positive.");
  }

  PlannedBoardScan plan;
  plan.boardDefinition = boardDefinition;
  plan.scanRecipe = scanRecipe;
  plan.originPose = originPose;
  plan.tileColumns = std::max(1, static_cast<int>(std::ceil(boardDefinition.boardLengthMm / scanRecipe.fovWidthMm)));
  plan.tileRows = std::max(1, static_cast<int>(std::ceil(boardDefinition.boardWidthMm / scanRecipe.fovHeightMm)));
  plan.poses.reserve(static_cast<std::size_t>(plan.tileRows * plan.tileColumns));

  CoordinateTransformer transformer;
  int captureIndex = 0;

  if (scanRecipe.scanOrder == ScanOrder::LeftToRight) {
    for (int row = 0; row < plan.tileRows; ++row) {
      const double topY = tileStartCoordinate(row, plan.tileRows, boardDefinition.boardWidthMm, scanRecipe.fovHeightMm);
      for (int column = 0; column < plan.tileColumns; ++column) {
        const double leftX =
            tileStartCoordinate(column, plan.tileColumns, boardDefinition.boardLengthMm, scanRecipe.fovWidthMm);
        const MillimeterPoint center {leftX + scanRecipe.fovWidthMm / 2.0, topY + scanRecipe.fovHeightMm / 2.0};
        plan.poses.push_back(FovCapturePose {
            captureIndex++,
            row,
            column,
            MillimeterPoint {leftX, topY},
            center,
            transformer.productToMechanical(center, originPose),
            scanRecipe.fovWidthMm,
            scanRecipe.fovHeightMm,
        });
      }
    }
  } else {
    for (int column = 0; column < plan.tileColumns; ++column) {
      const double leftX =
          tileStartCoordinate(column, plan.tileColumns, boardDefinition.boardLengthMm, scanRecipe.fovWidthMm);
      for (int row = 0; row < plan.tileRows; ++row) {
        const double topY = tileStartCoordinate(row, plan.tileRows, boardDefinition.boardWidthMm, scanRecipe.fovHeightMm);
        const MillimeterPoint center {leftX + scanRecipe.fovWidthMm / 2.0, topY + scanRecipe.fovHeightMm / 2.0};
        plan.poses.push_back(FovCapturePose {
            captureIndex++,
            row,
            column,
            MillimeterPoint {leftX, topY},
            center,
            transformer.productToMechanical(center, originPose),
            scanRecipe.fovWidthMm,
            scanRecipe.fovHeightMm,
        });
      }
    }
  }

  std::ostringstream summary;
  summary << "Planned board scan: " << plan.tileRows << " rows x " << plan.tileColumns << " cols.";
  return BoardScanPlanResult::success(plan, summary.str());
}

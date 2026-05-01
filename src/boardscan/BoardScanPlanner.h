#pragma once

#include "boardscan/BoardScanTypes.h"

class BoardScanPlanner {
public:
  [[nodiscard]] BoardScanPlanResult plan(const BoardDefinition &boardDefinition,
                                         const ScanRecipe &scanRecipe,
                                         const MechanicalPose &originPose) const;
};

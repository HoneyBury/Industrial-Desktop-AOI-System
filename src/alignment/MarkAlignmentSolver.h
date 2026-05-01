#pragma once

#include "calibration/CalibrationTypes.h"
#include "common/Result.h"
#include "coordinate/CoordinateTransformer.h"
#include "vision/MarkDetector.h"

#include <vector>

namespace alignment {

enum class AlignmentMode {
  SingleMarkTranslation,
  DualMarkRigid,
};

struct MarkAlignmentInput {
  std::vector<MarkPoint> referenceMarks;
  std::vector<MarkPoint> measuredMarks;
  PixelScaleCalibration pixelScale;
};

struct MarkAlignmentResult {
  AlignmentMode mode {AlignmentMode::SingleMarkTranslation};
  PixelPoint pixelOffset;
  MillimeterPoint millimeterOffset;
  double rotationDegrees {0.0};
  RigidTransform2D productCompensation;
};

class MarkAlignmentSolver {
public:
  Result<MarkAlignmentResult> solve(const MarkAlignmentInput &input) const;
};

} // namespace alignment

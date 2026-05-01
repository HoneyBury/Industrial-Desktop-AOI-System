#include "alignment/MarkAlignmentSolver.h"

#include <algorithm>

namespace alignment {

Result<MarkAlignmentResult> MarkAlignmentSolver::solve(const MarkAlignmentInput &input) const {
  if (input.referenceMarks.empty() || input.measuredMarks.empty()) {
    return Result<MarkAlignmentResult>::failure("Mark alignment requires both reference and measured marks.");
  }

  CoordinateTransformer transformer(input.pixelScale.pixelToMillimeterX, input.pixelScale.pixelToMillimeterY);
  MarkAlignmentResult result;

  const auto &referenceA = input.referenceMarks.front();
  const auto &measuredA = input.measuredMarks.front();
  result.pixelOffset = PixelPoint {measuredA.x - referenceA.x, measuredA.y - referenceA.y};
  result.millimeterOffset = transformer.pixelToMillimeter(result.pixelOffset);

  if (input.referenceMarks.size() >= 2 && input.measuredMarks.size() >= 2) {
    result.mode = AlignmentMode::DualMarkRigid;
    result.rotationDegrees = transformer.computeMarkRotationDegrees(
        {PixelPoint {input.referenceMarks[0].x, input.referenceMarks[0].y},
         PixelPoint {input.referenceMarks[1].x, input.referenceMarks[1].y}},
        {PixelPoint {input.measuredMarks[0].x, input.measuredMarks[0].y},
         PixelPoint {input.measuredMarks[1].x, input.measuredMarks[1].y}});
  }

  result.productCompensation = RigidTransform2D {
      result.millimeterOffset.x,
      result.millimeterOffset.y,
      result.rotationDegrees,
  };
  return Result<MarkAlignmentResult>::success(result, "Mark alignment completed.");
}

} // namespace alignment

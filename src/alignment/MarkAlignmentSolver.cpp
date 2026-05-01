#include "alignment/MarkAlignmentSolver.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace alignment {

Result<MarkAlignmentResult> MarkAlignmentSolver::solve(const MarkAlignmentInput &input) const {
  if (input.referenceMarks.empty() || input.measuredMarks.empty()) {
    return Result<MarkAlignmentResult>::failure("Mark alignment requires both reference and measured marks.");
  }

  const std::size_t pairCount = std::min(input.referenceMarks.size(), input.measuredMarks.size());
  CoordinateTransformer transformer(input.pixelScale.pixelToMillimeterX,
                                    input.pixelScale.pixelToMillimeterY);

  if (pairCount == 1) {
    return solveSingle(input, transformer);
  }
  if (pairCount == 2) {
    return solveDual(input, transformer);
  }
  return solveMulti(input, transformer);
}

Result<MarkAlignmentResult> MarkAlignmentSolver::solveSingle(
    const MarkAlignmentInput &input, const CoordinateTransformer &transformer) const {

  const auto &ref = input.referenceMarks.front();
  const auto &meas = input.measuredMarks.front();

  MarkAlignmentResult result;
  result.mode = AlignmentMode::SingleMarkTranslation;
  result.pixelOffset = PixelPoint {meas.x - ref.x, meas.y - ref.y};
  result.millimeterOffset = transformer.pixelToMillimeter(result.pixelOffset);
  result.rotationDegrees = 0.0;
  result.residualRmsPx = 0.0;
  result.productCompensation = RigidTransform2D {
      result.millimeterOffset.x, result.millimeterOffset.y, 0.0};
  return Result<MarkAlignmentResult>::success(result, "Single-mark translation alignment completed.");
}

Result<MarkAlignmentResult> MarkAlignmentSolver::solveDual(
    const MarkAlignmentInput &input, const CoordinateTransformer &transformer) const {

  const auto &refA = input.referenceMarks[0];
  const auto &refB = input.referenceMarks[1];
  const auto &measA = input.measuredMarks[0];
  const auto &measB = input.measuredMarks[1];

  MarkAlignmentResult result;
  result.mode = AlignmentMode::DualMarkRigid;

  // Translation: raw pixel offset from the first measured mark.
  const double dxA = measA.x - refA.x;
  const double dyA = measA.y - refA.y;

  result.pixelOffset = PixelPoint {dxA, dyA};
  result.millimeterOffset = transformer.pixelToMillimeter(result.pixelOffset);
  result.rotationDegrees = transformer.computeMarkRotationDegrees(
      {PixelPoint {refA.x, refA.y}, PixelPoint {refB.x, refB.y}},
      {PixelPoint {measA.x, measA.y}, PixelPoint {measB.x, measB.y}});

  // Residual: how much the second mark deviates after applying the rigid transform.
  const double cosR = std::cos(result.rotationDegrees * M_PI / 180.0);
  const double sinR = std::sin(result.rotationDegrees * M_PI / 180.0);
  const double txB = cosR * refB.x - sinR * refB.y + dxA;
  const double tyB = sinR * refB.x + cosR * refB.y + dyA;
  result.residualRmsPx = std::hypot(measB.x - txB, measB.y - tyB);

  result.productCompensation = RigidTransform2D {
      result.millimeterOffset.x, result.millimeterOffset.y, result.rotationDegrees};
  return Result<MarkAlignmentResult>::success(result, "Dual-mark rigid alignment completed.");
}

Result<MarkAlignmentResult> MarkAlignmentSolver::solveMulti(
    const MarkAlignmentInput &input, const CoordinateTransformer &transformer) const {

  const std::size_t N = std::min(input.referenceMarks.size(), input.measuredMarks.size());

  // 1. Compute centroids.
  double refCx = 0.0, refCy = 0.0;
  double measCx = 0.0, measCy = 0.0;
  for (std::size_t i = 0; i < N; ++i) {
    refCx += input.referenceMarks[i].x;
    refCy += input.referenceMarks[i].y;
    measCx += input.measuredMarks[i].x;
    measCy += input.measuredMarks[i].y;
  }
  refCx /= static_cast<double>(N);
  refCy /= static_cast<double>(N);
  measCx /= static_cast<double>(N);
  measCy /= static_cast<double>(N);

  // 2. Build 2x2 cross-covariance matrix H = sum(d_ref_i * d_meas_i^T).
  double h00 = 0.0, h01 = 0.0, h10 = 0.0, h11 = 0.0;
  for (std::size_t i = 0; i < N; ++i) {
    const double drx = input.referenceMarks[i].x - refCx;
    const double dry = input.referenceMarks[i].y - refCy;
    const double dmx = input.measuredMarks[i].x - measCx;
    const double dmy = input.measuredMarks[i].y - measCy;
    h00 += drx * dmx;
    h01 += drx * dmy;
    h10 += dry * dmx;
    h11 += dry * dmy;
  }

  // 3. Compute optimal rotation via Kabsch-Umeyama (2D closed form).
  //    Rotation angle theta = atan2(h01 - h10, h00 + h11).
  const double num = h01 - h10;
  const double den = h00 + h11;
  const double theta = std::atan2(num, den);
  const double cosR = std::cos(theta);
  const double sinR = std::sin(theta);

  // 4. Translation = centroid_meas - R * centroid_ref (in pixel space).
  const double txPx = measCx - (cosR * refCx - sinR * refCy);
  const double tyPx = measCy - (sinR * refCx + cosR * refCy);

  MarkAlignmentResult result;
  result.mode = AlignmentMode::MultiMarkLeastSquares;
  result.pixelOffset = PixelPoint {txPx, tyPx};
  result.millimeterOffset = transformer.pixelToMillimeter(result.pixelOffset);
  result.rotationDegrees = theta * 180.0 / M_PI;

  // 5. Compute residual RMS in pixels.
  double residualSum = 0.0;
  for (std::size_t i = 0; i < N; ++i) {
    const double rx = input.referenceMarks[i].x;
    const double ry = input.referenceMarks[i].y;
    const double predictedX = cosR * rx - sinR * ry + txPx;
    const double predictedY = sinR * rx + cosR * ry + tyPx;
    const double errX = input.measuredMarks[i].x - predictedX;
    const double errY = input.measuredMarks[i].y - predictedY;
    residualSum += errX * errX + errY * errY;
  }
  result.residualRmsPx = std::sqrt(residualSum / static_cast<double>(N));

  result.productCompensation = RigidTransform2D {
      result.millimeterOffset.x, result.millimeterOffset.y, result.rotationDegrees};

  std::ostringstream ss;
  ss << "Multi-mark (" << N << " marks) least-squares alignment completed. "
     << "RMS residual: " << result.residualRmsPx << " px.";
  return Result<MarkAlignmentResult>::success(result, ss.str());
}

} // namespace alignment

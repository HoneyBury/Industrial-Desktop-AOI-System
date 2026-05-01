#include <gtest/gtest.h>

#include "alignment/MarkAlignmentSolver.h"

TEST(MarkAlignmentSolverTest, SolvesSingleMarkTranslation) {
  alignment::MarkAlignmentSolver solver;
  const auto result = solver.solve(alignment::MarkAlignmentInput {
      {
          {"Mark-A", 100.0, 80.0, 40.0, 40.0, 0.0, 0.0, 0.8, 0.8, 0, "#fff",
           MarkShape::Rectangle, MarkAlgorithm::ColorBrushTemplate, true},
      },
      {
          {"Mark-A", 110.0, 90.0, 40.0, 40.0, 0.0, 0.0, 0.8, 0.8, 0, "#fff",
           MarkShape::Rectangle, MarkAlgorithm::ColorBrushTemplate, true},
      },
      PixelScaleCalibration {true, 0.01, 0.02},
  });

  ASSERT_TRUE(result);
  EXPECT_EQ(result.value.mode, alignment::AlignmentMode::SingleMarkTranslation);
  EXPECT_NEAR(result.value.pixelOffset.x, 10.0, 1e-9);
  EXPECT_NEAR(result.value.pixelOffset.y, 10.0, 1e-9);
  EXPECT_NEAR(result.value.millimeterOffset.x, 0.1, 1e-9);
  EXPECT_NEAR(result.value.millimeterOffset.y, 0.2, 1e-9);
  EXPECT_NEAR(result.value.rotationDegrees, 0.0, 1e-9);
}

TEST(MarkAlignmentSolverTest, SolvesDualMarkRigidAlignment) {
  alignment::MarkAlignmentSolver solver;
  const auto result = solver.solve(alignment::MarkAlignmentInput {
      {
          {"Mark-A", 0.0, 0.0, 40.0, 40.0, 0.0, 0.0, 0.8, 0.8, 0, "#fff",
           MarkShape::Rectangle, MarkAlgorithm::ColorBrushTemplate, true},
          {"Mark-B", 100.0, 0.0, 40.0, 40.0, 0.0, 0.0, 0.8, 0.8, 0, "#fff",
           MarkShape::Rectangle, MarkAlgorithm::ColorBrushTemplate, true},
      },
      {
          {"Mark-A", 5.0, 2.0, 40.0, 40.0, 0.0, 0.0, 0.8, 0.8, 0, "#fff",
           MarkShape::Rectangle, MarkAlgorithm::ColorBrushTemplate, true},
          {"Mark-B", 105.0, 12.0, 40.0, 40.0, 0.0, 0.0, 0.8, 0.8, 0, "#fff",
           MarkShape::Rectangle, MarkAlgorithm::ColorBrushTemplate, true},
      },
      PixelScaleCalibration {true, 0.01, 0.01},
  });

  ASSERT_TRUE(result);
  EXPECT_EQ(result.value.mode, alignment::AlignmentMode::DualMarkRigid);
  EXPECT_NEAR(result.value.pixelOffset.x, 5.0, 1e-9);
  EXPECT_NEAR(result.value.pixelOffset.y, 2.0, 1e-9);
  EXPECT_NEAR(result.value.millimeterOffset.x, 0.05, 1e-9);
  EXPECT_NEAR(result.value.millimeterOffset.y, 0.02, 1e-9);
  EXPECT_NEAR(result.value.rotationDegrees, 5.7105931375, 1e-6);
}

TEST(MarkAlignmentSolverTest, SolvesMultiMarkLeastSquaresFit) {
  alignment::MarkAlignmentSolver solver;

  // Four reference marks forming a square, measured with a known rigid transform:
  // translate (+5, +3) px, rotate 10 degrees around reference centroid.
  const double theta = 10.0 * M_PI / 180.0;
  const double cosT = std::cos(theta);
  const double sinT = std::sin(theta);
  const double tx = 5.0;
  const double ty = 3.0;

  std::vector<MarkPoint> refMarks;
  std::vector<MarkPoint> measMarks;

  const double refCorners[4][2] = {{0, 0}, {200, 0}, {200, 150}, {0, 150}};
  for (const auto &corner : refCorners) {
    MarkPoint ref;
    ref.x = corner[0];
    ref.y = corner[1];
    refMarks.push_back(ref);

    MarkPoint meas;
    meas.x = cosT * corner[0] - sinT * corner[1] + tx;
    meas.y = sinT * corner[0] + cosT * corner[1] + ty;
    measMarks.push_back(meas);
  }

  const auto result = solver.solve(alignment::MarkAlignmentInput {
      refMarks, measMarks, PixelScaleCalibration {true, 0.01, 0.01}});

  ASSERT_TRUE(result);
  EXPECT_EQ(result.value.mode, alignment::AlignmentMode::MultiMarkLeastSquares);
  EXPECT_NEAR(result.value.pixelOffset.x, tx, 1e-6);
  EXPECT_NEAR(result.value.pixelOffset.y, ty, 1e-6);
  EXPECT_NEAR(result.value.rotationDegrees, 10.0, 1e-6);
  EXPECT_NEAR(result.value.residualRmsPx, 0.0, 1e-6);
  EXPECT_NEAR(result.value.millimeterOffset.x, tx * 0.01, 1e-9);
  EXPECT_NEAR(result.value.millimeterOffset.y, ty * 0.01, 1e-9);
}

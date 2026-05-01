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

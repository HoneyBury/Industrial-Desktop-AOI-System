#include <gtest/gtest.h>

#include "vision/CoordinateTransformer.h"

TEST(MarkOffsetTest, ComputesDualMarkRotationDegrees) {
  CoordinateTransformer transformer(0.01, 0.01);

  const double angle = transformer.computeMarkRotationDegrees(
      {PixelPoint {0.0, 0.0}, PixelPoint {100.0, 0.0}},
      {PixelPoint {0.0, 0.0}, PixelPoint {100.0, 10.0}});

  EXPECT_NEAR(angle, 5.7105931375, 1e-6);
}


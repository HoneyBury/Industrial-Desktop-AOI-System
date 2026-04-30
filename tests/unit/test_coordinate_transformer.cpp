#include <gtest/gtest.h>

#include "vision/CoordinateTransformer.h"

TEST(CoordinateTransformerTest, ConvertsPixelOffsetToMillimeter) {
  CoordinateTransformer transformer(0.02, 0.05);
  const auto point = transformer.pixelToMillimeter(PixelPoint {10.0, 4.0});

  EXPECT_NEAR(point.x, 0.2, 1e-9);
  EXPECT_NEAR(point.y, 0.2, 1e-9);
}

TEST(CoordinateTransformerTest, ConvertsProductToMechanicalPose) {
  CoordinateTransformer transformer(0.01, 0.01);
  const auto pose = transformer.productToMechanical(MillimeterPoint {2.5, -1.0},
                                                    MechanicalPose {100.0, 50.0, 0.0, 0.0});

  EXPECT_NEAR(pose.x, 102.5, 1e-9);
  EXPECT_NEAR(pose.y, 49.0, 1e-9);
}


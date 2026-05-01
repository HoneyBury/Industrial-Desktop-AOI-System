#include <gtest/gtest.h>

#include "coordinate/CoordinateTransformer.h"

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

TEST(CoordinateTransformerTest, RunsFullPixelToLaserChain) {
  CoordinateTransformChain chain;
  chain.opticalCenterPixel = PixelPoint {640.0, 360.0};
  chain.pixelToMillimeterX = 0.01;
  chain.pixelToMillimeterY = 0.02;
  chain.imagePhysicalToProduct = RigidTransform2D {1.0, -2.0, 0.0};
  chain.productToMachine = RigidTransform2D {10.0, 20.0, 0.0};
  chain.machineToLaserOffset = MillimeterPoint {0.5, -0.25};

  CoordinateTransformer transformer(chain);
  const auto laserPose =
      transformer.cameraPixelToLaser(PixelPoint {650.0, 370.0}, MechanicalPose {100.0, 200.0, 0.0, 0.0});

  EXPECT_NEAR(laserPose.x, 111.6, 1e-9);
  EXPECT_NEAR(laserPose.y, 217.95, 1e-9);
  EXPECT_NEAR(laserPose.z, 0.0, 1e-9);
}

TEST(CoordinateTransformerTest, AppliesRigidTransformWithRotation) {
  const auto point = CoordinateTransformer::applyRigidTransform(MillimeterPoint {1.0, 0.0},
                                                                RigidTransform2D {2.0, 3.0, 90.0});

  EXPECT_NEAR(point.x, 2.0, 1e-9);
  EXPECT_NEAR(point.y, 4.0, 1e-9);
}

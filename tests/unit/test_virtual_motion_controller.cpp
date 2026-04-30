#include <gtest/gtest.h>

#include "motion/VirtualMotionController.h"

TEST(VirtualMotionControllerTest, MovesAxisToAbsolutePosition) {
  VirtualMotionController controller;

  ASSERT_TRUE(controller.moveAbsolute(MotionAxis::X, 125.0));
  ASSERT_TRUE(controller.position(MotionAxis::X).has_value());
  EXPECT_NEAR(controller.position(MotionAxis::X).value(), 125.0, 1e-9);
}

TEST(VirtualMotionControllerTest, MovesAxisByRelativeOffset) {
  VirtualMotionController controller;

  ASSERT_TRUE(controller.moveAbsolute(MotionAxis::Y, 20.0));
  ASSERT_TRUE(controller.moveRelative(MotionAxis::Y, -5.5));
  EXPECT_NEAR(controller.position(MotionAxis::Y).value(), 14.5, 1e-9);
}

TEST(VirtualMotionControllerTest, HomesAxisToZero) {
  VirtualMotionController controller;

  ASSERT_TRUE(controller.moveAbsolute(MotionAxis::Z, 42.0));
  ASSERT_TRUE(controller.home(MotionAxis::Z));
  EXPECT_NEAR(controller.position(MotionAxis::Z).value(), 0.0, 1e-9);
}

TEST(VirtualMotionControllerTest, EmergencyStopBlocksMotionUntilReset) {
  VirtualMotionController controller;

  controller.emergencyStop();
  EXPECT_TRUE(controller.isStopped());
  EXPECT_TRUE(!controller.moveAbsolute(MotionAxis::X, 50.0));

  controller.resetEmergencyStop();
  EXPECT_TRUE(!controller.isStopped());
  ASSERT_TRUE(controller.moveAbsolute(MotionAxis::X, 50.0));
  EXPECT_NEAR(controller.position(MotionAxis::X).value(), 50.0, 1e-9);
}

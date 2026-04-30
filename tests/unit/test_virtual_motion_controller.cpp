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


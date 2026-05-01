#include <gtest/gtest.h>

#include "motion/VirtualMotionController.h"

namespace {
void tickUntilDone(VirtualMotionController &controller, MotionAxis axis, int maxTicks = 200) {
  for (int i = 0; i < maxTicks; ++i) {
    controller.tick(0.05);
    if (controller.getAxisState(axis) == AxisState::Done) break;
  }
}
} // namespace

TEST(VirtualMotionControllerTest, MovesAxisToAbsolutePosition) {
  VirtualMotionController controller;

  ASSERT_TRUE(controller.moveAbs(MotionAxis::CameraX, 125.0));
  tickUntilDone(controller, MotionAxis::CameraX);
  ASSERT_TRUE(controller.position(MotionAxis::CameraX).has_value());
  EXPECT_NEAR(controller.position(MotionAxis::CameraX).value(), 125.0, 1e-9);
}

TEST(VirtualMotionControllerTest, MovesAxisByRelativeOffset) {
  VirtualMotionController controller;

  ASSERT_TRUE(controller.moveAbs(MotionAxis::CameraY, 20.0));
  tickUntilDone(controller, MotionAxis::CameraY);
  ASSERT_TRUE(controller.moveRel(MotionAxis::CameraY, -5.5));
  tickUntilDone(controller, MotionAxis::CameraY);
  EXPECT_NEAR(controller.position(MotionAxis::CameraY).value(), 14.5, 1e-9);
}

TEST(VirtualMotionControllerTest, HomesAxisToZero) {
  VirtualMotionController controller;

  ASSERT_TRUE(controller.moveAbs(MotionAxis::Z, 42.0));
  tickUntilDone(controller, MotionAxis::Z);
  ASSERT_TRUE(controller.home(MotionAxis::Z));
  EXPECT_NEAR(controller.position(MotionAxis::Z).value(), 0.0, 1e-9);
}

TEST(VirtualMotionControllerTest, EmergencyStopBlocksMotionUntilReset) {
  VirtualMotionController controller;

  controller.emergencyStop();
  EXPECT_TRUE(controller.isStopped());
  EXPECT_TRUE(!controller.moveAbsolute(MotionAxis::CameraX, 50.0));

  controller.resetEmergencyStop();
  EXPECT_TRUE(!controller.isStopped());
  ASSERT_TRUE(controller.moveAbsolute(MotionAxis::CameraX, 50.0));
  tickUntilDone(controller, MotionAxis::CameraX);
  EXPECT_NEAR(controller.position(MotionAxis::CameraX).value(), 50.0, 1e-9);
}

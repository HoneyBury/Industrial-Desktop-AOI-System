#include <gtest/gtest.h>

#include "camera/VirtualCameraDevice.h"

#include <filesystem>

TEST(VirtualCameraDeviceTest, CropsDifferentBoardRegionsAsMotionChanges) {
  VirtualCameraDevice camera;
  camera.setBoardImagePath((std::filesystem::current_path() / "demoimage/board.png").string());
  camera.setBoardDefinition(BoardDefinition {260.0, 180.0, 32.0});
  camera.setScanRecipe(ScanRecipe {32.0, 24.0, ScanOrder::LeftToRight, true});
  camera.setPreferredFrameSize(640, 360);
  camera.setTransportState(BoardTransportState::BoardReady, 450.0, 450.0);

  ASSERT_TRUE(camera.open(0));

  camera.setCurrentPose(MechanicalPose {0.0, 0.0, 0.0, 0.0});
  const CameraFrame rightBottomFrame = camera.grabFrame();
  ASSERT_EQ(rightBottomFrame.width, 640);
  ASSERT_EQ(rightBottomFrame.height, 360);
  EXPECT_TRUE(camera.lastPoseInfo().boardVisible);
  EXPECT_TRUE(camera.lastPoseInfo().centerInsideBoard);
  EXPECT_NEAR(camera.lastPoseInfo().logicalBoardCenterXmm, 260.0, 1e-6);
  EXPECT_NEAR(camera.lastPoseInfo().logicalBoardCenterYmm, 180.0, 1e-6);

  camera.setCurrentPose(MechanicalPose {-220.0, -140.0, 0.0, 0.0});
  const CameraFrame upperLeftFrame = camera.grabFrame();
  ASSERT_EQ(upperLeftFrame.width, 640);
  ASSERT_EQ(upperLeftFrame.height, 360);
  EXPECT_TRUE(camera.lastPoseInfo().boardVisible);
  EXPECT_TRUE(camera.lastPoseInfo().centerInsideBoard);
  EXPECT_NEAR(camera.lastPoseInfo().logicalBoardCenterXmm, 40.0, 1e-6);
  EXPECT_NEAR(camera.lastPoseInfo().logicalBoardCenterYmm, 40.0, 1e-6);

  EXPECT_TRUE(!(rightBottomFrame.data == upperLeftFrame.data));
}

TEST(VirtualCameraDeviceTest, ReportsBoardOutsideCenterWhenLoadingHasNotReachedStopper) {
  VirtualCameraDevice camera;
  camera.setBoardImagePath((std::filesystem::current_path() / "demoimage/board.png").string());
  camera.setBoardDefinition(BoardDefinition {260.0, 180.0, 32.0});
  camera.setScanRecipe(ScanRecipe {32.0, 24.0, ScanOrder::LeftToRight, true});
  camera.setPreferredFrameSize(640, 360);
  camera.setTransportState(BoardTransportState::Loading, 0.0, 450.0);
  camera.setCurrentPose(MechanicalPose {0.0, 0.0, 0.0, 0.0});

  ASSERT_TRUE(camera.open(0));
  const CameraFrame frame = camera.grabFrame();
  ASSERT_EQ(frame.width, 640);
  ASSERT_EQ(frame.height, 360);
  EXPECT_TRUE(camera.lastPoseInfo().boardVisible);
  EXPECT_TRUE(!camera.lastPoseInfo().centerInsideBoard);
  EXPECT_TRUE(camera.lastPoseInfo().boardTransportOffsetXmm > 0.0);
}

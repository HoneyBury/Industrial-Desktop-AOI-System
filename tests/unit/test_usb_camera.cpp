#include <gtest/gtest.h>

#include "camera/UsbCamera.h"

TEST(UsbCameraTest, ReturnsNonEmptyCameraName) {
  UsbCamera camera;
  EXPECT_TRUE(!camera.cameraName().empty());
}

#ifndef AOI_HAS_OPENCV
TEST(UsbCameraTest, GeneratesSyntheticFrameWithoutOpenCv) {
  UsbCamera camera;

  ASSERT_TRUE(camera.open(0));
  ASSERT_TRUE(camera.isOpened());

  const CameraFrame frame = camera.grabFrame();
  EXPECT_EQ(frame.width, 640);
  EXPECT_EQ(frame.height, 360);
  EXPECT_EQ(frame.channels, 3);
  EXPECT_EQ(frame.pixelFormat, CameraPixelFormat::Rgb24);
  EXPECT_EQ(frame.data.size(), static_cast<std::size_t>(640 * 360 * 3));

  camera.close();
  EXPECT_TRUE(!camera.isOpened());
}
#endif

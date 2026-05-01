#include <gtest/gtest.h>

#include "program/ProgramManager.h"

#include <filesystem>

TEST(ProgramManagerTest, CreatesDefaultProgramTemplate) {
  ProgramManager manager;

  ASSERT_TRUE(manager.createDefaultProgram());
  ASSERT_TRUE(manager.currentProgram().has_value());
  EXPECT_EQ(manager.currentProgram()->name, std::string("default_demo_program"));
  EXPECT_EQ(manager.currentProgram()->marks.size(), static_cast<std::size_t>(2));
  EXPECT_EQ(manager.currentProgram()->rois.size(), static_cast<std::size_t>(2));
  EXPECT_EQ(manager.currentProgram()->marks.front().name, std::string("Mark-Left"));
  EXPECT_EQ(manager.currentProgram()->rois.back().name, std::string("Code-Area"));
  EXPECT_TRUE(manager.currentProgram()->cameraIntrinsicCalibration.calibrated);
  EXPECT_TRUE(manager.currentProgram()->pixelScaleCalibration.calibrated);
  EXPECT_EQ(manager.currentProgram()->markReferences.size(), static_cast<std::size_t>(2));
  EXPECT_EQ(manager.currentProgram()->roiDetectorConfigs.size(), static_cast<std::size_t>(2));
  EXPECT_EQ(manager.currentProgram()->roiDetectorConfigs.back().detectorType, RoiDetectorType::Code);
}

TEST(ProgramManagerTest, SavesAndLoadsProgramRoundTrip) {
  ProgramManager manager;
  ASSERT_TRUE(manager.createDefaultProgram());

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "program_manager_roundtrip.json";
  ASSERT_TRUE(manager.saveProgram(outputPath.string()));

  ProgramManager loader;
  const auto loadResult = loader.loadProgram(outputPath.string());
  ASSERT_TRUE(loadResult);
  EXPECT_EQ(loadResult.value.name, std::string("default_demo_program"));
  EXPECT_EQ(loadResult.value.aiModelPath, std::string("models/demo.onnx"));
  EXPECT_EQ(loadResult.value.marks.size(), static_cast<std::size_t>(2));
  EXPECT_EQ(loadResult.value.rois.size(), static_cast<std::size_t>(2));
  EXPECT_EQ(loadResult.value.marks.front().sampledColor, std::string("#ff4d4f"));
  EXPECT_EQ(loadResult.value.rois.front().shape, RoiShape::Rectangle);
  EXPECT_TRUE(loadResult.value.cameraIntrinsicCalibration.calibrated);
  EXPECT_NEAR(loadResult.value.cameraIntrinsicCalibration.cx, 640.0, 1e-9);
  EXPECT_NEAR(loadResult.value.pixelScaleCalibration.pixelToMillimeterX, 0.01, 1e-9);
  EXPECT_EQ(loadResult.value.markReferences.size(), static_cast<std::size_t>(2));
  EXPECT_NEAR(loadResult.value.markReferences.front().referenceProductPoint.x, 1.0, 1e-9);
  ASSERT_EQ(loadResult.value.roiDetectorConfigs.size(), static_cast<std::size_t>(2));
  EXPECT_EQ(loadResult.value.roiDetectorConfigs.front().detectorType, RoiDetectorType::Geometry);
  EXPECT_EQ(loadResult.value.roiDetectorConfigs.back().detectorType, RoiDetectorType::Code);
  EXPECT_EQ(loadResult.value.codeRegionName, std::string("Code-Area"));
  EXPECT_NEAR(loadResult.value.calibrationData.pixelToMillimeterY, 0.01, 1e-9);
  EXPECT_EQ(loadResult.value.filePath, outputPath.string());

  std::filesystem::remove(outputPath);
}

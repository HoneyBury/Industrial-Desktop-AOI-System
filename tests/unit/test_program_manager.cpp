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
  EXPECT_EQ(manager.currentProgram()->laserPointTasks.size(), static_cast<std::size_t>(2));
  EXPECT_EQ(manager.currentProgram()->roiDetectorConfigs.back().detectorType, RoiDetectorType::Code);
  EXPECT_NEAR(manager.currentProgram()->boardDefinition.boardLengthMm, 260.0, 1e-9);
  EXPECT_NEAR(manager.currentProgram()->boardDefinition.railWidthMm, 32.0, 1e-9);
  EXPECT_EQ(manager.currentProgram()->scanRecipe.scanOrder, ScanOrder::LeftToRight);
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
  ASSERT_EQ(loadResult.value.laserPointTasks.size(), static_cast<std::size_t>(2));
  EXPECT_EQ(loadResult.value.roiDetectorConfigs.front().detectorType, RoiDetectorType::Geometry);
  EXPECT_EQ(loadResult.value.roiDetectorConfigs.back().detectorType, RoiDetectorType::Code);
  EXPECT_EQ(loadResult.value.laserPointTasks.front().linkedRoiName, std::string("Inspect-Top"));
  EXPECT_EQ(loadResult.value.codeRegionName, std::string("Code-Area"));
  EXPECT_NEAR(loadResult.value.calibrationData.pixelToMillimeterY, 0.01, 1e-9);
  EXPECT_EQ(loadResult.value.filePath, outputPath.string());
  EXPECT_NEAR(loadResult.value.boardDefinition.boardWidthMm, 180.0, 1e-9);
  EXPECT_NEAR(loadResult.value.scanRecipe.fovWidthMm, 32.0, 1e-9);

  std::filesystem::remove(outputPath);
}

TEST(ProgramManagerTest, PreservesNestedCalibrationAndRuntimeSummaryFields) {
  ProgramManager manager;
  ASSERT_TRUE(manager.createDefaultProgram());

  ProgramModel customized = *manager.currentProgram();
  customized.name = "demo \"program\"\nA";
  customized.cameraIntrinsicCalibration.calibrated = false;
  customized.cameraIntrinsicCalibration.fx = 888.0;
  customized.originCalibration.calibrated = true;
  customized.originCalibration.imageReferencePixel = PixelPoint {12.5, 24.5};
  customized.originCalibration.machineReferencePose = MechanicalPose {1.2, 3.4, 5.6, 7.8};
  customized.laserOffsetCalibration.calibrated = true;
  customized.laserOffsetCalibration.cameraToLaserDxMm = 0.42;
  customized.laserOffsetCalibration.cameraToLaserDyMm = -0.37;
  customized.runtimeSummary.templateCachePath = "/tmp/template \"cache\".png";
  customized.runtimeSummary.latestTemplateMatchSummary = "match\nsummary";
  customized.runtimeSummary.hasOriginCalibration = true;
  customized.runtimeSummary.originCorrectedPose = MechanicalPose {8.1, 9.2, 10.3, 11.4};
  customized.runtimeSummary.hasMarkCalibration = true;
  customized.runtimeSummary.markCalibrationOffsetXmm = 0.11;
  customized.runtimeSummary.markCalibrationOffsetYmm = -0.22;
  customized.runtimeSummary.markCalibrationRotationDegrees = 1.5;
  customized.boardDefinition = BoardDefinition {520.0, 410.0, 55.0};
  customized.scanRecipe = ScanRecipe {44.0, 33.0, ScanOrder::TopToBottom, true};
  customized.runtimeSummary.wholeBoardImagePath = "/tmp/whole_board.png";
  customized.runtimeSummary.scanTileRows = 7;
  customized.runtimeSummary.scanTileColumns = 9;
  customized.runtimeSummary.lastBoardScanSummary = "scan complete";
  customized.laserPointTasks = {
      {"Laser-A", 12.0, 18.0, "Inspect-Top", "CODE-123", true},
      {"Laser-B", 30.0, 42.0, "Code-Area", "CODE-456", false},
  };
  ASSERT_TRUE(manager.createProgram(customized));

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "program_manager_nested_roundtrip.json";
  ASSERT_TRUE(manager.saveProgram(outputPath.string()));

  ProgramManager loader;
  const auto loadResult = loader.loadProgram(outputPath.string());
  ASSERT_TRUE(loadResult);
  EXPECT_EQ(loadResult.value.name, customized.name);
  EXPECT_TRUE(!loadResult.value.cameraIntrinsicCalibration.calibrated);
  EXPECT_NEAR(loadResult.value.cameraIntrinsicCalibration.fx, 888.0, 1e-9);
  EXPECT_TRUE(loadResult.value.originCalibration.calibrated);
  EXPECT_NEAR(loadResult.value.originCalibration.imageReferencePixel.x, 12.5, 1e-9);
  EXPECT_NEAR(loadResult.value.originCalibration.machineReferencePose.r, 7.8, 1e-9);
  EXPECT_TRUE(loadResult.value.laserOffsetCalibration.calibrated);
  EXPECT_NEAR(loadResult.value.laserOffsetCalibration.cameraToLaserDxMm, 0.42, 1e-9);
  EXPECT_NEAR(loadResult.value.runtimeSummary.originCorrectedPose.z, 10.3, 1e-9);
  EXPECT_EQ(loadResult.value.runtimeSummary.templateCachePath, customized.runtimeSummary.templateCachePath);
  EXPECT_EQ(loadResult.value.runtimeSummary.latestTemplateMatchSummary,
            customized.runtimeSummary.latestTemplateMatchSummary);
  EXPECT_TRUE(loadResult.value.runtimeSummary.hasMarkCalibration);
  EXPECT_NEAR(loadResult.value.runtimeSummary.markCalibrationRotationDegrees, 1.5, 1e-9);
  EXPECT_NEAR(loadResult.value.boardDefinition.boardLengthMm, 520.0, 1e-9);
  EXPECT_NEAR(loadResult.value.boardDefinition.railWidthMm, 55.0, 1e-9);
  EXPECT_EQ(loadResult.value.scanRecipe.scanOrder, ScanOrder::TopToBottom);
  EXPECT_EQ(loadResult.value.runtimeSummary.wholeBoardImagePath, std::string("/tmp/whole_board.png"));
  EXPECT_EQ(loadResult.value.runtimeSummary.scanTileRows, 7);
  EXPECT_EQ(loadResult.value.runtimeSummary.scanTileColumns, 9);
  ASSERT_EQ(loadResult.value.laserPointTasks.size(), static_cast<std::size_t>(2));
  EXPECT_EQ(loadResult.value.laserPointTasks.front().expectedCodeText, std::string("CODE-123"));
  EXPECT_TRUE(!loadResult.value.laserPointTasks.back().enabled);

  std::filesystem::remove(outputPath);
}

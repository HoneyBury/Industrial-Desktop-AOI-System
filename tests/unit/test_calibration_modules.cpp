#include <gtest/gtest.h>

#include "calibration/CameraIntrinsicCalibrator.h"
#include "calibration/LaserOffsetCalibrator.h"
#include "calibration/LaserOffsetCalibrationModule.h"
#include "calibration/MarkReferenceCalibrator.h"
#include "calibration/OriginCalibrator.h"
#include "calibration/OriginCalibrationModule.h"
#include "calibration/PixelScaleCalibrator.h"
#include "calibration/PixelToMachineCalibrationModule.h"
#include "coordinate/CoordinateTransformer.h"

#include <filesystem>

TEST(CalibrationModulesTest, CalibratesPixelScaleFromKnownDistance) {
  calibration::PixelScaleCalibrator calibrator;
  const auto result = calibrator.calibrateFromKnownDistance(200.0, 100.0, 10.0, 5.0);

  ASSERT_TRUE(result);
  EXPECT_TRUE(result.value.calibrated);
  EXPECT_NEAR(result.value.pixelToMillimeterX, 0.05, 1e-9);
  EXPECT_NEAR(result.value.pixelToMillimeterY, 0.05, 1e-9);
}

TEST(CalibrationModulesTest, ComputesOriginCorrection) {
  calibration::OriginCalibrator calibrator;
  const auto result = calibrator.calibrate(calibration::OriginCalibrationInput {
      PixelPoint {660.0, 350.0},
      PixelPoint {640.0, 360.0},
      PixelScaleCalibration {true, 0.01, 0.02},
      MechanicalPose {100.0, 200.0, 0.0, 0.0},
  });

  ASSERT_TRUE(result);
  EXPECT_TRUE(result.value.calibration.calibrated);
  EXPECT_NEAR(result.value.pixelOffset.x, 20.0, 1e-9);
  EXPECT_NEAR(result.value.pixelOffset.y, -10.0, 1e-9);
  EXPECT_NEAR(result.value.millimeterOffset.x, 0.2, 1e-9);
  EXPECT_NEAR(result.value.millimeterOffset.y, -0.2, 1e-9);
  EXPECT_NEAR(result.value.correctedPose.x, 99.8, 1e-9);
  EXPECT_NEAR(result.value.correctedPose.y, 200.2, 1e-9);
}

TEST(CalibrationModulesTest, ComputesLaserOffset) {
  calibration::LaserOffsetCalibrator calibrator;
  const auto result = calibrator.calibrate(calibration::LaserOffsetCalibrationInput {
      PixelPoint {650.0, 368.0},
      PixelPoint {640.0, 360.0},
      PixelScaleCalibration {true, 0.01, 0.02},
  });

  ASSERT_TRUE(result);
  EXPECT_TRUE(result.value.calibration.calibrated);
  EXPECT_NEAR(result.value.calibration.cameraToLaserDxMm, 0.1, 1e-9);
  EXPECT_NEAR(result.value.calibration.cameraToLaserDyMm, 0.16, 1e-9);
}

TEST(CalibrationModulesTest, RecordsMarkReferences) {
  calibration::MarkReferenceCalibrator calibrator;
  const auto result = calibrator.recordReferences(
      {
          {"Mark-1", 100.0, 50.0, 40.0, 40.0, 0.0, 0.9, 0.8, 0.85, 10, "#ffffff",
           MarkShape::Circle, MarkAlgorithm::BinaryGeometry, true},
          {"Mark-2", 200.0, 70.0, 40.0, 40.0, 0.0, 0.9, 0.8, 0.85, 10, "#ffffff",
           MarkShape::Circle, MarkAlgorithm::BinaryGeometry, true},
      },
      PixelScaleCalibration {true, 0.01, 0.02});

  ASSERT_TRUE(result);
  ASSERT_EQ(result.value.size(), static_cast<std::size_t>(2));
  EXPECT_NEAR(result.value[0].referenceProductPoint.x, 1.0, 1e-9);
  EXPECT_NEAR(result.value[0].referenceProductPoint.y, 1.0, 1e-9);
}

TEST(CalibrationModulesTest, SavesAndLoadsIntrinsicCalibration) {
  calibration::CameraIntrinsicCalibrator calibrator;
  const auto calibrationResult = calibrator.calibrateFromChessboard("tests/data/chessboard");
  ASSERT_TRUE(calibrationResult);

  const std::filesystem::path outputPath =
      std::filesystem::current_path() / "camera_intrinsic_calibration.txt";
  ASSERT_TRUE(calibrator.save(calibrationResult.value, outputPath.string()));

  const auto loadResult = calibrator.load(outputPath.string());
  ASSERT_TRUE(loadResult);
  EXPECT_TRUE(loadResult.value.calibrated);
  EXPECT_NEAR(loadResult.value.fx, 1000.0, 1e-9);
  EXPECT_EQ(loadResult.value.distortionCoefficients.size(), static_cast<std::size_t>(4));

  std::filesystem::remove(outputPath);
}

// ── OriginCalibrationModule ──

TEST(CalibrationModulesTest, OriginModuleStartsEmptyAndTracksState) {
  calibration::OriginCalibrationModule module;
  EXPECT_TRUE(!module.hasReference());
  EXPECT_TRUE(!module.hasUnappliedChanges());

  module.setBoardDefinition(200.0, 150.0);
  module.setReferencePose({120.0, 80.0, 10.0, 0.5});
  EXPECT_TRUE(module.hasReference());
  EXPECT_TRUE(module.hasUnappliedChanges());
}

TEST(CalibrationModulesTest, OriginModuleCalculatesLogicalOrigin) {
  calibration::OriginCalibrationModule module;
  module.setBoardDefinition(200.0, 150.0);
  module.setReferencePose({250.0, 180.0, 10.0, 0.5});

  const auto origin = module.calculateLogicalOrigin();
  EXPECT_NEAR(origin.x, 50.0, 1e-9);
  EXPECT_NEAR(origin.y, 30.0, 1e-9);
  EXPECT_NEAR(origin.z, 10.0, 1e-9);
  EXPECT_NEAR(origin.r, 0.5, 1e-9);
}

TEST(CalibrationModulesTest, OriginModuleApplyProducesCalibration) {
  calibration::OriginCalibrationModule module;
  module.setBoardDefinition(200.0, 150.0);
  module.setReferencePose({250.0, 180.0, 10.0, 0.5});
  module.setReferencePixel(640.0, 360.0);

  const auto cal = module.apply();
  EXPECT_TRUE(cal.calibrated);
  EXPECT_NEAR(cal.machineReferencePose.x, 250.0, 1e-9);
  EXPECT_NEAR(cal.machineReferencePose.y, 180.0, 1e-9);
  EXPECT_NEAR(cal.imageReferencePixel.x, 640.0, 1e-9);

  EXPECT_TRUE(!module.hasUnappliedChanges());
}

TEST(CalibrationModulesTest, OriginModuleResetClearsState) {
  calibration::OriginCalibrationModule module;
  module.setBoardDefinition(200.0, 150.0);
  module.setReferencePose({250.0, 180.0, 10.0, 0.5});
  EXPECT_TRUE(module.hasReference());

  module.reset();
  EXPECT_TRUE(!module.hasReference());
  EXPECT_TRUE(!module.hasUnappliedChanges());
  EXPECT_NEAR(module.referenceMachinePose().x, 0.0, 1e-9);
}

// ── LaserOffsetCalibrationModule ──

TEST(CalibrationModulesTest, LaserOffsetModuleStartsEmpty) {
  calibration::LaserOffsetCalibrationModule module;
  EXPECT_TRUE(!module.hasCameraPoint());
  EXPECT_TRUE(!module.hasLaserPoint());
  EXPECT_TRUE(!module.hasComputedResult());
  EXPECT_TRUE(!module.hasUnappliedResult());
}

TEST(CalibrationModulesTest, LaserOffsetModuleRecordsAndComputes) {
  calibration::LaserOffsetCalibrationModule module;
  module.setPixelScale({true, 0.01, 0.01});

  module.recordCameraPoint({100.0, 200.0, 10.0, 0.0});
  EXPECT_TRUE(module.hasCameraPoint());

  module.recordLaserPoint({105.0, 198.0, 10.0, 0.0});
  EXPECT_TRUE(module.hasCameraPoint());
  EXPECT_TRUE(module.hasLaserPoint());

  const auto result = module.computeOffset();
  EXPECT_TRUE(result.calibrated);
  EXPECT_NEAR(result.cameraToLaserDxMm, 5.0, 1e-9);
  EXPECT_NEAR(result.cameraToLaserDyMm, -2.0, 1e-9);
  EXPECT_TRUE(module.hasComputedResult());
  EXPECT_TRUE(module.hasUnappliedResult());
}

TEST(CalibrationModulesTest, LaserOffsetModuleApplyAndClear) {
  calibration::LaserOffsetCalibrationModule module;
  module.recordCameraPoint({100.0, 200.0, 10.0, 0.0});
  module.recordLaserPoint({105.0, 198.0, 10.0, 0.0});
  (void)module.computeOffset();

  const auto cal = module.apply();
  EXPECT_TRUE(cal.calibrated);
  EXPECT_TRUE(!module.hasUnappliedResult());
  EXPECT_TRUE(module.hasComputedResult());

  module.clear();
  EXPECT_TRUE(!module.hasCameraPoint());
  EXPECT_TRUE(!module.hasLaserPoint());
  EXPECT_TRUE(!module.hasComputedResult());
  EXPECT_TRUE(!module.hasUnappliedResult());
}

TEST(CalibrationModulesTest, LaserOffsetModuleComputeWithoutBothPointsReturnsEmpty) {
  calibration::LaserOffsetCalibrationModule module;
  module.recordCameraPoint({100.0, 200.0, 0.0, 0.0});

  const auto result = module.computeOffset();
  EXPECT_TRUE(!result.calibrated);
  EXPECT_NEAR(result.cameraToLaserDxMm, 0.0, 1e-9);
}

// ── PixelToMachineCalibrationModule ──

TEST(CalibrationModulesTest, PixelToMachineModuleInferFromFov) {
  calibration::PixelToMachineCalibrationModule module;
  EXPECT_TRUE(!module.isCalibrated());

  module.inferFromFov(12.8, 7.2, 1280, 720);
  EXPECT_TRUE(module.isCalibrated());
  EXPECT_NEAR(module.pixelScale().pixelToMillimeterX, 0.01, 1e-9);
  EXPECT_NEAR(module.pixelScale().pixelToMillimeterY, 0.01, 1e-9);
}

TEST(CalibrationModulesTest, PixelToMachineModuleConvertsOffsets) {
  calibration::PixelToMachineCalibrationModule module;
  module.setPixelScale({true, 0.01, 0.02});

  const auto mm = module.pixelOffsetToMm(100.0, 50.0);
  EXPECT_NEAR(mm.x, 1.0, 1e-9);
  EXPECT_NEAR(mm.y, 1.0, 1e-9);

  const auto px = module.mmToPixelOffset(1.0, 1.0);
  EXPECT_NEAR(px.x, 100.0, 1e-9);
  EXPECT_NEAR(px.y, 50.0, 1e-9);
}

// ── CoordinateTransformer new methods ──

TEST(CoordinateTransformerTest, MachineToProductReversesProductToMachine) {
  CoordinateTransformer xf;
  CoordinateTransformChain chain;
  chain.productToMachine = RigidTransform2D{5.0, 10.0, 45.0};
  xf.setChain(chain);

  const MechanicalPose originPose{100.0, 200.0, 0.0, 0.0};
  const MillimeterPoint productPoint{20.0, 10.0};

  const MechanicalPose machinePose = xf.productToMechanical(productPoint, originPose);
  const MillimeterPoint recovered = xf.machineToProduct(machinePose, originPose);
  EXPECT_NEAR(recovered.x, productPoint.x, 1e-6);
  EXPECT_NEAR(recovered.y, productPoint.y, 1e-6);
}

TEST(CoordinateTransformerTest, ImageClickToMoveDeltaComputesCorrectly) {
  CoordinateTransformer xf(0.01, 0.01);

  const auto delta = xf.imageClickToMoveDelta({740.0, 260.0}, {640.0, 360.0});
  EXPECT_NEAR(delta.x, 1.0, 1e-9);
  EXPECT_NEAR(delta.y, -1.0, 1e-9);
}

TEST(CoordinateTransformerTest, ApplyLaserOffsetAddsToPose) {
  CoordinateTransformer xf;
  const MechanicalPose original{100.0, 200.0, 10.0, 0.0};

  const auto result = xf.applyLaserOffset(original, {5.0, -3.0});
  EXPECT_NEAR(result.x, 105.0, 1e-9);
  EXPECT_NEAR(result.y, 197.0, 1e-9);
  EXPECT_NEAR(result.z, 10.0, 1e-9);
}

TEST(CoordinateTransformerTest, TargetProductPointToLaserPosition) {
  CoordinateTransformer xf;
  CoordinateTransformChain chain;
  chain.productToMachine = RigidTransform2D{0.0, 0.0, 0.0};
  xf.setChain(chain);

  const MechanicalPose originPose{50.0, 50.0, 0.0, 0.0};
  const auto laserPose = xf.targetProductPointToLaserPosition({30.0, 20.0}, originPose, {5.0, -3.0});

  EXPECT_NEAR(laserPose.x, 85.0, 1e-9);
  EXPECT_NEAR(laserPose.y, 67.0, 1e-9);
}

TEST(CoordinateTransformerTest, ApplyMarkTransform) {
  const RigidTransform2D markShift{2.0, -1.0, 0.0};
  const auto shifted = CoordinateTransformer::applyMarkTransform({15.0, 10.0}, markShift);
  EXPECT_NEAR(shifted.x, 17.0, 1e-9);
  EXPECT_NEAR(shifted.y, 9.0, 1e-9);
}

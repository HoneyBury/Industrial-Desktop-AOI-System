#include <gtest/gtest.h>

#include "calibration/CameraIntrinsicCalibrator.h"
#include "calibration/LaserOffsetCalibrator.h"
#include "calibration/MarkReferenceCalibrator.h"
#include "calibration/OriginCalibrator.h"
#include "calibration/PixelScaleCalibrator.h"

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

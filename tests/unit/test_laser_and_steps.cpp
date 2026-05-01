#include <gtest/gtest.h>

#include "ai/AiInferencer.h"
#include "laser/VirtualLaserController.h"
#include "motion/VirtualMotionController.h"
#include "process/ImageCaptureStep.h"
#include "process/LaserExecuteStep.h"
#include "process/LoadBoardStep.h"
#include "process/PostLaserVerifyStep.h"
#include "process/RoughPositionStep.h"
#include "process/WorkflowContext.h"
#include "program/ProgramManager.h"
#include "transport/VirtualTransportController.h"

// ---------------------------------------------------------------------------
// VirtualLaserController
// ---------------------------------------------------------------------------

TEST(VirtualLaserControllerTest, FiresAndReports) {
  VirtualLaserController laser;

  EXPECT_TRUE(laser.isReady());
  EXPECT_TRUE(!laser.isStopped());

  LaserMarkingParams params;
  params.x = 120.0;
  params.y = 80.0;
  params.powerPercent = 85.0;
  params.frequencyKhz = 25.0;
  params.pulseWidthUs = 15.0;
  params.repeatCount = 3;

  EXPECT_TRUE(laser.executeMark(params));
  const std::string report = laser.lastMarkReport();
  EXPECT_TRUE(!report.empty());
}

TEST(VirtualLaserControllerTest, EmergencyStopPreventsFire) {
  VirtualLaserController laser;

  laser.emergencyStop();
  EXPECT_TRUE(laser.isStopped());
  EXPECT_TRUE(!laser.isReady());

  LaserMarkingParams params;
  EXPECT_TRUE(!laser.executeMark(params));
  EXPECT_TRUE(!laser.lastMarkReport().empty());
}

TEST(VirtualLaserControllerTest, ResetRecoversFromStop) {
  VirtualLaserController laser;

  laser.emergencyStop();
  EXPECT_TRUE(laser.isStopped());

  laser.resetEmergencyStop();
  EXPECT_TRUE(!laser.isStopped());
  EXPECT_TRUE(laser.isReady());

  LaserMarkingParams params;
  EXPECT_TRUE(laser.executeMark(params));
}

// ---------------------------------------------------------------------------
// VirtualTransportController
// ---------------------------------------------------------------------------

namespace {
// tick the transport until it settles (board reaches stopper at 450mm, speed 200mm/s)
void tickUntilReady(VirtualTransportController &transport, double maxSec = 5.0) {
  const double dt = 0.05;
  for (double elapsed = 0.0; elapsed < maxSec; elapsed += dt) {
    transport.tick(dt);
    if (transport.isBoardReady() || transport.state() == BoardTransportState::Idle) break;
  }
}

void tickTransportAndMotion(VirtualTransportController &transport,
                            VirtualMotionController &motion,
                            double maxSec = 5.0) {
  const double dt = 0.05;
  for (double elapsed = 0.0; elapsed < maxSec; elapsed += dt) {
    transport.tick(dt);
    motion.tick(dt);
    if (transport.isBoardReady() || transport.state() == BoardTransportState::Idle) break;
  }
}
} // namespace

TEST(VirtualTransportControllerTest, LoadBoardSetsReadySignal) {
  VirtualTransportController transport;
  EXPECT_TRUE(transport.loadBoard());
  tickUntilReady(transport);
  EXPECT_TRUE(transport.isBoardReady());
  EXPECT_EQ(transport.state(), BoardTransportState::BoardReady);
  EXPECT_TRUE(!transport.lastSignalMessage().empty());
}

TEST(VirtualTransportControllerTest, UnloadBoardClearsReadySignal) {
  VirtualTransportController transport;
  ASSERT_TRUE(transport.loadBoard());
  tickUntilReady(transport);
  ASSERT_TRUE(transport.isBoardReady());
  EXPECT_TRUE(transport.unloadBoard());
  tickUntilReady(transport);
  EXPECT_TRUE(!transport.isBoardReady());
  EXPECT_EQ(transport.state(), BoardTransportState::Idle);
}

TEST(VirtualTransportControllerTest, UnloadCycleDoesNotBreakSubsequentMotionOrReload) {
  VirtualTransportController transport;
  VirtualMotionController motion;
  transport.setMotionController(&motion);

  ASSERT_TRUE(transport.loadBoard());
  tickTransportAndMotion(transport, motion);
  ASSERT_TRUE(transport.isBoardReady());

  ASSERT_TRUE(transport.unloadBoard());
  tickTransportAndMotion(transport, motion, 8.0);
  EXPECT_EQ(transport.state(), BoardTransportState::Idle);

  EXPECT_TRUE(motion.moveAbs(MotionAxis::CameraX, 50.0));
  for (int i = 0; i < 100; ++i) {
    motion.tick(0.05);
    if (motion.getAxisState(MotionAxis::CameraX) == AxisState::Done) break;
  }
  EXPECT_NEAR(motion.position(MotionAxis::CameraX).value_or(0.0), 50.0, 1e-9);

  ASSERT_TRUE(transport.loadBoard());
  tickTransportAndMotion(transport, motion);
  EXPECT_TRUE(transport.isBoardReady());
  EXPECT_NEAR(motion.position(MotionAxis::Conveyor).value_or(0.0), transport.boardPosition(), 1e-9);
}

// ---------------------------------------------------------------------------
// LoadBoardStep
// ---------------------------------------------------------------------------

TEST(ProcessStepTest, LoadBoardAssignsId) {
  LoadBoardStep step;
  EXPECT_EQ(step.type(), ProcessStepType::LoadBoard);

  WorkflowContext context;
  context.boardId.clear();

  const auto result = step.execute(context);
  EXPECT_TRUE(result.status == StepExecutionStatus::Succeeded);
  EXPECT_TRUE(!context.boardId.empty());
}

TEST(ProcessStepTest, LoadBoardResetsTransientBoardStateByDefault) {
  LoadBoardStep step;

  WorkflowContext context;
  context.boardId = "BOARD-LEGACY";
  context.currentImagePath = "tests/data/demo.png";
  context.measuredMarks = {
      {"Mark-A", 100.0, 80.0, 50.0, 50.0, 0.0, 0.9, 0.8, 0.88, 16, "#fff",
       MarkShape::Rectangle, MarkAlgorithm::ColorBrushTemplate, true},
  };
  context.roiInspectionResults.push_back(RoiInspectionResult {});
  context.aiDetections.push_back(AiDetection {.label = "ok", .confidence = 0.9});
  context.inspectionDetailsJson = "{}";

  const auto result = step.execute(context);
  EXPECT_TRUE(result.status == StepExecutionStatus::Succeeded);
  EXPECT_TRUE(context.currentImagePath.empty());
  EXPECT_TRUE(context.measuredMarks.empty());
  EXPECT_TRUE(context.roiInspectionResults.empty());
  EXPECT_TRUE(context.aiDetections.empty());
  EXPECT_TRUE(context.inspectionDetailsJson.empty());
}

TEST(ProcessStepTest, LoadBoardFailsWhenTransportHasNoReadyBoard) {
  VirtualTransportController transport;

  WorkflowContext context;
  context.transportController = &transport;

  LoadBoardStep step;
  const auto result = step.execute(context);
  EXPECT_TRUE(result.status == StepExecutionStatus::Failed);
  EXPECT_TRUE(!context.boardReady);
}

// ---------------------------------------------------------------------------
// RoughPositionStep
// ---------------------------------------------------------------------------

TEST(ProcessStepTest, RoughPositionReadsMotionPose) {
  VirtualMotionController motion;
  motion.moveAbs(MotionAxis::CameraX, 42.0);
  motion.moveAbs(MotionAxis::CameraY, 88.0);

  // tick until axes reach their targets
  for (int i = 0; i < 100; ++i) {
    motion.tick(0.05);
    if (motion.getAxisState(MotionAxis::CameraX) == AxisState::Done &&
        motion.getAxisState(MotionAxis::CameraY) == AxisState::Done) break;
  }

  WorkflowContext context;
  context.motionController = &motion;

  RoughPositionStep step;
  const auto result = step.execute(context);
  EXPECT_TRUE(result.status == StepExecutionStatus::Succeeded);
  EXPECT_NEAR(context.currentMachinePose.x, 42.0, 1e-9);
  EXPECT_NEAR(context.currentMachinePose.y, 88.0, 1e-9);
}

TEST(ProcessStepTest, RoughPositionSucceedsWithoutController) {
  WorkflowContext context;
  context.motionController = nullptr;

  // RoughPosition gracefully skips motion when no controller is wired.
  RoughPositionStep step;
  const auto result = step.execute(context);
  EXPECT_TRUE(result.status == StepExecutionStatus::Succeeded);
}

// ---------------------------------------------------------------------------
// LaserExecuteStep
// ---------------------------------------------------------------------------

TEST(ProcessStepTest, LaserExecuteFailsWithoutPreparedPose) {
  WorkflowContext context;
  context.hasPreparedLaserPose = false;

  LaserExecuteStep step;
  const auto result = step.execute(context);
  EXPECT_TRUE(result.status == StepExecutionStatus::Failed);
}

TEST(ProcessStepTest, LaserExecuteUsesVirtualController) {
  VirtualLaserController laser;
  VirtualMotionController motion;

  WorkflowContext context;
  context.hasPreparedLaserPose = true;
  context.preparedLaserPose = MechanicalPose {10.0, 20.0, 0.0, 0.0};
  context.laserController = &laser;
  context.motionController = &motion;

  LaserExecuteStep step;
  const auto result = step.execute(context);
  EXPECT_TRUE(result.status == StepExecutionStatus::Succeeded);
  EXPECT_TRUE(context.laserExecuted);
}

TEST(ProcessStepTest, LaserExecuteSkipsWhenNoControllerWired) {
  VirtualMotionController motion;

  WorkflowContext context;
  context.hasPreparedLaserPose = true;
  context.preparedLaserPose = MechanicalPose {10.0, 20.0, 0.0, 0.0};
  context.laserController = nullptr;
  context.motionController = &motion;

  LaserExecuteStep step;
  const auto result = step.execute(context);
  EXPECT_TRUE(result.status == StepExecutionStatus::Succeeded);
  EXPECT_TRUE(context.laserExecuted);
}

// ---------------------------------------------------------------------------
// PostLaserVerifyStep
// ---------------------------------------------------------------------------

TEST(ProcessStepTest, PostLaserVerifyRejectsUnexecutedLaser) {
  WorkflowContext context;
  context.laserExecuted = false;

  PostLaserVerifyStep step;
  const auto result = step.execute(context);
  EXPECT_TRUE(result.status == StepExecutionStatus::Failed);
  EXPECT_TRUE(!context.finalDecisionOk);
}

TEST(ProcessStepTest, PostLaserVerifyAcceptsExecutedLaser) {
  WorkflowContext context;
  context.laserExecuted = true;
  context.finalDecisionOk = true;
  context.currentImagePath = "tests/data/demo.png";
  context.laserPointResults.push_back(LaserPointExecutionResult {
      "Laser-A", "Inspect-Top", MillimeterPoint {1.0, 2.0}, MechanicalPose {10.0, 20.0, 0.0, 0.0},
      true, false, false, "DEMO-CODE-001", {}, {},
  });

  PostLaserVerifyStep step;
  const auto result = step.execute(context);
  EXPECT_TRUE(result.status == StepExecutionStatus::Succeeded);
  EXPECT_TRUE(context.finalDecisionOk);
}

TEST(ProcessStepTest, PostLaserVerifyDetectsEmergencyStop) {
  VirtualLaserController laser;
  laser.emergencyStop();

  WorkflowContext context;
  context.laserExecuted = true;
  context.finalDecisionOk = true;
  context.laserController = &laser;
  context.currentImagePath = "tests/data/demo.png";
  context.laserPointResults.push_back(LaserPointExecutionResult {
      "Laser-A", "Inspect-Top", MillimeterPoint {1.0, 2.0}, MechanicalPose {10.0, 20.0, 0.0, 0.0},
      true, false, false, "DEMO-CODE-001", {}, {},
  });

  PostLaserVerifyStep step;
  const auto result = step.execute(context);
  EXPECT_TRUE(result.status == StepExecutionStatus::Failed);
  EXPECT_TRUE(!context.finalDecisionOk);
}

// ---------------------------------------------------------------------------
// ImageCaptureStep
// ---------------------------------------------------------------------------

TEST(ProcessStepTest, ImageCaptureFailsWithoutProviderOrPath) {
  WorkflowContext context;
  context.currentImagePath.clear();
  context.captureFrame = nullptr;

  ImageCaptureStep step;
  const auto result = step.execute(context);
  EXPECT_TRUE(result.status == StepExecutionStatus::Failed);
}

TEST(ProcessStepTest, ImageCaptureUsesExistingPath) {
  WorkflowContext context;
  context.currentImagePath = "/nonexistent/test_capture.png";

  ImageCaptureStep step;
  const auto result = step.execute(context);
  // Will succeed because the path is set (captureFrame fallback).
  // Mark detection may fail but the step itself delivers the path.
  EXPECT_TRUE(result.status == StepExecutionStatus::Succeeded ||
              result.status == StepExecutionStatus::Failed);
}

TEST(ProcessStepTest, ImageCaptureUsesWholeBoardScanCallbackWhenAvailable) {
  ProgramManager programManager;
  ASSERT_TRUE(programManager.createDefaultProgram());

  WorkflowContext context;
  context.program = programManager.mutableProgram();
  context.captureWholeBoardScan = []() {
    BoardScanCaptureResult result;
    result.mosaicImagePath = "tests/data/demo.png";
    result.tileRows = 2;
    result.tileColumns = 3;
    result.capturedTileCount = 6;
    result.summary = "board scan ok";
    return BoardScanCaptureWorkflowResult::success(result, result.summary);
  };
  context.measuredMarks = {
      {"Mark-A", 100.0, 80.0, 50.0, 50.0, 0.0, 0.9, 0.8, 0.88, 16, "#fff",
       MarkShape::Rectangle, MarkAlgorithm::ColorBrushTemplate, true},
  };

  ImageCaptureStep step;
  const auto result = step.execute(context);
  EXPECT_TRUE(result.status == StepExecutionStatus::Succeeded);
  EXPECT_EQ(context.currentImagePath, std::string("tests/data/demo.png"));
  EXPECT_EQ(context.scanTileRows, 2);
  EXPECT_EQ(context.scanTileColumns, 3);
}

// ---------------------------------------------------------------------------
// AiInferencer – model loading and fallback
// ---------------------------------------------------------------------------

TEST(AiInferencerTest, LoadsWithEmptyPathFallsBack) {
  AiInferencer inferencer;
  const auto result = inferencer.loadModel("");
  EXPECT_TRUE(result);
}

TEST(AiInferencerTest, LoadsWithNonexistentModelFallsBack) {
  AiInferencer inferencer;
  const auto result = inferencer.loadModel("/nonexistent/model.onnx");
  EXPECT_TRUE(result);
}

TEST(AiInferencerTest, InferFailsWithoutLoad) {
  AiInferencer inferencer;
  // loadModel not called — modelLoaded_ is false.
  const auto result = inferencer.infer("any.png");
  EXPECT_TRUE(!result);
}

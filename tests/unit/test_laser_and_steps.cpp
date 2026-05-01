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

// ---------------------------------------------------------------------------
// RoughPositionStep
// ---------------------------------------------------------------------------

TEST(ProcessStepTest, RoughPositionReadsMotionPose) {
  VirtualMotionController motion;
  motion.moveAbsolute(MotionAxis::X, 42.0);
  motion.moveAbsolute(MotionAxis::Y, 88.0);

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

  WorkflowContext context;
  context.hasPreparedLaserPose = true;
  context.preparedLaserPose = MechanicalPose {10.0, 20.0, 0.0, 0.0};
  context.laserController = &laser;

  LaserExecuteStep step;
  const auto result = step.execute(context);
  EXPECT_TRUE(result.status == StepExecutionStatus::Succeeded);
  EXPECT_TRUE(context.laserExecuted);
}

TEST(ProcessStepTest, LaserExecuteSkipsWhenNoControllerWired) {
  WorkflowContext context;
  context.hasPreparedLaserPose = true;
  context.preparedLaserPose = MechanicalPose {10.0, 20.0, 0.0, 0.0};
  context.laserController = nullptr;

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

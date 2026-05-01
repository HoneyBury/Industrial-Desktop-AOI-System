#include <gtest/gtest.h>

#include "motion/VirtualMotionController.h"
#include "process/BoardWorkflow.h"
#include "process/MarkAlignStep.h"
#include "process/PreLaserStep.h"
#include "process/ProcessEngine.h"
#include "program/ProgramManager.h"

#include <memory>

TEST(ProcessEngineTest, RunsBoardWorkflowQueueInIndustrialOrder) {
  ProgramManager programManager;
  ASSERT_TRUE(programManager.createDefaultProgram());
  auto program = programManager.currentProgram();
  ASSERT_TRUE(program.has_value());
  program->laserOffsetCalibration.calibrated = true;
  program->laserOffsetCalibration.cameraToLaserDxMm = 0.5;
  program->laserOffsetCalibration.cameraToLaserDyMm = -0.25;

  VirtualMotionController motionController;
  WorkflowContext context;
  context.boardId = "BOARD-001";
  context.program = &(*program);
  context.motionController = &motionController;
  context.currentImagePath = "tests/data/demo.png";
  context.currentMachinePose = MechanicalPose {100.0, 200.0, 0.0, 0.0};
  context.measuredMarks = {
      {"Mark-A", 105.0, 82.0, 52.0, 46.0, 0.0, 0.92, 0.82, 0.89, 16, "#ff4d4f",
       MarkShape::Diamond, MarkAlgorithm::ColorBrushTemplate, true},
      {"Mark-B", 245.0, 96.0, 48.0, 48.0, 0.0, 0.90, 0.80, 0.87, 14, "#f97316",
       MarkShape::Cross, MarkAlgorithm::ColorBrushTemplate, true},
  };

  ProcessEngine engine;
  ASSERT_EQ(engine.stepCount(), static_cast<std::size_t>(5));

  const auto result = engine.runBoard(context);
  ASSERT_TRUE(result.ok);
  ASSERT_EQ(result.records.size(), static_cast<std::size_t>(5));
  EXPECT_EQ(result.records.front().stepId, std::string("mark_align"));
  EXPECT_EQ(result.records.back().stepId, std::string("post_laser_verify"));
  EXPECT_TRUE(context.hasMarkAlignment);
  EXPECT_TRUE(context.hasPreparedLaserPose);
  EXPECT_TRUE(context.laserExecuted);
  EXPECT_TRUE(context.finalDecisionOk);
  EXPECT_NEAR(context.currentMachinePose.x, 100.45, 1e-9);
  EXPECT_NEAR(context.currentMachinePose.y, 199.73, 1e-9);
  EXPECT_NEAR(context.currentMachinePose.r, -4.8920860651, 1e-3);
  ASSERT_TRUE(motionController.position(MotionAxis::X).has_value());
  EXPECT_NEAR(*motionController.position(MotionAxis::X), 100.45, 1e-9);
}

TEST(ProcessEngineTest, StopsWorkflowWhenQueueOrderViolatesPreconditions) {
  ProgramManager programManager;
  ASSERT_TRUE(programManager.createDefaultProgram());
  auto program = programManager.currentProgram();
  ASSERT_TRUE(program.has_value());

  VirtualMotionController motionController;
  WorkflowContext context;
  context.program = &(*program);
  context.motionController = &motionController;
  context.currentMachinePose = MechanicalPose {10.0, 20.0, 0.0, 0.0};

  BoardWorkflow workflow;
  workflow.addStep(std::make_unique<PreLaserStep>("pre_laser_before_mark"));
  workflow.addStep(std::make_unique<MarkAlignStep>("mark_align_after"));

  ProcessEngine engine;
  engine.useWorkflow(std::move(workflow));

  const auto result = engine.runBoard(context);
  ASSERT_TRUE(!result.ok);
  ASSERT_EQ(result.records.size(), static_cast<std::size_t>(1));
  EXPECT_EQ(result.records.front().stepId, std::string("pre_laser_before_mark"));
  EXPECT_EQ(result.records.front().status, StepExecutionStatus::Failed);
}

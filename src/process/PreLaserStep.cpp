#include "process/PreLaserStep.h"

#include "motion/MotionAxis.h"

#include <sstream>

PreLaserStep::PreLaserStep(std::string stepId) : stepId_(std::move(stepId)) {}

std::string PreLaserStep::id() const { return stepId_; }

ProcessStepType PreLaserStep::type() const { return ProcessStepType::PreLaser; }

StepExecutionResult PreLaserStep::execute(WorkflowContext &context) const {
  if (context.program == nullptr) {
    return StepExecutionResult {StepExecutionStatus::Failed, "Program context is missing."};
  }
  if (context.motionController == nullptr) {
    return StepExecutionResult {StepExecutionStatus::Failed, "Motion controller is missing."};
  }
  if (!context.hasMarkAlignment) {
    return StepExecutionResult {StepExecutionStatus::Failed, "Mark alignment result is missing."};
  }

  MechanicalPose targetPose = context.currentMachinePose;
  targetPose.x -= context.lastMarkAlignment.millimeterOffset.x;
  targetPose.y -= context.lastMarkAlignment.millimeterOffset.y;
  targetPose.r -= context.lastMarkAlignment.rotationDegrees;

  if (context.program->laserOffsetCalibration.calibrated) {
    targetPose.x += context.program->laserOffsetCalibration.cameraToLaserDxMm;
    targetPose.y += context.program->laserOffsetCalibration.cameraToLaserDyMm;
  }

  if (!context.motionController->moveAbsolute(MotionAxis::X, targetPose.x) ||
      !context.motionController->moveAbsolute(MotionAxis::Y, targetPose.y) ||
      !context.motionController->moveAbsolute(MotionAxis::R, targetPose.r)) {
    return StepExecutionResult {StepExecutionStatus::Failed, "Failed to move to pre-laser compensated pose."};
  }

  context.preparedLaserPose = targetPose;
  context.hasPreparedLaserPose = true;
  context.currentMachinePose = targetPose;

  std::ostringstream stream;
  stream << "Pre-laser pose prepared: X=" << targetPose.x << ", Y=" << targetPose.y << ", R=" << targetPose.r;
  return StepExecutionResult {StepExecutionStatus::Succeeded, stream.str()};
}

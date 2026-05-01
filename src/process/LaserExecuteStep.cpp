#include "process/LaserExecuteStep.h"

LaserExecuteStep::LaserExecuteStep(std::string stepId) : stepId_(std::move(stepId)) {}

std::string LaserExecuteStep::id() const { return stepId_; }

ProcessStepType LaserExecuteStep::type() const { return ProcessStepType::LaserExecute; }

StepExecutionResult LaserExecuteStep::execute(WorkflowContext &context) const {
  if (!context.hasPreparedLaserPose) {
    return StepExecutionResult {StepExecutionStatus::Failed, "Pre-laser pose has not been prepared."};
  }

  context.laserExecuted = true;
  return StepExecutionResult {StepExecutionStatus::Succeeded,
                              "Laser execution simulated at prepared compensated pose."};
}

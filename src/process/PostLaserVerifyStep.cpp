#include "process/PostLaserVerifyStep.h"

PostLaserVerifyStep::PostLaserVerifyStep(std::string stepId) : stepId_(std::move(stepId)) {}

std::string PostLaserVerifyStep::id() const { return stepId_; }

ProcessStepType PostLaserVerifyStep::type() const { return ProcessStepType::PostLaserVerify; }

StepExecutionResult PostLaserVerifyStep::execute(WorkflowContext &context) const {
  if (!context.laserExecuted) {
    context.finalDecisionOk = false;
    return StepExecutionResult {StepExecutionStatus::Failed, "Laser has not been executed yet."};
  }

  const std::string resultText = context.finalDecisionOk ? "OK" : "NG";
  return StepExecutionResult {StepExecutionStatus::Succeeded,
                              "Post-laser verification completed with final decision: " + resultText};
}

#include "process/PostLaserVerifyStep.h"

#include "laser/ILaserController.h"

#include <sstream>

PostLaserVerifyStep::PostLaserVerifyStep(std::string stepId) : stepId_(std::move(stepId)) {}

std::string PostLaserVerifyStep::id() const { return stepId_; }

ProcessStepType PostLaserVerifyStep::type() const { return ProcessStepType::PostLaserVerify; }

StepExecutionResult PostLaserVerifyStep::execute(WorkflowContext &context) const {
  if (!context.laserExecuted) {
    context.finalDecisionOk = false;
    return StepExecutionResult {StepExecutionStatus::Failed, "Laser has not been executed yet."};
  }

  // If a laser controller is wired, verify it is still in a safe state.
  if (context.laserController != nullptr && context.laserController->isStopped()) {
    context.finalDecisionOk = false;
    return StepExecutionResult {StepExecutionStatus::Failed,
                                "Laser controller entered emergency-stop state during mark."};
  }

  const std::string resultText = context.finalDecisionOk ? "OK" : "NG";
  std::ostringstream ss;
  ss << "Post-laser verification: final decision=" << resultText;
  if (context.laserController != nullptr) {
    ss << " | " << context.laserController->lastMarkReport();
  }
  return StepExecutionResult {StepExecutionStatus::Succeeded, ss.str()};
}

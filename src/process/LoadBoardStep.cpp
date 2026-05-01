#include "process/LoadBoardStep.h"

#include <sstream>

LoadBoardStep::LoadBoardStep(std::string stepId) : stepId_(std::move(stepId)) {}

std::string LoadBoardStep::id() const { return stepId_; }

ProcessStepType LoadBoardStep::type() const { return ProcessStepType::LoadBoard; }

StepExecutionResult LoadBoardStep::execute(WorkflowContext &context) const {
  if (context.boardId.empty()) {
    context.boardId = "BOARD-" + std::to_string(context.boardIndex + 1);
  }

  // Only reset board-scoped flags; preserve externally injected data
  // (e.g. image path, measured marks) so test and replay scenarios work.
  context.finalDecisionOk = true;
  context.hasMarkAlignment = false;
  context.hasPreparedLaserPose = false;
  context.laserExecuted = false;
  context.aiDetections.clear();
  context.eventLog.clear();

  std::ostringstream stream;
  stream << "Board loaded: " << context.boardId << " (simulated conveyer entry)";
  return StepExecutionResult {StepExecutionStatus::Succeeded, stream.str()};
}

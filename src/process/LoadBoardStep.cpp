#include "process/LoadBoardStep.h"

#include <sstream>

LoadBoardStep::LoadBoardStep(std::string stepId) : stepId_(std::move(stepId)) {}

std::string LoadBoardStep::id() const { return stepId_; }

ProcessStepType LoadBoardStep::type() const { return ProcessStepType::LoadBoard; }

StepExecutionResult LoadBoardStep::execute(WorkflowContext &context) const {
  const bool transportReady = context.transportController == nullptr || context.transportController->isBoardReady();
  if (!transportReady) {
    context.boardReady = false;
    return StepExecutionResult {StepExecutionStatus::Failed, "Board is not ready from transport controller."};
  }

  context.boardReady = true;
  if (context.boardId.empty()) {
    context.boardId = "BOARD-" + std::to_string(context.boardIndex + 1);
  }

  // Reset per-board state so multi-board runs cannot accidentally reuse
  // the previous board's capture, mark, or inspection results.
  context.finalDecisionOk = true;
  context.hasMarkAlignment = false;
  context.hasPreparedLaserPose = false;
  context.laserExecuted = false;
  context.wholeBoardImagePath.clear();
  context.scanTileRows = 0;
  context.scanTileColumns = 0;
  context.boardScanSummary.clear();
  context.capturedBoardTiles.clear();
  context.capturedRoiImages.clear();
  context.aiDetections.clear();
  context.roiInspectionResults.clear();
  context.laserPointResults.clear();
  context.inspectionDetailsJson.clear();
  context.eventLog.clear();
  if (!context.reuseInjectedInputs) {
    context.currentImagePath.clear();
    context.measuredMarks.clear();
  }

  std::ostringstream stream;
  stream << "Board loaded: " << context.boardId << " (board ready signal acknowledged)";
  return StepExecutionResult {StepExecutionStatus::Succeeded, stream.str()};
}

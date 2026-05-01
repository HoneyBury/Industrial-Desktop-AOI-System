#include "process/MarkAlignStep.h"

#include "alignment/MarkAlignmentSolver.h"

#include <sstream>

MarkAlignStep::MarkAlignStep(std::string stepId) : stepId_(std::move(stepId)) {}

std::string MarkAlignStep::id() const { return stepId_; }

ProcessStepType MarkAlignStep::type() const { return ProcessStepType::MarkAlign; }

StepExecutionResult MarkAlignStep::execute(WorkflowContext &context) const {
  if (context.program == nullptr) {
    return StepExecutionResult {StepExecutionStatus::Failed, "Program context is missing."};
  }

  if (context.measuredMarks.empty()) {
    return StepExecutionResult {StepExecutionStatus::Failed, "Measured marks are missing."};
  }

  alignment::MarkAlignmentSolver solver;
  const auto result = solver.solve(alignment::MarkAlignmentInput {
      context.program->marks,
      context.measuredMarks,
      context.program->pixelScaleCalibration,
  });
  if (!result) {
    return StepExecutionResult {StepExecutionStatus::Failed, result.message};
  }

  context.lastMarkAlignment = result.value;
  context.hasMarkAlignment = true;

  std::ostringstream stream;
  stream << "Mark aligned: dX=" << result.value.millimeterOffset.x << " mm, dY="
         << result.value.millimeterOffset.y << " mm, dR=" << result.value.rotationDegrees << " deg.";
  return StepExecutionResult {StepExecutionStatus::Succeeded, stream.str()};
}

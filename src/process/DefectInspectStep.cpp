#include "process/DefectInspectStep.h"

#include "ai/AiInferencer.h"

#include <sstream>

DefectInspectStep::DefectInspectStep(std::string stepId) : stepId_(std::move(stepId)) {}

std::string DefectInspectStep::id() const { return stepId_; }

ProcessStepType DefectInspectStep::type() const { return ProcessStepType::DefectInspect; }

bool DefectInspectStep::isEnabled(const WorkflowContext &context) const {
  return context.program != nullptr && !context.program->aiModelPath.empty() && !context.currentImagePath.empty();
}

StepFailurePolicy DefectInspectStep::failurePolicy() const { return StepFailurePolicy::ContinueWorkflow; }

StepExecutionResult DefectInspectStep::execute(WorkflowContext &context) const {
  if (context.program == nullptr) {
    return StepExecutionResult {StepExecutionStatus::Failed, "Program context is missing."};
  }

  AiInferencer inferencer;
  const auto loadResult = inferencer.loadModel(context.program->aiModelPath);
  if (!loadResult) {
    context.finalDecisionOk = false;
    return StepExecutionResult {StepExecutionStatus::Failed, loadResult.message};
  }

  const auto inferResult = inferencer.infer(context.currentImagePath);
  if (!inferResult) {
    context.finalDecisionOk = false;
    return StepExecutionResult {StepExecutionStatus::Failed, inferResult.message};
  }

  context.aiDetections = inferResult.value;
  const bool aiOk = !context.aiDetections.empty() && context.aiDetections.front().label != "ng";
  context.finalDecisionOk = context.finalDecisionOk && aiOk;

  std::ostringstream stream;
  stream << "Defect inspection finished: " << context.aiDetections.front().label << " ("
         << context.aiDetections.front().confidence << ")";
  return StepExecutionResult {StepExecutionStatus::Succeeded, stream.str()};
}

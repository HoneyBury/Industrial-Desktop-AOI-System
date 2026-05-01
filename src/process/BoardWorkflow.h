#pragma once

#include "process/IProcessStep.h"
#include "process/ProcessTypes.h"
#include "process/WorkflowContext.h"

#include <functional>
#include <memory>
#include <vector>

class BoardWorkflow {
public:
  using StepProgressFn = std::function<void(int stepIndex, const std::string &stepId, StepExecutionStatus status)>;

  void addStep(std::unique_ptr<IProcessStep> step);
  [[nodiscard]] WorkflowRunResult run(WorkflowContext &context) const;
  [[nodiscard]] WorkflowRunResult run(WorkflowContext &context, StepProgressFn onStep) const;
  [[nodiscard]] std::size_t stepCount() const;

private:
  std::vector<std::unique_ptr<IProcessStep>> steps_;
};

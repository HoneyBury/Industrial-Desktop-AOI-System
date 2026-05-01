#pragma once

#include "process/IProcessStep.h"

class MarkAlignStep final : public IProcessStep {
public:
  explicit MarkAlignStep(std::string stepId = "mark_align");

  [[nodiscard]] std::string id() const override;
  [[nodiscard]] ProcessStepType type() const override;
  [[nodiscard]] StepExecutionResult execute(WorkflowContext &context) const override;

private:
  std::string stepId_;
};

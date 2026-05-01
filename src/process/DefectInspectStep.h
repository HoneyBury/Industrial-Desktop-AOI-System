#pragma once

#include "process/IProcessStep.h"

class DefectInspectStep final : public IProcessStep {
public:
  explicit DefectInspectStep(std::string stepId = "defect_inspect");

  [[nodiscard]] std::string id() const override;
  [[nodiscard]] ProcessStepType type() const override;
  [[nodiscard]] bool isEnabled(const WorkflowContext &context) const override;
  [[nodiscard]] StepFailurePolicy failurePolicy() const override;
  [[nodiscard]] StepExecutionResult execute(WorkflowContext &context) const override;

private:
  std::string stepId_;
};

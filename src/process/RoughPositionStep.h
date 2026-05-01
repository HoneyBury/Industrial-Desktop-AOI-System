#pragma once

#include "process/IProcessStep.h"

class RoughPositionStep final : public IProcessStep {
public:
  explicit RoughPositionStep(std::string stepId = "rough_position");

  [[nodiscard]] std::string id() const override;
  [[nodiscard]] ProcessStepType type() const override;
  [[nodiscard]] StepExecutionResult execute(WorkflowContext &context) const override;

private:
  std::string stepId_;
};

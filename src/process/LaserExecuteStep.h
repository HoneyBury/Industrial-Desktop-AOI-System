#pragma once

#include "process/IProcessStep.h"

class LaserExecuteStep final : public IProcessStep {
public:
  explicit LaserExecuteStep(std::string stepId = "laser_execute");

  [[nodiscard]] std::string id() const override;
  [[nodiscard]] ProcessStepType type() const override;
  [[nodiscard]] StepExecutionResult execute(WorkflowContext &context) const override;

private:
  std::string stepId_;
};

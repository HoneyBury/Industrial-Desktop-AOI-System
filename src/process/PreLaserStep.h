#pragma once

#include "process/IProcessStep.h"

class PreLaserStep final : public IProcessStep {
public:
  explicit PreLaserStep(std::string stepId = "pre_laser");

  [[nodiscard]] std::string id() const override;
  [[nodiscard]] ProcessStepType type() const override;
  [[nodiscard]] StepExecutionResult execute(WorkflowContext &context) const override;

private:
  std::string stepId_;
};

#pragma once

#include "process/IProcessStep.h"

class PostLaserVerifyStep final : public IProcessStep {
public:
  explicit PostLaserVerifyStep(std::string stepId = "post_laser_verify");

  [[nodiscard]] std::string id() const override;
  [[nodiscard]] ProcessStepType type() const override;
  [[nodiscard]] StepExecutionResult execute(WorkflowContext &context) const override;

private:
  std::string stepId_;
};

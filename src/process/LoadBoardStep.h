#pragma once

#include "process/IProcessStep.h"

class LoadBoardStep final : public IProcessStep {
public:
  explicit LoadBoardStep(std::string stepId = "load_board");

  [[nodiscard]] std::string id() const override;
  [[nodiscard]] ProcessStepType type() const override;
  [[nodiscard]] StepExecutionResult execute(WorkflowContext &context) const override;

private:
  std::string stepId_;
};

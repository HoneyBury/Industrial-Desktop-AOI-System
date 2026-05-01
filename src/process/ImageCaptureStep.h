#pragma once

#include "process/IProcessStep.h"

class ImageCaptureStep final : public IProcessStep {
public:
  explicit ImageCaptureStep(std::string stepId = "image_capture");

  [[nodiscard]] std::string id() const override;
  [[nodiscard]] ProcessStepType type() const override;
  [[nodiscard]] StepFailurePolicy failurePolicy() const override;
  [[nodiscard]] StepExecutionResult execute(WorkflowContext &context) const override;

private:
  std::string stepId_;
};

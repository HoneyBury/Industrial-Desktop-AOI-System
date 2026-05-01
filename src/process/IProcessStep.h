#pragma once

#include "common/Result.h"
#include "process/ProcessTypes.h"
#include "process/WorkflowContext.h"

#include <string>

struct StepExecutionResult {
  StepExecutionStatus status {StepExecutionStatus::Pending};
  std::string message;
};

class IProcessStep {
public:
  virtual ~IProcessStep() = default;

  [[nodiscard]] virtual std::string id() const = 0;
  [[nodiscard]] virtual ProcessStepType type() const = 0;
  [[nodiscard]] virtual bool isEnabled(const WorkflowContext &context) const;
  [[nodiscard]] virtual StepFailurePolicy failurePolicy() const;
  [[nodiscard]] virtual StepExecutionResult execute(WorkflowContext &context) const = 0;
};

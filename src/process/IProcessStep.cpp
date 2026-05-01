#include "process/IProcessStep.h"

bool IProcessStep::isEnabled(const WorkflowContext &) const { return true; }

StepFailurePolicy IProcessStep::failurePolicy() const { return StepFailurePolicy::StopWorkflow; }

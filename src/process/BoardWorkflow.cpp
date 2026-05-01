#include "process/BoardWorkflow.h"

void BoardWorkflow::addStep(std::unique_ptr<IProcessStep> step) { steps_.push_back(std::move(step)); }

WorkflowRunResult BoardWorkflow::run(WorkflowContext &context) const {
  return run(context, nullptr);
}

WorkflowRunResult BoardWorkflow::run(WorkflowContext &context, StepProgressFn onStep) const {
  WorkflowRunResult runResult;
  runResult.ok = true;

  const int totalSteps = static_cast<int>(steps_.size());

  for (int idx = 0; idx < totalSteps; ++idx) {
    const auto &step = steps_[static_cast<std::size_t>(idx)];

    StepExecutionRecord record;
    record.stepId = step->id();
    record.stepType = step->type();

    if (!step->isEnabled(context)) {
      record.status = StepExecutionStatus::Skipped;
      record.message = "Step disabled by current workflow context.";
      runResult.records.push_back(record);
      context.eventLog.push_back(record.stepId + ": skipped");

      if (onStep) {
        onStep(idx + 1, record.stepId, StepExecutionStatus::Skipped);
      }

      if (context.onLog) {
        context.onLog(record.stepId + ": 已跳过");
      }
      continue;
    }

    if (onStep) {
      onStep(idx + 1, record.stepId, StepExecutionStatus::Running);
    }

    if (context.onLog) {
      context.onLog(record.stepId + ": 开始执行");
    }

    const StepExecutionResult stepResult = step->execute(context);
    record.status = stepResult.status;
    record.message = stepResult.message;
    runResult.records.push_back(record);
    context.eventLog.push_back(record.stepId + ": " + record.message);

    if (onStep) {
      onStep(idx + 1, record.stepId, stepResult.status);
    }

    if (record.status == StepExecutionStatus::Failed) {
      runResult.ok = false;
      runResult.message = record.message;
      if (step->failurePolicy() == StepFailurePolicy::StopWorkflow) {
        return runResult;
      }
    }
  }

  if (runResult.message.empty()) {
    runResult.message = runResult.ok ? "Workflow completed." : "Workflow completed with failures.";
  }

  return runResult;
}

std::size_t BoardWorkflow::stepCount() const { return steps_.size(); }

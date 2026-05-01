#pragma once

#include "process/BoardWorkflow.h"
#include "process/WorkflowContext.h"

#include <atomic>

class ProcessEngine {
public:
  ProcessEngine();

  void useWorkflow(BoardWorkflow workflow);

  /// Run a single board through the workflow.
  [[nodiscard]] WorkflowRunResult runBoard(WorkflowContext &context) const;

  /// Run all boards sequentially. Returns when all boards are processed or
  /// cancelled. The context's callbacks are invoked for progress and results.
  void runAllBoards(WorkflowContext &context);

  /// Request cancellation of a running runAllBoards loop.
  void requestCancel();

  [[nodiscard]] bool isCancelled() const;

  [[nodiscard]] std::size_t stepCount() const;

private:
  BoardWorkflow workflow_;
  std::atomic<bool> cancelRequested_ {false};
};

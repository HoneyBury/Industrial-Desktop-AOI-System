#include "process/ProcessEngine.h"

#include "process/DefectInspectStep.h"
#include "process/ImageCaptureStep.h"
#include "process/LaserExecuteStep.h"
#include "process/LoadBoardStep.h"
#include "process/MarkAlignStep.h"
#include "process/PostLaserVerifyStep.h"
#include "process/PreLaserStep.h"
#include "process/RoughPositionStep.h"

#include <memory>
#include <sstream>

ProcessEngine::ProcessEngine() {
  workflow_.addStep(std::make_unique<LoadBoardStep>());
  workflow_.addStep(std::make_unique<RoughPositionStep>());
  workflow_.addStep(std::make_unique<ImageCaptureStep>());
  workflow_.addStep(std::make_unique<MarkAlignStep>());
  workflow_.addStep(std::make_unique<DefectInspectStep>());
  workflow_.addStep(std::make_unique<PreLaserStep>());
  workflow_.addStep(std::make_unique<LaserExecuteStep>());
  workflow_.addStep(std::make_unique<PostLaserVerifyStep>());
}

void ProcessEngine::useWorkflow(BoardWorkflow workflow) { workflow_ = std::move(workflow); }

WorkflowRunResult ProcessEngine::runBoard(WorkflowContext &context) const {
  const auto totalSteps = static_cast<int>(workflow_.stepCount());

  return workflow_.run(context, [&](int stepIdx, const std::string &stepId, StepExecutionStatus status) {
    if (context.onStepProgress) {
      context.onStepProgress(stepIdx, totalSteps, stepId, status);
    }
  });
}

void ProcessEngine::runAllBoards(WorkflowContext &context) {
  cancelRequested_ = false;

  if (context.totalBoards <= 0) {
    context.totalBoards = 1;
  }

  for (int boardIdx = context.boardIndex; boardIdx < context.totalBoards; ++boardIdx) {
    if (cancelRequested_) {
      if (context.onLog) {
        context.onLog("运行已被用户取消。");
      }
      break;
    }

    context.boardIndex = boardIdx;

    {
      std::ostringstream ss;
      ss << "开始处理板 #" << (boardIdx + 1) << " / " << context.totalBoards;
      if (context.onLog) {
        context.onLog(ss.str());
      }
    }

    const auto result = runBoard(context);

    if (result.ok) {
      ++context.okCount;
    } else {
      ++context.ngCount;
    }

    if (context.onBoardResult) {
      context.onBoardResult(boardIdx + 1, result.ok, result.message);
    }
  }
}

void ProcessEngine::requestCancel() { cancelRequested_ = true; }

bool ProcessEngine::isCancelled() const { return cancelRequested_; }

std::size_t ProcessEngine::stepCount() const { return workflow_.stepCount(); }

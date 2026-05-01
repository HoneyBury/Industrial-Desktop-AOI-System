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
#include <string_view>

namespace {

std::string escapeJson(const std::string &input) {
  std::string output;
  output.reserve(input.size() * 2);
  for (const char ch : input) {
    switch (ch) {
    case '\\':
      output += "\\\\";
      break;
    case '"':
      output += "\\\"";
      break;
    case '\n':
      output += "\\n";
      break;
    default:
      output += ch;
      break;
    }
  }
  return output;
}

std::string buildInspectionDetailsJson(const WorkflowContext &context) {
  std::ostringstream output;
  output << "{";
  output << "\"finalDecisionOk\":" << (context.finalDecisionOk ? "true" : "false") << ",";
  output << "\"events\":[";
  for (std::size_t index = 0; index < context.eventLog.size(); ++index) {
    output << "\"" << escapeJson(context.eventLog[index]) << "\"";
    if (index + 1 < context.eventLog.size()) {
      output << ",";
    }
  }
  output << "],";
  output << "\"roiInspectionResults\":[";
  for (std::size_t index = 0; index < context.roiInspectionResults.size(); ++index) {
    const auto &result = context.roiInspectionResults[index];
    output << "{"
           << "\"roiName\":\"" << escapeJson(result.roiName) << "\","
           << "\"detectorType\":\"" << escapeJson(std::string(toString(result.detectorType))) << "\","
           << "\"passed\":" << (result.passed ? "true" : "false") << ","
           << "\"confidence\":" << result.confidence << ","
           << "\"candidateCount\":" << result.candidateCount << ","
           << "\"decodedText\":\"" << escapeJson(result.decodedText) << "\","
           << "\"summary\":\"" << escapeJson(result.summary) << "\""
           << "}";
    if (index + 1 < context.roiInspectionResults.size()) {
      output << ",";
    }
  }
  output << "]";
  output << ",";
  output << "\"wholeBoardImagePath\":\"" << escapeJson(context.wholeBoardImagePath) << "\",";
  output << "\"scanTileRows\":" << context.scanTileRows << ",";
  output << "\"scanTileColumns\":" << context.scanTileColumns << ",";
  output << "\"boardScanSummary\":\"" << escapeJson(context.boardScanSummary) << "\",";
  output << "\"laserPointResults\":[";
  for (std::size_t index = 0; index < context.laserPointResults.size(); ++index) {
    const auto &result = context.laserPointResults[index];
    output << "{"
           << "\"taskName\":\"" << escapeJson(result.taskName) << "\","
           << "\"linkedRoiName\":\"" << escapeJson(result.linkedRoiName) << "\","
           << "\"productX\":" << result.productPoint.x << ","
           << "\"productY\":" << result.productPoint.y << ","
           << "\"machineX\":" << result.machinePose.x << ","
           << "\"machineY\":" << result.machinePose.y << ","
           << "\"machineZ\":" << result.machinePose.z << ","
           << "\"machineR\":" << result.machinePose.r << ","
           << "\"laserExecuted\":" << (result.laserExecuted ? "true" : "false") << ","
           << "\"codeVerified\":" << (result.codeVerified ? "true" : "false") << ","
           << "\"passed\":" << (result.passed ? "true" : "false") << ","
           << "\"expectedCodeText\":\"" << escapeJson(result.expectedCodeText) << "\","
           << "\"decodedText\":\"" << escapeJson(result.decodedText) << "\","
           << "\"summary\":\"" << escapeJson(result.summary) << "\""
           << "}";
    if (index + 1 < context.laserPointResults.size()) {
      output << ",";
    }
  }
  output << "]";
  output << "}";
  return output.str();
}

WorkflowRunResult finalizeBoardRun(WorkflowContext &context) {
  WorkflowRunResult result;
  result.records = context.currentBoardRecords;
  result.ok = context.finalDecisionOk;
  for (const auto &record : context.currentBoardRecords) {
    if (record.status == StepExecutionStatus::Failed) {
      result.ok = false;
      if (result.message.empty()) {
        result.message = record.message;
      }
    }
  }

  if (result.message.empty()) {
    result.message = result.ok ? "Workflow completed." : "Workflow completed with failures.";
  }

  context.inspectionDetailsJson = buildInspectionDetailsJson(context);

  if (result.ok) {
    ++context.okCount;
  } else {
    ++context.ngCount;
  }

  if (context.databaseManager != nullptr && context.databaseManager->isOpen()) {
    InspectionRecord record;
    record.boardId = context.boardId;
    record.programName = context.program != nullptr ? context.program->name : "";
    record.imagePath = context.currentImagePath;
    record.aiLabel = result.ok ? "ok" : "ng";
    record.aiConfidence = !context.aiDetections.empty() ? context.aiDetections.front().confidence
                                                        : (result.ok ? 1.0 : 0.0);
    if (!context.aiDetections.empty()) {
      record.aiLabel = context.aiDetections.front().label;
    }
    record.finalDecision = result.ok ? "OK" : "NG";
    record.marksCount = static_cast<int>(context.measuredMarks.size());
    record.roisCount = static_cast<int>(context.roiInspectionResults.size());
    record.detailsJson = context.inspectionDetailsJson;
    context.databaseManager->insertInspectionResult(record);

    BoardRecord boardRecord;
    boardRecord.boardId = context.boardId;
    boardRecord.programName = context.program != nullptr ? context.program->name : "";
    boardRecord.status = result.ok ? "ok" : "ng";
    context.databaseManager->upsertBoardRecord(boardRecord);
  }

  if (context.onBoardResult) {
    context.onBoardResult(context.boardIndex + 1, result.ok, result.message);
  }

  context.nextStepIndex = 0;
  context.currentBoardRecords.clear();
  context.boardId.clear();
  context.wholeBoardImagePath.clear();
  context.scanTileRows = 0;
  context.scanTileColumns = 0;
  context.boardScanSummary.clear();
  context.capturedBoardTiles.clear();
  context.laserPointResults.clear();
  if (!context.reuseInjectedInputs) {
    context.currentImagePath.clear();
    context.measuredMarks.clear();
  }

  return result;
}

} // namespace

ProcessEngine::ProcessEngine() {
  workflow_.addStep(std::make_unique<LoadBoardStep>());
  workflow_.addStep(std::make_unique<RoughPositionStep>());
  workflow_.addStep(std::make_unique<ImageCaptureStep>());
  workflow_.addStep(std::make_unique<MarkAlignStep>());
  workflow_.addStep(std::make_unique<PreLaserStep>());
  workflow_.addStep(std::make_unique<LaserExecuteStep>());
  workflow_.addStep(std::make_unique<PostLaserVerifyStep>());
  workflow_.addStep(std::make_unique<DefectInspectStep>());
}

void ProcessEngine::useWorkflow(BoardWorkflow workflow) { workflow_ = std::move(workflow); }

WorkflowRunResult ProcessEngine::runBoard(WorkflowContext &context) const {
  context.nextStepIndex = 0;
  context.currentBoardRecords.clear();

  while (true) {
    const auto stepRun = runNextStep(context);
    if (!stepRun.advanced) {
      return WorkflowRunResult {false, {}, "Workflow could not advance."};
    }
    if (stepRun.boardCompleted) {
      return stepRun.boardResult;
    }
  }
}

WorkflowStepRunResult ProcessEngine::runNextStep(WorkflowContext &context) const {
  WorkflowStepRunResult stepRun;
  const auto currentStepIndex = static_cast<std::size_t>(context.nextStepIndex);
  const auto totalSteps = static_cast<int>(workflow_.stepCount());
  const IProcessStep *step = workflow_.stepAt(currentStepIndex);
  if (step == nullptr) {
    return stepRun;
  }

  StepExecutionRecord record;
  record.stepId = step->id();
  record.stepType = step->type();

  if (!step->isEnabled(context)) {
    record.status = StepExecutionStatus::Skipped;
    record.message = "Step disabled by current workflow context.";
    if (context.onStepProgress) {
      context.onStepProgress(static_cast<int>(currentStepIndex) + 1, totalSteps, record.stepId, record.status);
    }
    if (context.onLog) {
      context.onLog(record.stepId + ": 已跳过");
    }
    context.eventLog.push_back(record.stepId + ": skipped");
  } else {
    if (context.onStepProgress) {
      context.onStepProgress(static_cast<int>(currentStepIndex) + 1, totalSteps, record.stepId,
                             StepExecutionStatus::Running);
    }
    if (context.onLog) {
      context.onLog(record.stepId + ": 开始执行");
    }

    const StepExecutionResult stepResult = step->execute(context);
    record.status = stepResult.status;
    record.message = stepResult.message;
    context.eventLog.push_back(record.stepId + ": " + record.message);

    if (context.onStepProgress) {
      context.onStepProgress(static_cast<int>(currentStepIndex) + 1, totalSteps, record.stepId, record.status);
    }

    if (record.stepType == ProcessStepType::LoadBoard && record.status == StepExecutionStatus::Succeeded &&
        context.databaseManager != nullptr && context.databaseManager->isOpen()) {
      BoardRecord boardRecord;
      boardRecord.boardId = context.boardId;
      boardRecord.programName = context.program != nullptr ? context.program->name : "";
      boardRecord.status = "processing";
      context.databaseManager->upsertBoardRecord(boardRecord);
    }
  }

  context.currentBoardRecords.push_back(record);
  stepRun.advanced = true;
  stepRun.record = record;

  const bool lastStep = currentStepIndex + 1 >= workflow_.stepCount();
  const bool stopOnFailure =
      record.status == StepExecutionStatus::Failed && step->failurePolicy() == StepFailurePolicy::StopWorkflow;

  if (lastStep || stopOnFailure) {
    stepRun.boardCompleted = true;
    stepRun.boardResult = finalizeBoardRun(context);
    stepRun.boardOk = stepRun.boardResult.ok;
    stepRun.nextStepIndex = 0;
    return stepRun;
  }

  context.nextStepIndex += 1;
  stepRun.nextStepIndex = context.nextStepIndex;
  return stepRun;
}

void ProcessEngine::runAllBoards(WorkflowContext &context) {
  cancelRequested_ = false;

  if (context.totalBoards <= 0) {
    context.totalBoards = 1;
  }

  while (context.boardIndex < context.totalBoards) {
    if (cancelRequested_) {
      if (context.onLog) {
        context.onLog("运行已被用户取消。");
      }
      break;
    }

    {
      std::ostringstream ss;
      ss << "开始处理板 #" << (context.boardIndex + 1) << " / " << context.totalBoards;
      if (context.onLog) {
        context.onLog(ss.str());
      }
    }

    const auto boardResult = runBoard(context);
    (void)boardResult;
    context.boardIndex += 1;
  }
}

void ProcessEngine::requestCancel() { cancelRequested_ = true; }

bool ProcessEngine::isCancelled() const { return cancelRequested_; }

std::size_t ProcessEngine::stepCount() const { return workflow_.stepCount(); }

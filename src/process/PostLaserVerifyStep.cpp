#include "process/PostLaserVerifyStep.h"

#include "laser/ILaserController.h"
#include "vision/CodeReader.h"

#include <algorithm>
#include <sstream>

PostLaserVerifyStep::PostLaserVerifyStep(std::string stepId) : stepId_(std::move(stepId)) {}

std::string PostLaserVerifyStep::id() const { return stepId_; }

ProcessStepType PostLaserVerifyStep::type() const { return ProcessStepType::PostLaserVerify; }

StepExecutionResult PostLaserVerifyStep::execute(WorkflowContext &context) const {
  if (!context.laserExecuted) {
    context.finalDecisionOk = false;
    return StepExecutionResult {StepExecutionStatus::Failed, "Laser has not been executed yet."};
  }

  // If a laser controller is wired, verify it is still in a safe state.
  if (context.laserController != nullptr && context.laserController->isStopped()) {
    context.finalDecisionOk = false;
    return StepExecutionResult {StepExecutionStatus::Failed,
                                "Laser controller entered emergency-stop state during mark."};
  }

  CodeReader codeReader;
  int passedCount = 0;
  int failedCount = 0;
  for (auto &pointResult : context.laserPointResults) {
    if (!pointResult.laserExecuted) {
      pointResult.codeVerified = false;
      pointResult.passed = false;
      pointResult.summary = "Skipped code verification because laser execution failed.";
      ++failedCount;
      continue;
    }

    const auto codeResult = codeReader.readQrCode(context.currentImagePath);
    pointResult.codeVerified = static_cast<bool>(codeResult);
    pointResult.decodedText = codeResult ? codeResult.value : "";
    const bool codeMatches = codeResult && pointResult.expectedCodeText == codeResult.value;
    pointResult.passed = codeMatches;
    pointResult.summary =
        codeResult ? (codeMatches ? "Code verification matched expected text." : "Decoded text did not match expected text.")
                   : codeResult.message;
    if (codeMatches) {
      ++passedCount;
    } else {
      ++failedCount;
    }
  }

  context.finalDecisionOk = context.finalDecisionOk && failedCount == 0 &&
                            !context.laserPointResults.empty();

  const std::string resultText = context.finalDecisionOk ? "OK" : "NG";
  std::ostringstream ss;
  ss << "Post-laser verification: " << passedCount << "/" << context.laserPointResults.size()
     << " point(s) matched expected code, final decision=" << resultText;
  if (context.laserController != nullptr) {
    ss << " | " << context.laserController->lastMarkReport();
  }
  return StepExecutionResult {context.finalDecisionOk ? StepExecutionStatus::Succeeded : StepExecutionStatus::Failed,
                              ss.str()};
}

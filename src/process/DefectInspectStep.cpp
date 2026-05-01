#include "process/DefectInspectStep.h"

#include "ai/AiInferencer.h"
#include "vision/CodeReader.h"
#include "vision/RoiDetector.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace {

const RoiRegion *findRoiByName(const ProgramModel &program, const std::string &roiName) {
  for (const auto &roi : program.rois) {
    if (roi.name == roiName) {
      return &roi;
    }
  }

  return nullptr;
}

RoiInspectionResult buildFallbackAiResult(const ProgramModel &program, const std::string &imagePath) {
  RoiInspectionResult result;
  result.roiName = "full_frame";
  result.detectorType = RoiDetectorType::Ai;

  AiInferencer inferencer;
  const auto loadResult = inferencer.loadModel(program.aiModelPath);
  if (!loadResult) {
    result.passed = false;
    result.summary = loadResult.message;
    return result;
  }

  const auto inferResult = inferencer.infer(imagePath);
  if (!inferResult) {
    result.passed = false;
    result.summary = inferResult.message;
    return result;
  }

  result.detections = inferResult.value;
  result.candidateCount = static_cast<int>(inferResult.value.size());
  if (!inferResult.value.empty()) {
    result.confidence = inferResult.value.front().confidence;
    result.passed = inferResult.value.front().label != "ng";
  }
  result.summary = inferResult.message;
  return result;
}

} // namespace

DefectInspectStep::DefectInspectStep(std::string stepId) : stepId_(std::move(stepId)) {}

std::string DefectInspectStep::id() const { return stepId_; }

ProcessStepType DefectInspectStep::type() const { return ProcessStepType::DefectInspect; }

bool DefectInspectStep::isEnabled(const WorkflowContext &context) const {
  if (context.program == nullptr || context.currentImagePath.empty()) {
    return false;
  }
  std::ifstream test(context.currentImagePath);
  return test.good();
}

StepFailurePolicy DefectInspectStep::failurePolicy() const { return StepFailurePolicy::ContinueWorkflow; }

StepExecutionResult DefectInspectStep::execute(WorkflowContext &context) const {
  if (context.program == nullptr) {
    return StepExecutionResult {StepExecutionStatus::Failed, "Program context is missing."};
  }

  context.aiDetections.clear();
  context.roiInspectionResults.clear();

  const auto &program = *context.program;
  const int enabledLaserPointCount = static_cast<int>(std::count_if(
      program.laserPointTasks.begin(), program.laserPointTasks.end(),
      [](const LaserPointTask &task) { return task.enabled; }));
  if (enabledLaserPointCount == 0) {
    context.finalDecisionOk = false;
    return StepExecutionResult {StepExecutionStatus::Failed, "No enabled laser point tasks are configured."};
  }

  if (program.roiDetectorConfigs.empty()) {
    RoiInspectionResult fallback = buildFallbackAiResult(program, context.currentImagePath);
    context.roiInspectionResults.push_back(fallback);
    context.aiDetections = fallback.detections;
    context.finalDecisionOk = context.finalDecisionOk && fallback.passed;
    return StepExecutionResult {fallback.passed ? StepExecutionStatus::Succeeded : StepExecutionStatus::Failed,
                                fallback.summary};
  }

  RoiDetector roiDetector;
  CodeReader codeReader;
  bool anyFailed = false;
  int passedCount = 0;

  for (const auto &config : program.roiDetectorConfigs) {
    if (!config.enabled) {
      continue;
    }

    RoiInspectionResult inspection;
    inspection.roiName = config.roiName;
    inspection.detectorType = config.detectorType;

    const RoiRegion *roiDefinition = findRoiByName(program, config.roiName);
    if (roiDefinition == nullptr) {
      inspection.passed = false;
      inspection.summary = "ROI definition not found for detector config.";
      anyFailed = true;
      context.roiInspectionResults.push_back(inspection);
      continue;
    }

    // Prefer per-ROI FOV capture image; fall back to full-frame image.
    const auto roiImageIt = context.capturedRoiImages.find(config.roiName);
    const std::string &inspectImagePath =
        (roiImageIt != context.capturedRoiImages.end()) ? roiImageIt->second : context.currentImagePath;

    switch (config.detectorType) {
    case RoiDetectorType::Geometry:
    case RoiDetectorType::Color: {
      const auto detectResult = roiDetector.detectByThreshold(inspectImagePath);
      inspection.passed = static_cast<bool>(detectResult);
      inspection.candidateCount = detectResult ? static_cast<int>(detectResult.value.size()) : 0;
      inspection.confidence = inspection.passed ? 0.75 : 0.0;
      inspection.summary = detectResult ? detectResult.message : detectResult.message;
      break;
    }
    case RoiDetectorType::Template: {
      const auto detectResult = roiDetector.detectByTemplate(
          inspectImagePath, config.templateImagePath);
      inspection.passed = static_cast<bool>(detectResult);
      inspection.candidateCount = detectResult ? static_cast<int>(detectResult.value.size()) : 0;
      inspection.confidence = inspection.passed ? 0.8 : 0.0;
      inspection.summary = detectResult ? detectResult.message : detectResult.message;
      break;
    }
    case RoiDetectorType::Ai: {
      AiInferencer inferencer;
      const std::string modelPath = config.aiModelPath.empty() ? program.aiModelPath : config.aiModelPath;
      const auto loadResult = inferencer.loadModel(modelPath);
      if (!loadResult) {
        inspection.passed = false;
        inspection.summary = loadResult.message;
        break;
      }

      const auto inferResult = inferencer.infer(inspectImagePath);
      if (!inferResult) {
        inspection.passed = false;
        inspection.summary = inferResult.message;
        break;
      }

      inspection.detections = inferResult.value;
      inspection.candidateCount = static_cast<int>(inferResult.value.size());
      if (!inferResult.value.empty()) {
        inspection.confidence = inferResult.value.front().confidence;
        inspection.passed = inferResult.value.front().label != "ng";
      }
      inspection.summary = inferResult.message;
      context.aiDetections.insert(context.aiDetections.end(), inferResult.value.begin(), inferResult.value.end());
      break;
    }
    case RoiDetectorType::Code: {
      const auto codeResult = codeReader.readQrCode(inspectImagePath);
      inspection.passed = static_cast<bool>(codeResult);
      inspection.decodedText = codeResult ? codeResult.value : "";
      inspection.candidateCount = inspection.passed ? 1 : 0;
      inspection.confidence = inspection.passed ? 1.0 : 0.0;
      inspection.summary = codeResult ? codeResult.message : codeResult.message;
      break;
    }
    }

    if (inspection.passed) {
      ++passedCount;
    } else {
      anyFailed = true;
    }

    context.roiInspectionResults.push_back(inspection);
  }

  context.finalDecisionOk = context.finalDecisionOk && !anyFailed;

  bool unloadOk = true;
  if (context.transportController != nullptr) {
    unloadOk = context.transportController->unloadBoard();
    context.boardReady = context.transportController->isBoardReady();
  } else {
    context.boardReady = false;
  }
  if (!unloadOk) {
    context.finalDecisionOk = false;
  }

  std::ostringstream stream;
  stream << "Defect inspection finished: " << passedCount << "/" << context.roiInspectionResults.size()
         << " detector(s) passed, laser points=" << enabledLaserPointCount << ".";
  if (context.transportController != nullptr) {
    stream << " | unload=" << (unloadOk ? "ok" : "failed");
  }

  return StepExecutionResult {anyFailed ? StepExecutionStatus::Failed : StepExecutionStatus::Succeeded,
                              stream.str()};
}

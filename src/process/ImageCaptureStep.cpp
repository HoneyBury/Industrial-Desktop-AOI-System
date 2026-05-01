#include "process/ImageCaptureStep.h"

#include "alignment/MarkDetector.h"

#include <sstream>

ImageCaptureStep::ImageCaptureStep(std::string stepId) : stepId_(std::move(stepId)) {}

std::string ImageCaptureStep::id() const { return stepId_; }

ProcessStepType ImageCaptureStep::type() const { return ProcessStepType::LoadBoard; }

StepFailurePolicy ImageCaptureStep::failurePolicy() const { return StepFailurePolicy::StopWorkflow; }

StepExecutionResult ImageCaptureStep::execute(WorkflowContext &context) const {
  // 1. Capture a frame from the camera via callback
  std::string imagePath;

  if (context.captureFrame) {
    imagePath = context.captureFrame();
  }

  if (imagePath.empty()) {
    imagePath = context.currentImagePath;
  }

  if (imagePath.empty()) {
    return StepExecutionResult {StepExecutionStatus::Failed,
                                "Image capture failed: no frame provider available and no image path set."};
  }

  context.currentImagePath = imagePath;

  // 2. Run Mark detection on the captured frame, unless marks were
  //    already injected (e.g. by a test harness or replay).
  if (context.program == nullptr) {
    return StepExecutionResult {StepExecutionStatus::Failed, "Program context is missing for mark detection."};
  }

  if (!context.program->marks.empty() && context.measuredMarks.empty()) {
    alignment::MarkDetector detector;

    const MarkAlgorithm algorithm = context.program->marks.empty()
                                        ? MarkAlgorithm::ColorBrushTemplate
                                        : context.program->marks.front().algorithm;

    const auto detectResult = detector.detect(imagePath, algorithm);
    if (detectResult) {
      context.measuredMarks = detectResult.value;
    }
  }

  std::ostringstream stream;
  stream << "Frame captured: " << imagePath
         << " | marks detected: " << context.measuredMarks.size();

  return StepExecutionResult {StepExecutionStatus::Succeeded, stream.str()};
}

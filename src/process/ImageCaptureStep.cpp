#include "process/ImageCaptureStep.h"

#include "alignment/MarkDetector.h"
#include "coordinate/CoordinateTransformer.h"

#include <sstream>

ImageCaptureStep::ImageCaptureStep(std::string stepId) : stepId_(std::move(stepId)) {}

std::string ImageCaptureStep::id() const { return stepId_; }

ProcessStepType ImageCaptureStep::type() const { return ProcessStepType::ImageCapture; }

StepFailurePolicy ImageCaptureStep::failurePolicy() const { return StepFailurePolicy::StopWorkflow; }

StepExecutionResult ImageCaptureStep::execute(WorkflowContext &context) const {
  std::string imagePath;
  std::string captureSummary;

  // ── Whole-board scan (if enabled) ──
  if (context.program != nullptr && context.program->scanRecipe.enabled && context.captureWholeBoardScan) {
    const auto scanResult = context.captureWholeBoardScan();
    if (!scanResult) {
      return StepExecutionResult {StepExecutionStatus::Failed,
                                  "Whole-board scan failed: " + scanResult.message};
    }

    imagePath = scanResult.value.mosaicImagePath;
    context.wholeBoardImagePath = scanResult.value.mosaicImagePath;
    context.scanTileRows = scanResult.value.tileRows;
    context.scanTileColumns = scanResult.value.tileColumns;
    context.boardScanSummary = scanResult.value.summary;
    context.capturedBoardTiles = scanResult.value.capturedTiles;
    captureSummary = scanResult.value.summary;
  }

  // ── Per-ROI FOV capture ──
  // When a motion controller is wired and the program defines ROIs, move to
  // each ROI's mechanical position and capture a focused frame so that
  // downstream inspection steps can use the per-ROI images.
  if (context.program != nullptr && context.motionController != nullptr &&
      context.captureFrame && !context.program->rois.empty()) {
    CoordinateTransformer transformer;
    int roiCaptureCount = 0;

    for (const auto &roi : context.program->rois) {
      if (!roi.enabled) {
        continue;
      }

      const MillimeterPoint roiProductCenter {roi.x + roi.width / 2.0,
                                              roi.y + roi.height / 2.0};
      const MechanicalPose roiPose = transformer.productToMechanical(
          roiProductCenter, context.currentMachinePose);

      context.motionController->moveAbsolute(MotionAxis::X, roiPose.x);
      context.motionController->moveAbsolute(MotionAxis::Y, roiPose.y);

      const std::string roiImagePath = context.captureFrame();
      if (!roiImagePath.empty()) {
        context.capturedRoiImages[roi.name] = roiImagePath;
        ++roiCaptureCount;
      }
    }

    if (roiCaptureCount > 0) {
      if (!captureSummary.empty()) {
        captureSummary += " | ";
      }
      captureSummary +=
          "Per-ROI captures: " + std::to_string(roiCaptureCount) + " ROI(s)";
    }
  }

  // ── Fallback: single frame capture ──
  if (context.captureFrame) {
    if (imagePath.empty()) {
      imagePath = context.captureFrame();
    }
  }

  if (imagePath.empty()) {
    imagePath = context.currentImagePath;
  }

  if (imagePath.empty()) {
    return StepExecutionResult {StepExecutionStatus::Failed,
                                "Image capture failed: no frame provider available and no image path set."};
  }

  context.currentImagePath = imagePath;

  // ── Mark detection on the captured frame ──
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
  if (!captureSummary.empty()) {
    stream << captureSummary << " | ";
  }
  stream << "Frame captured: " << imagePath
         << " | marks detected: " << context.measuredMarks.size();

  return StepExecutionResult {StepExecutionStatus::Succeeded, stream.str()};
}

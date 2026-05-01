#include "process/PreLaserStep.h"

#include "coordinate/CoordinateTransformer.h"
#include "motion/MotionAxis.h"

#include <algorithm>
#include <sstream>

namespace {

MechanicalPose laserPreparationOriginPose(const WorkflowContext &context) {
  if (context.program != nullptr) {
    if (context.program->runtimeSummary.hasOriginCalibration) {
      return context.program->runtimeSummary.originCorrectedPose;
    }
    if (context.program->originCalibration.calibrated) {
      return context.program->originCalibration.machineReferencePose;
    }
  }

  return context.currentMachinePose;
}

} // namespace

PreLaserStep::PreLaserStep(std::string stepId) : stepId_(std::move(stepId)) {}

std::string PreLaserStep::id() const { return stepId_; }

ProcessStepType PreLaserStep::type() const { return ProcessStepType::PreLaser; }

StepExecutionResult PreLaserStep::execute(WorkflowContext &context) const {
  if (context.program == nullptr) {
    return StepExecutionResult {StepExecutionStatus::Failed, "Program context is missing."};
  }
  if (context.motionController == nullptr) {
    return StepExecutionResult {StepExecutionStatus::Failed, "Motion controller is missing."};
  }
  if (!context.hasMarkAlignment) {
    return StepExecutionResult {StepExecutionStatus::Failed, "Mark alignment result is missing."};
  }

  context.laserPointResults.clear();

  CoordinateTransformer transformer;
  const MechanicalPose originPose = laserPreparationOriginPose(context);
  const double laserOffsetX =
      context.program->laserOffsetCalibration.calibrated ? context.program->laserOffsetCalibration.cameraToLaserDxMm : 0.0;
  const double laserOffsetY =
      context.program->laserOffsetCalibration.calibrated ? context.program->laserOffsetCalibration.cameraToLaserDyMm : 0.0;

  for (const auto &task : context.program->laserPointTasks) {
    if (!task.enabled) {
      continue;
    }

    MechanicalPose targetPose =
        transformer.productToMechanical(MillimeterPoint {task.x, task.y}, originPose);
    targetPose.x -= context.lastMarkAlignment.millimeterOffset.x;
    targetPose.y -= context.lastMarkAlignment.millimeterOffset.y;
    targetPose.r -= context.lastMarkAlignment.rotationDegrees;
    targetPose.x += laserOffsetX;
    targetPose.y += laserOffsetY;

    context.laserPointResults.push_back(LaserPointExecutionResult {
        task.name,
        task.linkedRoiName,
        MillimeterPoint {task.x, task.y},
        targetPose,
        false,
        false,
        false,
        task.expectedCodeText,
        {},
        {},
    });
  }

  if (context.laserPointResults.empty()) {
    return StepExecutionResult {StepExecutionStatus::Failed, "No enabled laser point tasks are available for execution."};
  }

  const auto &firstPoint = context.laserPointResults.front();
  context.preparedLaserPose = firstPoint.machinePose;
  context.hasPreparedLaserPose = true;
  context.currentMachinePose = firstPoint.machinePose;

  std::ostringstream stream;
  stream << "Prepared " << context.laserPointResults.size()
         << " compensated laser point(s); first point X=" << firstPoint.machinePose.x
         << ", Y=" << firstPoint.machinePose.y << ", R=" << firstPoint.machinePose.r;
  return StepExecutionResult {StepExecutionStatus::Succeeded, stream.str()};
}

#include "process/LaserExecuteStep.h"

#include "laser/ILaserController.h"
#include "motion/MotionAxis.h"

#include <algorithm>
#include <sstream>

LaserExecuteStep::LaserExecuteStep(std::string stepId) : stepId_(std::move(stepId)) {}

std::string LaserExecuteStep::id() const { return stepId_; }

ProcessStepType LaserExecuteStep::type() const { return ProcessStepType::LaserExecute; }

StepExecutionResult LaserExecuteStep::execute(WorkflowContext &context) const {
  if (!context.hasPreparedLaserPose) {
    return StepExecutionResult {StepExecutionStatus::Failed, "Pre-laser pose has not been prepared."};
  }
  if (context.motionController == nullptr) {
    return StepExecutionResult {StepExecutionStatus::Failed, "Motion controller is missing for laser execution."};
  }

  ILaserController *laser = context.laserController;
  if (laser != nullptr && !laser->isReady()) {
    return StepExecutionResult {StepExecutionStatus::Failed, "Laser controller is not ready."};
  }

  int executedCount = 0;
  int failedCount = 0;
  std::string lastReport;

  if (context.laserPointResults.empty()) {
    context.laserPointResults.push_back(LaserPointExecutionResult {
        "single_laser_pose",
        {},
        {},
        context.preparedLaserPose,
        false,
        false,
        false,
        "DEMO-CODE-001",
        {},
        {},
    });
  }

  for (auto &pointResult : context.laserPointResults) {
    if (!context.motionController->moveAbsolute(MotionAxis::CameraX, pointResult.machinePose.x) ||
        !context.motionController->moveAbsolute(MotionAxis::CameraY, pointResult.machinePose.y) ||
        !context.motionController->moveAbsolute(MotionAxis::Z, pointResult.machinePose.z) ||
        !context.motionController->moveAbsolute(MotionAxis::R, pointResult.machinePose.r)) {
      pointResult.summary = "Failed to move to compensated laser pose.";
      pointResult.laserExecuted = false;
      pointResult.passed = false;
      ++failedCount;
      continue;
    }

    LaserMarkingParams params;
    params.x = pointResult.machinePose.x;
    params.y = pointResult.machinePose.y;
    params.powerPercent = context.program != nullptr ? context.program->laserPowerPercent : 80.0;
    params.frequencyKhz = context.program != nullptr ? context.program->laserFrequencyKhz : 20.0;
    params.pulseWidthUs = context.program != nullptr ? context.program->laserPulseWidthUs : 10.0;
    params.repeatCount = context.program != nullptr ? context.program->laserRepeatCount : 1;

    const bool ok = laser == nullptr ? true : laser->executeMark(params);
    pointResult.laserExecuted = ok;
    pointResult.passed = ok;
    pointResult.summary = ok ? "Laser executed at compensated point." : "Laser execution failed.";
    lastReport = laser != nullptr ? laser->lastMarkReport() : "Simulated laser execution without controller.";
    if (ok) {
      ++executedCount;
      context.currentMachinePose = pointResult.machinePose;
    } else {
      ++failedCount;
    }
  }

  context.laserExecuted = failedCount == 0 && executedCount > 0;

  std::ostringstream ss;
  ss << "Laser point execution finished: " << executedCount << "/" << context.laserPointResults.size()
     << " point(s) executed.";
  if (!lastReport.empty()) {
    ss << " | " << lastReport;
  }
  return StepExecutionResult {context.laserExecuted ? StepExecutionStatus::Succeeded : StepExecutionStatus::Failed,
                              ss.str()};
}

#include "process/LaserExecuteStep.h"

#include "laser/ILaserController.h"

#include <sstream>

LaserExecuteStep::LaserExecuteStep(std::string stepId) : stepId_(std::move(stepId)) {}

std::string LaserExecuteStep::id() const { return stepId_; }

ProcessStepType LaserExecuteStep::type() const { return ProcessStepType::LaserExecute; }

StepExecutionResult LaserExecuteStep::execute(WorkflowContext &context) const {
  if (!context.hasPreparedLaserPose) {
    return StepExecutionResult {StepExecutionStatus::Failed, "Pre-laser pose has not been prepared."};
  }

  ILaserController *laser = context.laserController;
  if (laser == nullptr) {
    context.laserExecuted = true;
    return StepExecutionResult {StepExecutionStatus::Succeeded,
                                "No laser controller wired; skipping laser fire (simulated pass)."};
  }

  if (!laser->isReady()) {
    return StepExecutionResult {StepExecutionStatus::Failed, "Laser controller is not ready."};
  }

  LaserMarkingParams params;
  params.x = context.preparedLaserPose.x;
  params.y = context.preparedLaserPose.y;
  params.powerPercent = context.program != nullptr ? context.program->laserPowerPercent : 80.0;
  params.frequencyKhz = context.program != nullptr ? context.program->laserFrequencyKhz : 20.0;
  params.pulseWidthUs = context.program != nullptr ? context.program->laserPulseWidthUs : 10.0;
  params.repeatCount = context.program != nullptr ? context.program->laserRepeatCount : 1;

  const bool ok = laser->executeMark(params);
  context.laserExecuted = ok;

  std::ostringstream ss;
  ss << "Laser mark " << (ok ? "executed" : "failed") << ": " << laser->lastMarkReport();
  return StepExecutionResult {ok ? StepExecutionStatus::Succeeded : StepExecutionStatus::Failed, ss.str()};
}

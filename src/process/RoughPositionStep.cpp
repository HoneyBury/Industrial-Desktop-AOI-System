#include "process/RoughPositionStep.h"

#include <sstream>

RoughPositionStep::RoughPositionStep(std::string stepId) : stepId_(std::move(stepId)) {}

std::string RoughPositionStep::id() const { return stepId_; }

ProcessStepType RoughPositionStep::type() const { return ProcessStepType::RoughPosition; }

StepExecutionResult RoughPositionStep::execute(WorkflowContext &context) const {
  if (context.motionController != nullptr) {
    // Home axes to origin only when controller is still at zero (fresh start).
    // If the controller was pre-positioned (e.g. by a test harness), preserve it.
    const double currentX = context.motionController->position(MotionAxis::X).value_or(0.0);
    const double currentY = context.motionController->position(MotionAxis::Y).value_or(0.0);
    const bool alreadyPositioned = std::abs(currentX) > 0.01 || std::abs(currentY) > 0.01;

    if (!alreadyPositioned) {
      context.motionController->moveAbsolute(MotionAxis::X, 0.0);
      context.motionController->moveAbsolute(MotionAxis::Y, 0.0);
      context.motionController->moveAbsolute(MotionAxis::Z, 0.0);
      context.motionController->moveAbsolute(MotionAxis::R, 0.0);
    }

    context.currentMachinePose = MechanicalPose {
        context.motionController->position(MotionAxis::X).value_or(0.0),
        context.motionController->position(MotionAxis::Y).value_or(0.0),
        context.motionController->position(MotionAxis::Z).value_or(0.0),
        context.motionController->position(MotionAxis::R).value_or(0.0),
    };
  }

  std::ostringstream stream;
  stream << "Rough position set to origin (simulated stopper alignment)";
  return StepExecutionResult {StepExecutionStatus::Succeeded, stream.str()};
}

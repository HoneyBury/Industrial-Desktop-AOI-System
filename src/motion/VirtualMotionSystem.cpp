#include "motion/VirtualMotionSystem.h"

#include <cmath>

#ifdef AOI_HAS_QT_WIDGETS
#include <QCoreApplication>
#include <QEventLoop>
#endif

VirtualMotionSystem::VirtualMotionSystem(VirtualMotionController &motionController,
                                         VirtualTransportController &transportController)
    : motionController_(&motionController),
      transportController_(&transportController) {
  transportController_->setMotionController(motionController_);
}

void VirtualMotionSystem::tick(const double deltaSec) {
  if (advancing_) return;

  transportController_->tick(deltaSec);
  motionController_->tick(deltaSec);
}

bool VirtualMotionSystem::isAdvancing() const { return advancing_; }

void VirtualMotionSystem::setBoardDefinition(const BoardDefinition &definition) {
  boardDefinition_ = definition;
}

const BoardDefinition &VirtualMotionSystem::boardDefinition() const { return boardDefinition_; }

MechanicalPose VirtualMotionSystem::currentCameraPose() const {
  return MechanicalPose {
      motionController_->position(MotionAxis::CameraX).value_or(0.0),
      motionController_->position(MotionAxis::CameraY).value_or(0.0),
      motionController_->position(MotionAxis::Z).value_or(0.0),
      motionController_->position(MotionAxis::R).value_or(0.0),
  };
}

bool VirtualMotionSystem::moveCameraPose(const MechanicalPose &pose,
                                         const double speed,
                                         const double timeoutSec) {
  if (!motionController_->moveAbs(MotionAxis::CameraX, pose.x, speed) ||
      !motionController_->moveAbs(MotionAxis::CameraY, pose.y, speed) ||
      !motionController_->moveAbs(MotionAxis::Z, pose.z, speed) ||
      !motionController_->moveAbs(MotionAxis::R, pose.r, speed)) {
    return false;
  }

  return waitForCameraAxes(timeoutSec);
}

bool VirtualMotionSystem::moveCameraXY(const double x,
                                       const double y,
                                       const double speed,
                                       const double timeoutSec) {
  const MechanicalPose currentPose = currentCameraPose();
  return moveCameraPose(MechanicalPose {x, y, currentPose.z, currentPose.r}, speed, timeoutSec);
}

bool VirtualMotionSystem::homeCameraAxes(const double timeoutSec) {
  return motionController_->homeAxis(MotionAxis::CameraX) &&
         motionController_->homeAxis(MotionAxis::CameraY) &&
         motionController_->homeAxis(MotionAxis::Z) &&
         motionController_->homeAxis(MotionAxis::R) &&
         waitForCameraAxes(timeoutSec);
}

bool VirtualMotionSystem::waitForCameraAxes(const double timeoutSec) {
  return waitForAxes({MotionAxis::CameraX, MotionAxis::CameraY, MotionAxis::Z, MotionAxis::R}, timeoutSec);
}

bool VirtualMotionSystem::moveAxis(const MotionAxis axis,
                                   const double position,
                                   const double speed,
                                   const double timeoutSec) {
  return motionController_->moveAbs(axis, position, speed) && waitForAxis(axis, timeoutSec);
}

bool VirtualMotionSystem::jogAxis(const MotionAxis axis,
                                  const double delta,
                                  const double speed,
                                  const double timeoutSec) {
  return motionController_->moveRel(axis, delta, speed) && waitForAxis(axis, timeoutSec);
}

bool VirtualMotionSystem::homeAxis(const MotionAxis axis, const double timeoutSec) {
  return motionController_->homeAxis(axis) && waitForAxis(axis, timeoutSec);
}

bool VirtualMotionSystem::waitForAxis(const MotionAxis axis, const double timeoutSec) {
  return waitForAxes({axis}, timeoutSec);
}

bool VirtualMotionSystem::loadBoard() { return transportController_->loadBoard(); }

bool VirtualMotionSystem::unloadBoard() { return transportController_->unloadBoard(); }

bool VirtualMotionSystem::raiseStopper() { return transportController_->raiseStopper(); }

bool VirtualMotionSystem::lowerStopper() { return transportController_->lowerStopper(); }

void VirtualMotionSystem::resetBoardTransport() { transportController_->resetBoardReadySignal(); }

bool VirtualMotionSystem::waitForBoardReady(const double timeoutSec) {
  return waitForTransportState(BoardTransportState::BoardReady, timeoutSec);
}

bool VirtualMotionSystem::waitForTransportIdle(const double timeoutSec) {
  return waitForTransportState(BoardTransportState::Idle, timeoutSec);
}

VirtualMotionController &VirtualMotionSystem::motionController() { return *motionController_; }

const VirtualMotionController &VirtualMotionSystem::motionController() const { return *motionController_; }

VirtualTransportController &VirtualMotionSystem::transportController() { return *transportController_; }

const VirtualTransportController &VirtualMotionSystem::transportController() const { return *transportController_; }

bool VirtualMotionSystem::waitForAxes(const std::initializer_list<MotionAxis> axes, const double timeoutSec) {
  const int maxTicks = std::max(1, static_cast<int>(std::ceil(timeoutSec / kStepSec)));
  advancing_ = true;

  for (int tickIndex = 0; tickIndex < maxTicks; ++tickIndex) {
    transportController_->tick(kStepSec);
    motionController_->tick(kStepSec);

    bool allSettled = true;
    for (const MotionAxis axis : axes) {
      if (!axisSettled(motionController_->getAxisState(axis))) {
        allSettled = false;
        break;
      }
    }

#ifdef AOI_HAS_QT_WIDGETS
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 1);
#endif

    if (allSettled) {
      advancing_ = false;
      return true;
    }
  }

  advancing_ = false;
  for (const MotionAxis axis : axes) {
    if (!axisSettled(motionController_->getAxisState(axis))) {
      return false;
    }
  }
  return true;
}

bool VirtualMotionSystem::waitForTransportState(const BoardTransportState targetState, const double timeoutSec) {
  const int maxTicks = std::max(1, static_cast<int>(std::ceil(timeoutSec / kStepSec)));
  advancing_ = true;

  for (int tickIndex = 0; tickIndex < maxTicks; ++tickIndex) {
    transportController_->tick(kStepSec);
    motionController_->tick(kStepSec);

#ifdef AOI_HAS_QT_WIDGETS
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 1);
#endif

    if (transportController_->state() == targetState) {
      advancing_ = false;
      return true;
    }
  }

  advancing_ = false;
  return transportController_->state() == targetState;
}

bool VirtualMotionSystem::axisSettled(const AxisState state) {
  return state != AxisState::Moving && state != AxisState::Homing;
}

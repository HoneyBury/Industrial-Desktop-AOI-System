#include "transport/VirtualTransportController.h"

#include "motion/IMotionController.h"
#include "motion/VirtualMotionController.h"

#include <algorithm>
#include <sstream>

namespace {

void syncTransportAxis(IMotionController *motion, const MotionAxis axis, const double position) {
  if (motion == nullptr) return;

  if (auto *virtualMotion = dynamic_cast<VirtualMotionController *>(motion); virtualMotion != nullptr) {
    virtualMotion->setAxisPosition(axis, position);
    return;
  }

  motion->moveAbs(axis, position, 5000.0);
}

} // namespace

VirtualTransportController::VirtualTransportController() = default;

void VirtualTransportController::setMotionController(IMotionController *motion) {
  motion_ = motion;
}

bool VirtualTransportController::loadBoard() {
  if (state_ != BoardTransportState::Idle) {
    lastSignalMessage_ = "Transport is busy; only Idle state can accept a new load request.";
    return false;
  }

  state_ = BoardTransportState::Loading;
  boardPosMm_ = 0.0;
  travelTimer_ = 0.0;
  conveyorSpeed_ = kTravelSpeed;
  lastSignalMessage_ = "Board loading started, conveyor running.";
  updateMotionAxes();
  return true;
}

bool VirtualTransportController::unloadBoard() {
  if (state_ != BoardTransportState::BoardReady) {
    lastSignalMessage_ = "No board ready to unload.";
    return false;
  }

  lowerStopper();
  state_ = BoardTransportState::Unloading;
  conveyorSpeed_ = kTravelSpeed;
  travelTimer_ = 0.0;
  lastSignalMessage_ = "Board unloading, conveyor running.";
  updateMotionAxes();
  return true;
}

bool VirtualTransportController::isBoardReady() const {
  return state_ == BoardTransportState::BoardReady;
}

void VirtualTransportController::resetBoardReadySignal() {
  if (state_ == BoardTransportState::Idle && boardPosMm_ <= 0.0 && !stopperRaised_) return;

  state_ = BoardTransportState::Idle;
  boardPosMm_ = 0.0;
  conveyorSpeed_ = 0.0;
  travelTimer_ = 0.0;
  stopperRaised_ = false;
  lastSignalMessage_ = "Board transport reset, state back to Idle.";
  updateMotionAxes();
}

BoardTransportState VirtualTransportController::state() const { return state_; }

std::string VirtualTransportController::lastSignalMessage() const { return lastSignalMessage_; }

bool VirtualTransportController::raiseStopper() {
  stopperRaised_ = true;
  lastSignalMessage_ = "Stopper raised.";
  updateMotionAxes();
  return true;
}

bool VirtualTransportController::lowerStopper() {
  stopperRaised_ = false;
  lastSignalMessage_ = "Stopper lowered.";
  updateMotionAxes();
  return true;
}

bool VirtualTransportController::isStopperRaised() const { return stopperRaised_; }

double VirtualTransportController::boardPosition() const { return boardPosMm_; }

double VirtualTransportController::conveyorSpeed() const { return conveyorSpeed_; }

double VirtualTransportController::stopperTargetPosition() const { return kStopperTarget; }

void VirtualTransportController::tick(const double deltaSec) {
  const double dt = std::min(deltaSec, 0.05);

  switch (state_) {
  case BoardTransportState::Loading:
    // 板在传送带上向挡板移动
    boardPosMm_ += conveyorSpeed_ * dt;
    if (boardPosMm_ >= kStopperTarget) {
      boardPosMm_ = kStopperTarget;
      conveyorSpeed_ = 0.0;
      stopperRaised_ = true;
      state_ = BoardTransportState::BoardReady;
      lastSignalMessage_ = "Board reached stopper, ready for inspection.";
    }
    break;

  case BoardTransportState::Unloading:
    // 板离开
    boardPosMm_ += conveyorSpeed_ * dt;
    if (boardPosMm_ >= kExitTarget) {
      boardPosMm_ = 0.0;
      conveyorSpeed_ = 0.0;
      stopperRaised_ = false;
      state_ = BoardTransportState::Idle;
      lastSignalMessage_ = "Board unloaded, conveyor stopped.";
    }
    break;

  case BoardTransportState::BoardReady:
  case BoardTransportState::Idle:
    travelTimer_ = 0.0;
    break;
  }

  updateMotionAxes();
}

void VirtualTransportController::updateMotionAxes() {
  if (motion_ == nullptr) return;

  // Conveyor / Stopper 轴是运输仿真的镜像量，不参与真实定位闭环，
  // 这里直接同步，避免被普通运动轴软限位和异步回位拖住。
  syncTransportAxis(motion_, MotionAxis::Conveyor, boardPosMm_);
  syncTransportAxis(motion_, MotionAxis::Stopper, stopperRaised_ ? 1.0 : 0.0);
}

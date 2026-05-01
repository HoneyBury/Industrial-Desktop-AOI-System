#include "transport/VirtualTransportController.h"

#include "motion/IMotionController.h"

#include <algorithm>
#include <sstream>

VirtualTransportController::VirtualTransportController() = default;

void VirtualTransportController::setMotionController(IMotionController *motion) {
  motion_ = motion;
}

bool VirtualTransportController::loadBoard() {
  if (state_ == BoardTransportState::Loading || state_ == BoardTransportState::BoardReady) {
    lastSignalMessage_ = "Board already loading or ready; ignoring load request.";
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
  if (state_ == BoardTransportState::BoardReady) {
    state_ = BoardTransportState::Idle;
    boardPosMm_ = 0.0;
    conveyorSpeed_ = 0.0;
    stopperRaised_ = false;
    lastSignalMessage_ = "Board ready signal reset, state back to Idle.";
    updateMotionAxes();
  }
}

BoardTransportState VirtualTransportController::state() const { return state_; }

std::string VirtualTransportController::lastSignalMessage() const { return lastSignalMessage_; }

bool VirtualTransportController::raiseStopper() {
  stopperRaised_ = true;
  if (motion_ != nullptr) {
    motion_->moveAbs(MotionAxis::Stopper, 1.0); // 1.0 = 升起
  }
  lastSignalMessage_ = "Stopper raised.";
  return true;
}

bool VirtualTransportController::lowerStopper() {
  stopperRaised_ = false;
  if (motion_ != nullptr) {
    motion_->moveAbs(MotionAxis::Stopper, 0.0); // 0.0 = 下降
  }
  lastSignalMessage_ = "Stopper lowered.";
  return true;
}

bool VirtualTransportController::isStopperRaised() const { return stopperRaised_; }

double VirtualTransportController::boardPosition() const { return boardPosMm_; }

double VirtualTransportController::conveyorSpeed() const { return conveyorSpeed_; }

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

  // Conveyor 轴反映板位置（mm），动画通过此轴读取板位置
  // 使用较高速度确保运动轴能跟上运输模拟
  const double speed = (conveyorSpeed_ > 0) ? conveyorSpeed_ : 100.0;
  motion_->moveAbs(MotionAxis::Conveyor, boardPosMm_, speed);

  // Stopper 轴反映挡板状态
  motion_->moveAbs(MotionAxis::Stopper, stopperRaised_ ? 1.0 : 0.0, 200.0);
}

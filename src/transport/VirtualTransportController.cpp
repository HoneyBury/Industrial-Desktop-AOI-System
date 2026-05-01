#include "transport/VirtualTransportController.h"

bool VirtualTransportController::loadBoard() {
  state_ = BoardTransportState::Loading;
  state_ = BoardTransportState::BoardReady;
  lastSignalMessage_ = "Board ready signal sent to upper controller.";
  return true;
}

bool VirtualTransportController::unloadBoard() {
  state_ = BoardTransportState::Unloading;
  state_ = BoardTransportState::Idle;
  lastSignalMessage_ = "Board unloaded and ready signal cleared.";
  return true;
}

bool VirtualTransportController::isBoardReady() const { return state_ == BoardTransportState::BoardReady; }

void VirtualTransportController::resetBoardReadySignal() {
  if (state_ == BoardTransportState::BoardReady) {
    state_ = BoardTransportState::Idle;
    lastSignalMessage_ = "Board ready signal reset.";
  }
}

BoardTransportState VirtualTransportController::state() const { return state_; }

std::string VirtualTransportController::lastSignalMessage() const { return lastSignalMessage_; }

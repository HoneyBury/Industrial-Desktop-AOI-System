#pragma once

#include "transport/ITransportController.h"

class VirtualTransportController final : public ITransportController {
public:
  bool loadBoard() override;
  bool unloadBoard() override;
  bool isBoardReady() const override;
  void resetBoardReadySignal() override;
  BoardTransportState state() const override;
  std::string lastSignalMessage() const override;

private:
  BoardTransportState state_ {BoardTransportState::Idle};
  std::string lastSignalMessage_ {"No board event yet."};
};

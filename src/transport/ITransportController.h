#pragma once

#include <string>

enum class BoardTransportState {
  Idle,
  Loading,
  BoardReady,
  Unloading,
};

class ITransportController {
public:
  virtual ~ITransportController() = default;

  virtual bool loadBoard() = 0;
  virtual bool unloadBoard() = 0;
  virtual bool isBoardReady() const = 0;
  virtual void resetBoardReadySignal() = 0;
  virtual BoardTransportState state() const = 0;
  virtual std::string lastSignalMessage() const = 0;
  virtual void tick(double /*deltaSec*/) {} // 默认空实现，虚拟子类可覆写
};

#pragma once

#include "statemachine/MachineState.h"

#include <functional>
#include <map>
#include <string>

/// 工业设备主状态机。
///
/// 设计要点：
/// - 基于转移表实现，无 Qt/QML 依赖
/// - 每个状态支持 onEnter / onExit 回调
/// - 通过 sendEvent() 触发状态转移
/// - 通过 tick() 定期检查自动转移条件
/// - 状态变更时触发 onChange 回调，供 UI / 动画 / 日志消费
class MachineStateMachine {
public:
  MachineStateMachine();

  /// 发送事件，若存在匹配转移则执行
  bool sendEvent(MachineEvent event);

  /// 定时更新：检查当前状态的自动转移条件
  void tick();

  /// 注册进入/退出动作
  void setEnterAction(MachineState state, StateAction action);
  void setExitAction(MachineState state, StateAction action);

  /// 当前状态
  [[nodiscard]] MachineState currentState() const;

  /// 状态变更回调
  using StateChangeCallback = std::function<void(MachineState from, MachineState to, MachineEvent event)>;
  void setStateChangeCallback(StateChangeCallback callback);

  struct TransitionKey {
    MachineState from;
    MachineEvent event;

    bool operator<(const TransitionKey &other) const {
      if (from != other.from) return from < other.from;
      return event < other.event;
    }
  };

private:

  void enterState(MachineState state);
  void exitState(MachineState state);
  bool applyTransition(MachineState target, MachineEvent event);

  std::map<TransitionKey, MachineState> transitions_;
  std::map<MachineState, StateAction> enterActions_;
  std::map<MachineState, StateAction> exitActions_;
  MachineState currentState_ {MachineState::PowerOff};
  StateChangeCallback onChange_;
};

#include "statemachine/MachineStateMachine.h"

namespace {

// ── 转移表定义 ────────────────────────────────────

std::map<MachineStateMachine::TransitionKey, MachineState> buildTransitionTable() {
  std::map<MachineStateMachine::TransitionKey, MachineState> table;

  using S = MachineState;
  using E = MachineEvent;

  // 启动流程
  table[{S::PowerOff,        E::Start}]           = S::Initializing;
  table[{S::Initializing,    E::HomeFinished}]    = S::Idle;

  // 回零
  table[{S::Idle,            E::Start}]           = S::Homing;
  table[{S::Homing,          E::HomeFinished}]    = S::Idle;

  // 进板流程
  table[{S::Idle,            E::BoardDetected}]   = S::WaitingBoard;
  table[{S::WaitingBoard,    E::Start}]           = S::LoadingBoard;
  table[{S::LoadingBoard,    E::StopperReached}]  = S::BoardArrived;
  table[{S::BoardArrived,    E::Start}]           = S::StopperPositioning;
  table[{S::StopperPositioning, E::Start}]        = S::ReadyForInspection;

  // 检测流程
  table[{S::ReadyForInspection, E::Start}]        = S::MovingCameraToMark;
  table[{S::MovingCameraToMark, E::CameraMoveFinished}] = S::CapturingImage;
  table[{S::CapturingImage,   E::ImageCaptured}]  = S::DetectingMark;
  table[{S::DetectingMark,    E::MarkDetected}]   = S::CalculatingOffset;
  table[{S::CalculatingOffset, E::Start}]         = S::ApplyingCompensation;
  table[{S::ApplyingCompensation, E::Start}]      = S::Inspecting;

  // 镭射流程
  table[{S::Inspecting,      E::InspectionFinished}] = S::LaserMarking;
  table[{S::LaserMarking,    E::LaserFinished}]   = S::GeneratingQRCode;
  table[{S::GeneratingQRCode, E::Start}]          = S::UnloadingBoard;
  table[{S::UnloadingBoard,  E::UnloadFinished}]  = S::Completed;
  table[{S::Completed,       E::Start}]           = S::WaitingBoard;  // 循环

  // 急停 / 报警（可从任意状态进入）
  table[{S::Idle,               E::EmergencyStop}] = S::EmergencyStopped;
  table[{S::Homing,             E::EmergencyStop}] = S::EmergencyStopped;
  table[{S::WaitingBoard,       E::EmergencyStop}] = S::EmergencyStopped;
  table[{S::LoadingBoard,       E::EmergencyStop}] = S::EmergencyStopped;
  table[{S::BoardArrived,       E::EmergencyStop}] = S::EmergencyStopped;
  table[{S::ReadyForInspection, E::EmergencyStop}] = S::EmergencyStopped;
  table[{S::MovingCameraToMark, E::EmergencyStop}] = S::EmergencyStopped;
  table[{S::CapturingImage,     E::EmergencyStop}] = S::EmergencyStopped;
  table[{S::DetectingMark,      E::EmergencyStop}] = S::EmergencyStopped;
  table[{S::Inspecting,         E::EmergencyStop}] = S::EmergencyStopped;
  table[{S::LaserMarking,       E::EmergencyStop}] = S::EmergencyStopped;
  table[{S::UnloadingBoard,     E::EmergencyStop}] = S::EmergencyStopped;

  table[{S::Idle,               E::AlarmRaised}]   = S::Alarm;
  table[{S::WaitingBoard,       E::AlarmRaised}]   = S::Alarm;
  table[{S::LoadingBoard,       E::AlarmRaised}]   = S::Alarm;

  table[{S::Alarm,              E::AlarmCleared}]  = S::Idle;
  table[{S::EmergencyStopped,   E::Reset}]         = S::Resetting;
  table[{S::Resetting,          E::Start}]         = S::Idle;

  // 停在任意中间状态
  table[{S::LoadingBoard,       E::Stop}]          = S::Idle;
  table[{S::Inspecting,         E::Stop}]          = S::Idle;
  table[{S::LaserMarking,       E::Stop}]          = S::Idle;

  return table;
}

} // namespace

MachineStateMachine::MachineStateMachine()
    : transitions_(buildTransitionTable()) {}

bool MachineStateMachine::sendEvent(const MachineEvent event) {
  const auto key = TransitionKey{currentState_, event};
  const auto it = transitions_.find(key);
  if (it == transitions_.end()) {
    return false; // 当前状态下不接受该事件
  }

  return applyTransition(it->second, event);
}

void MachineStateMachine::tick() {
  // 自动转移逻辑：当某些状态完成后，自动发送下一事件
  // 例如：LoadingBoard 等待 StopperReached，由外部调用 sendEvent
  // tick() 主要用于动画驱动和时间相关条件判断
}

void MachineStateMachine::setEnterAction(const MachineState state, StateAction action) {
  enterActions_[state] = std::move(action);
}

void MachineStateMachine::setExitAction(const MachineState state, StateAction action) {
  exitActions_[state] = std::move(action);
}

MachineState MachineStateMachine::currentState() const { return currentState_; }

void MachineStateMachine::setStateChangeCallback(StateChangeCallback callback) {
  onChange_ = std::move(callback);
}

void MachineStateMachine::enterState(const MachineState state) {
  const auto it = enterActions_.find(state);
  if (it != enterActions_.end() && it->second) {
    it->second();
  }
}

void MachineStateMachine::exitState(const MachineState state) {
  const auto it = exitActions_.find(state);
  if (it != exitActions_.end() && it->second) {
    it->second();
  }
}

bool MachineStateMachine::applyTransition(const MachineState target, const MachineEvent event) {
  const MachineState from = currentState_;
  exitState(from);
  currentState_ = target;
  enterState(target);

  if (onChange_) {
    onChange_(from, target, event);
  }

  return true;
}

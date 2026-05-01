#pragma once

#include <functional>
#include <string>

/// 工业设备主状态
enum class MachineState {
  PowerOff,
  Initializing,
  Idle,
  Homing,
  WaitingBoard,
  LoadingBoard,
  BoardArrived,
  StopperPositioning,
  ReadyForInspection,
  MovingCameraToMark,
  CapturingImage,
  DetectingMark,
  CalculatingOffset,
  ApplyingCompensation,
  Inspecting,
  LaserMarking,
  GeneratingQRCode,
  UnloadingBoard,
  Completed,
  Alarm,
  EmergencyStopped,
  Resetting,
};

/// 驱动状态转移的事件
enum class MachineEvent {
  Start,
  Stop,
  Reset,
  EmergencyStop,
  BoardDetected,
  StopperReached,
  HomeFinished,
  CameraMoveFinished,
  ImageCaptured,
  MarkDetected,
  InspectionFinished,
  LaserFinished,
  UnloadFinished,
  AlarmRaised,
  AlarmCleared,
};

/// 状态机回调：进入/退出状态
using StateAction = std::function<void()>;

inline std::string toString(MachineState state) {
  switch (state) {
  case MachineState::PowerOff:             return "PowerOff";
  case MachineState::Initializing:         return "Initializing";
  case MachineState::Idle:                 return "Idle";
  case MachineState::Homing:                return "Homing";
  case MachineState::WaitingBoard:          return "WaitingBoard";
  case MachineState::LoadingBoard:          return "LoadingBoard";
  case MachineState::BoardArrived:          return "BoardArrived";
  case MachineState::StopperPositioning:    return "StopperPositioning";
  case MachineState::ReadyForInspection:    return "ReadyForInspection";
  case MachineState::MovingCameraToMark:    return "MovingCameraToMark";
  case MachineState::CapturingImage:        return "CapturingImage";
  case MachineState::DetectingMark:         return "DetectingMark";
  case MachineState::CalculatingOffset:     return "CalculatingOffset";
  case MachineState::ApplyingCompensation:  return "ApplyingCompensation";
  case MachineState::Inspecting:            return "Inspecting";
  case MachineState::LaserMarking:          return "LaserMarking";
  case MachineState::GeneratingQRCode:      return "GeneratingQRCode";
  case MachineState::UnloadingBoard:        return "UnloadingBoard";
  case MachineState::Completed:             return "Completed";
  case MachineState::Alarm:                 return "Alarm";
  case MachineState::EmergencyStopped:      return "EmergencyStopped";
  case MachineState::Resetting:             return "Resetting";
  }
  return "Unknown";
}

inline std::string toString(MachineEvent event) {
  switch (event) {
  case MachineEvent::Start:                return "Start";
  case MachineEvent::Stop:                 return "Stop";
  case MachineEvent::Reset:                return "Reset";
  case MachineEvent::EmergencyStop:        return "EmergencyStop";
  case MachineEvent::BoardDetected:        return "BoardDetected";
  case MachineEvent::StopperReached:       return "StopperReached";
  case MachineEvent::HomeFinished:         return "HomeFinished";
  case MachineEvent::CameraMoveFinished:   return "CameraMoveFinished";
  case MachineEvent::ImageCaptured:        return "ImageCaptured";
  case MachineEvent::MarkDetected:         return "MarkDetected";
  case MachineEvent::InspectionFinished:   return "InspectionFinished";
  case MachineEvent::LaserFinished:        return "LaserFinished";
  case MachineEvent::UnloadFinished:       return "UnloadFinished";
  case MachineEvent::AlarmRaised:          return "AlarmRaised";
  case MachineEvent::AlarmCleared:         return "AlarmCleared";
  }
  return "Unknown";
}

#pragma once

#include "coordinate/CoordinateTransformer.h"
#include "motion/VirtualMotionController.h"
#include "program/ProgramModel.h"
#include "transport/VirtualTransportController.h"

#include <initializer_list>

/// 虚拟运控系统门面
///
/// 对上层暴露“可直接用于业务流程”的 API：
/// - 相机相关轴的移动/回零/等待
/// - 进板/出板/挡板/运输等待
/// - 统一 tick 虚拟运动与运输，让动画和业务共用一套时钟
class VirtualMotionSystem final {
public:
  VirtualMotionSystem(VirtualMotionController &motionController,
                      VirtualTransportController &transportController);

  void tick(double deltaSec);
  [[nodiscard]] bool isAdvancing() const;

  void setBoardDefinition(const BoardDefinition &definition);
  [[nodiscard]] const BoardDefinition &boardDefinition() const;

  [[nodiscard]] MechanicalPose currentCameraPose() const;

  [[nodiscard]] bool moveCameraPose(const MechanicalPose &pose,
                                    double speed = 120.0,
                                    double timeoutSec = 8.0);
  [[nodiscard]] bool moveCameraXY(double x,
                                  double y,
                                  double speed = 120.0,
                                  double timeoutSec = 8.0);
  [[nodiscard]] bool homeCameraAxes(double timeoutSec = 8.0);
  [[nodiscard]] bool waitForCameraAxes(double timeoutSec = 8.0);
  [[nodiscard]] bool moveAxis(MotionAxis axis,
                              double position,
                              double speed = 120.0,
                              double timeoutSec = 8.0);
  [[nodiscard]] bool jogAxis(MotionAxis axis,
                             double delta,
                             double speed = 120.0,
                             double timeoutSec = 8.0);
  [[nodiscard]] bool homeAxis(MotionAxis axis, double timeoutSec = 8.0);
  [[nodiscard]] bool waitForAxis(MotionAxis axis, double timeoutSec = 8.0);

  [[nodiscard]] bool loadBoard();
  [[nodiscard]] bool unloadBoard();
  [[nodiscard]] bool raiseStopper();
  [[nodiscard]] bool lowerStopper();
  void resetBoardTransport();
  [[nodiscard]] bool waitForBoardReady(double timeoutSec = 8.0);
  [[nodiscard]] bool waitForTransportIdle(double timeoutSec = 8.0);

  [[nodiscard]] VirtualMotionController &motionController();
  [[nodiscard]] const VirtualMotionController &motionController() const;
  [[nodiscard]] VirtualTransportController &transportController();
  [[nodiscard]] const VirtualTransportController &transportController() const;

private:
  [[nodiscard]] bool waitForAxes(std::initializer_list<MotionAxis> axes, double timeoutSec);
  [[nodiscard]] bool waitForTransportState(BoardTransportState targetState, double timeoutSec);
  static bool axisSettled(AxisState state);

  VirtualMotionController *motionController_ {nullptr};
  VirtualTransportController *transportController_ {nullptr};
  BoardDefinition boardDefinition_ {};
  bool advancing_ {false};

  static constexpr double kStepSec = 0.016;
};

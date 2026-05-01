#pragma once

#include "transport/ITransportController.h"

class IMotionController;

/// 虚拟运输控制器 — 闭环模拟传送带进板/出板/挡板定位
///
/// 通过 tick() 驱动实时模拟：
///   loadBoard() → 传送带运动 → 板到达挡板 → BoardReady
///   unloadBoard() → 挡板释放 → 传送带运动 → 板离开 → Idle
///
/// 运动状态通过 IMotionController 的 Conveyor/Stopper 轴对外暴露，
/// 动画系统可直接读取轴位置进行渲染。
class VirtualTransportController final : public ITransportController {
public:
  VirtualTransportController();

  /// 注入运动控制器引用，用于写 Conveyor/Stopper 轴位置
  void setMotionController(IMotionController *motion);

  // ── ITransportController 接口 ──
  bool loadBoard() override;
  bool unloadBoard() override;
  bool isBoardReady() const override;
  void resetBoardReadySignal() override;
  BoardTransportState state() const override;
  std::string lastSignalMessage() const override;

  // ── 实时模拟 ──
  void tick(double deltaSec) override;

  // ── IO 操作 ──
  bool raiseStopper();
  bool lowerStopper();
  [[nodiscard]] bool isStopperRaised() const;

  /// 板在传送带上的当前位置（mm），供动画读取
  [[nodiscard]] double boardPosition() const;

  /// 传送带当前速度（mm/s），供动画读取
  [[nodiscard]] double conveyorSpeed() const;
  [[nodiscard]] double stopperTargetPosition() const;

private:
  void updateMotionAxes();

  IMotionController *motion_ {nullptr};
  BoardTransportState state_ {BoardTransportState::Idle};
  std::string lastSignalMessage_ {"No board event yet."};

  double boardPosMm_ {0.0};       // 板在传送带上的位置
  double stopperPosMm_ {500.0};   // 挡板位置
  double conveyorSpeed_ {0.0};    // 当前传送带速度
  double travelTimer_ {0.0};      // 动画计时器
  bool stopperRaised_ {false};

  static constexpr double kTravelSpeed = 200.0;   // mm/s 传送带线速度
  static constexpr double kStopperTarget = 450.0;  // 板到达挡板的目标位置
  static constexpr double kExitTarget = 700.0;     // 板完全离开的位置
};

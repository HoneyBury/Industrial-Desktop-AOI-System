#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include "animation/AnimationItems.h"
#include "motion/IMotionController.h"

#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>

/// 设备动画场景 — 管理所有动画图形项，提供统一更新入口。
///
/// 布局（场景坐标，mm 映射到像素 1:1）:
///   y=0    传送带上轨
///   y=60   板子
///   y=65   传送带下轨
///   y=120  相机头/激光头区域
class DeviceAnimationScene : public QGraphicsScene {
  Q_OBJECT

public:
  explicit DeviceAnimationScene(QObject *parent = nullptr);

  void updateFromMotion(IMotionController &motion, double deltaSec);

  void setBoardLoaded(bool loaded);
  void setBoardHasMarks(bool hasMarks);
  void setStopperRaised(bool raised);
  void setAlarmActive(bool active);
  void setStateText(const QString &text);

private:
  void initScene();

  ConveyorBeltItem *upperBelt_ {nullptr};
  ConveyorBeltItem *lowerBelt_ {nullptr};
  BoardItem *boardItem_ {nullptr};
  StopperItem *stopperItem_ {nullptr};
  CameraHeadItem *cameraHead_ {nullptr};
  LaserHeadItem *laserHead_ {nullptr};
  AlarmIndicatorItem *alarmIndicator_ {nullptr};
  QGraphicsSimpleTextItem *stateLabel_ {nullptr};

  double lastConveyorPos_ {0.0};
  bool conveyorPosInitialized_ {false};

  static constexpr double kSceneWidth = 800.0;
  static constexpr double kSceneHeight = 200.0;
  static constexpr double kBeltY = 0.0;
  static constexpr double kBoardY = 32.0;
  static constexpr double kLowerBeltY = 65.0;
  static constexpr double kHeadY = 130.0;
};

#endif

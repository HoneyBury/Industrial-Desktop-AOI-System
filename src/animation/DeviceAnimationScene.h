#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include "animation/AnimationItems.h"
#include "motion/IMotionController.h"
#include "program/ProgramModel.h"

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
  void setBoardDefinition(const BoardDefinition &definition);

  void setBoardLoaded(bool loaded);
  void setBoardHasMarks(bool hasMarks);
  void setStopperRaised(bool raised);
  void setAlarmActive(bool active);
  void setStateText(const QString &text);

private:
  void initScene();
  void updateTrackGeometry(double deltaSec);
  void applyTrackGeometry();
  static double approach(double current, double target, double deltaSec, double response);
  static double mapBoardLengthToScene(double boardLengthMm);
  static double mapBoardWidthToScene(double boardWidthMm);
  static double mapRailWidthToScene(double railWidthMm);

  ConveyorBeltItem *upperBelt_ {nullptr};
  ConveyorBeltItem *lowerBelt_ {nullptr};
  BoardItem *boardItem_ {nullptr};
  StopperItem *stopperItem_ {nullptr};
  CameraHeadItem *cameraHead_ {nullptr};
  LaserHeadItem *laserHead_ {nullptr};
  AlarmIndicatorItem *alarmIndicator_ {nullptr};
  QGraphicsSimpleTextItem *stateLabel_ {nullptr};
  QGraphicsSimpleTextItem *geometryLabel_ {nullptr};

  double lastConveyorPos_ {0.0};
  bool conveyorPosInitialized_ {false};
  double currentBoardLengthPx_ {120.0};
  double currentBoardHeightPx_ {30.0};
  double currentRailWidthPx_ {28.0};
  double targetBoardLengthPx_ {120.0};
  double targetBoardHeightPx_ {30.0};
  double targetRailWidthPx_ {28.0};
  BoardDefinition boardDefinition_ {};

  static constexpr double kSceneWidth = 800.0;
  static constexpr double kSceneHeight = 200.0;
  static constexpr double kTrackCenterY = 48.0;
  static constexpr double kHeadY = 130.0;
};

#endif

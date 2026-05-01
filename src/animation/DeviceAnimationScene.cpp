#ifdef AOI_HAS_QT_WIDGETS

#include "animation/DeviceAnimationScene.h"

#include <algorithm>
#include <QFont>

namespace {

constexpr double kStopperTargetMm = 450.0;
constexpr double kExitTargetMm = 700.0;
constexpr double kBoardEntryX = -180.0;
constexpr double kBoardExitX = 860.0;
constexpr double kStopperX = 744.0;

} // namespace

DeviceAnimationScene::DeviceAnimationScene(QObject *parent) : QGraphicsScene(parent) {
  initScene();
}

void DeviceAnimationScene::initScene() {
  setSceneRect(0, 0, kSceneWidth, kSceneHeight);
  setBackgroundBrush(QColor(25, 25, 30));

  upperBelt_ = new ConveyorBeltItem();
  addItem(upperBelt_);

  lowerBelt_ = new ConveyorBeltItem();
  addItem(lowerBelt_);

  boardItem_ = new BoardItem();
  boardItem_->setVisible(false);
  addItem(boardItem_);

  stopperItem_ = new StopperItem();
  addItem(stopperItem_);

  cameraHead_ = new CameraHeadItem();
  cameraHead_->setPos(kStopperX, kHeadY);
  addItem(cameraHead_);

  laserHead_ = new LaserHeadItem();
  laserHead_->setPos(kStopperX - 20.0, kHeadY);
  addItem(laserHead_);

  alarmIndicator_ = new AlarmIndicatorItem();
  alarmIndicator_->setPos(kSceneWidth - 30, 15);
  addItem(alarmIndicator_);

  QFont font;
  font.setPixelSize(12);
  stateLabel_ = new QGraphicsSimpleTextItem();
  stateLabel_->setFont(font);
  stateLabel_->setBrush(QColor(200, 200, 200));
  stateLabel_->setPos(10, kHeadY + 30);
  stateLabel_->setText(QStringLiteral("Idle"));
  addItem(stateLabel_);

  geometryLabel_ = new QGraphicsSimpleTextItem();
  geometryLabel_->setFont(font);
  geometryLabel_->setBrush(QColor(148, 163, 184));
  geometryLabel_->setPos(10, kHeadY + 48);
  addItem(geometryLabel_);

  setBoardDefinition(boardDefinition_);
  applyTrackGeometry();
}

void DeviceAnimationScene::updateFromMotion(IMotionController &motion, double deltaSec) {
  updateTrackGeometry(deltaSec);

  const double conveyorPos = motion.getAxisPosition(MotionAxis::Conveyor);

  // 传送带速度：根据传送带轴位置变化率计算，映射到像素/秒
  double beltPixelSpeed = 0.0;
  if (conveyorPosInitialized_) {
    const double deltaMm = conveyorPos - lastConveyorPos_;
    // mm/s → px/s (1mm → ~1px in scene coords)
    beltPixelSpeed = (deltaSec > 0.001) ? (deltaMm / deltaSec) * 1.5 : 0.0;
  }
  lastConveyorPos_ = conveyorPos;
  conveyorPosInitialized_ = true;

  upperBelt_->setSpeed(beltPixelSpeed);
  lowerBelt_->setSpeed(beltPixelSpeed);
  upperBelt_->tick(deltaSec);
  lowerBelt_->tick(deltaSec);

  // 板子位置：跟随传送带轴位置
  const double readyBoardX = kStopperX - currentBoardLengthPx_;
  double boardX = readyBoardX;
  if (conveyorPos <= kStopperTargetMm) {
    const double loadRatio = std::clamp(conveyorPos / kStopperTargetMm, 0.0, 1.0);
    boardX = kBoardEntryX + (readyBoardX - kBoardEntryX) * loadRatio;
  } else {
    const double unloadRatio =
        std::clamp((conveyorPos - kStopperTargetMm) / (kExitTargetMm - kStopperTargetMm), 0.0, 1.0);
    boardX = readyBoardX + (kBoardExitX - readyBoardX) * unloadRatio;
  }
  boardItem_->setPos(boardX, kTrackCenterY - currentBoardHeightPx_ * 0.5);

  // 相机头位置：跟随 CameraX/Y 轴
  const double camX = kStopperX + motion.getAxisPosition(MotionAxis::CameraX) * 0.5;
  const double camY = kHeadY + motion.getAxisPosition(MotionAxis::CameraY) * 0.3;
  cameraHead_->setPos(std::clamp(camX, 30.0, kSceneWidth - 30.0),
                      std::clamp(camY, 88.0, 170.0));

  // 激光头位置：跟随 LaserX/Y 轴（与相机有偏移）
  const double laserX = camX - 20.0 + motion.getAxisPosition(MotionAxis::LaserX) * 0.5;
  const double laserY = camY + motion.getAxisPosition(MotionAxis::LaserY) * 0.3;
  laserHead_->setPos(std::clamp(laserX, 10.0, kSceneWidth - 10.0),
                     std::clamp(laserY, 88.0, 170.0));

  // 挡板状态：从 Stopper 轴读取（>=0.5 = 上升）
  const double stopperVal = motion.getAxisPosition(MotionAxis::Stopper);
  stopperItem_->setRaised(stopperVal >= 0.5);

  // 报警指示灯
  alarmIndicator_->tick(deltaSec);
}

void DeviceAnimationScene::setBoardDefinition(const BoardDefinition &definition) {
  boardDefinition_ = definition;
  targetBoardLengthPx_ = mapBoardLengthToScene(definition.boardLengthMm);
  targetBoardHeightPx_ = mapBoardWidthToScene(definition.boardWidthMm);
  targetRailWidthPx_ = mapRailWidthToScene(definition.railWidthMm);

  geometryLabel_->setText(
      QStringLiteral("板长 %1 mm | 板宽 %2 mm | 轨道 %3 mm")
          .arg(definition.boardLengthMm, 0, 'f', 1)
          .arg(definition.boardWidthMm, 0, 'f', 1)
          .arg(definition.railWidthMm, 0, 'f', 1));
}

void DeviceAnimationScene::setBoardLoaded(bool loaded) {
  boardItem_->setVisible(loaded);
}

void DeviceAnimationScene::setBoardHasMarks(bool hasMarks) {
  boardItem_->setHasMarks(hasMarks, hasMarks ? 2 : 0);
}

void DeviceAnimationScene::setStopperRaised(bool raised) {
  stopperItem_->setRaised(raised);
}

void DeviceAnimationScene::setAlarmActive(bool active) {
  alarmIndicator_->setAlarmActive(active);
  if (active) {
    upperBelt_->setSpeed(0);
    lowerBelt_->setSpeed(0);
  }
}

void DeviceAnimationScene::setStateText(const QString &text) {
  stateLabel_->setText(QStringLiteral("%1").arg(text));
}

void DeviceAnimationScene::updateTrackGeometry(const double deltaSec) {
  currentBoardLengthPx_ = approach(currentBoardLengthPx_, targetBoardLengthPx_, deltaSec, 8.0);
  currentBoardHeightPx_ = approach(currentBoardHeightPx_, targetBoardHeightPx_, deltaSec, 8.0);
  currentRailWidthPx_ = approach(currentRailWidthPx_, targetRailWidthPx_, deltaSec, 8.0);
  applyTrackGeometry();
}

void DeviceAnimationScene::applyTrackGeometry() {
  const double railGapPx = std::clamp(currentBoardHeightPx_ + currentRailWidthPx_ + 6.0, 36.0, 120.0);
  const double upperY = kTrackCenterY - railGapPx * 0.5 - currentRailWidthPx_ * 0.5;
  const double lowerY = kTrackCenterY + railGapPx * 0.5 - currentRailWidthPx_ * 0.5;

  upperBelt_->setBeltSize(kSceneWidth, currentRailWidthPx_);
  upperBelt_->setPos(0.0, upperY);
  lowerBelt_->setBeltSize(kSceneWidth, currentRailWidthPx_);
  lowerBelt_->setPos(0.0, lowerY);

  boardItem_->setBoardSize(currentBoardLengthPx_, currentBoardHeightPx_);
  stopperItem_->setPos(kStopperX, lowerY + currentRailWidthPx_ + 10.0);
}

double DeviceAnimationScene::approach(const double current,
                                      const double target,
                                      const double deltaSec,
                                      const double response) {
  const double alpha = std::clamp(deltaSec * response, 0.0, 1.0);
  return current + (target - current) * alpha;
}

double DeviceAnimationScene::mapBoardLengthToScene(const double boardLengthMm) {
  return std::clamp(boardLengthMm * 0.42, 90.0, 260.0);
}

double DeviceAnimationScene::mapBoardWidthToScene(const double boardWidthMm) {
  return std::clamp(boardWidthMm * 0.22, 18.0, 88.0);
}

double DeviceAnimationScene::mapRailWidthToScene(const double railWidthMm) {
  return std::clamp(railWidthMm * 0.55, 14.0, 42.0);
}

#endif

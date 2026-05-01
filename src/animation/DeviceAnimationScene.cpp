#ifdef AOI_HAS_QT_WIDGETS

#include "animation/DeviceAnimationScene.h"

#include <QFont>

DeviceAnimationScene::DeviceAnimationScene(QObject *parent) : QGraphicsScene(parent) {
  initScene();
}

void DeviceAnimationScene::initScene() {
  setSceneRect(0, 0, kSceneWidth, kSceneHeight);
  setBackgroundBrush(QColor(25, 25, 30));

  upperBelt_ = new ConveyorBeltItem();
  upperBelt_->setBeltSize(kSceneWidth, 28);
  upperBelt_->setPos(0, kBeltY);
  addItem(upperBelt_);

  lowerBelt_ = new ConveyorBeltItem();
  lowerBelt_->setBeltSize(kSceneWidth, 28);
  lowerBelt_->setPos(0, kLowerBeltY);
  addItem(lowerBelt_);

  boardItem_ = new BoardItem();
  boardItem_->setBoardSize(120, 30);
  boardItem_->setPos(-150, kBoardY);
  boardItem_->setVisible(false);
  addItem(boardItem_);

  stopperItem_ = new StopperItem();
  stopperItem_->setPos(kSceneWidth * 0.6, kBoardY + 15);
  addItem(stopperItem_);

  cameraHead_ = new CameraHeadItem();
  cameraHead_->setPos(100, kHeadY);
  addItem(cameraHead_);

  laserHead_ = new LaserHeadItem();
  laserHead_->setPos(80, kHeadY);
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
}

void DeviceAnimationScene::updateFromMotion(IMotionController &motion, double deltaSec) {
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
  const double boardX = std::clamp(conveyorPos, -140.0, kSceneWidth - 140.0) + 20.0;
  boardItem_->setPos(boardX, kBoardY);

  // 相机头位置：跟随 CameraX/Y 轴
  const double camX = 60.0 + motion.getAxisPosition(MotionAxis::CameraX) * 0.5;
  const double camY = kHeadY + motion.getAxisPosition(MotionAxis::CameraY) * 0.3;
  cameraHead_->setPos(std::clamp(camX, 30.0, kSceneWidth - 30.0),
                      std::clamp(camY, 100.0, 170.0));

  // 激光头位置：跟随 LaserX/Y 轴（与相机有偏移）
  const double laserX = camX - 20.0 + motion.getAxisPosition(MotionAxis::LaserX) * 0.5;
  const double laserY = camY + motion.getAxisPosition(MotionAxis::LaserY) * 0.3;
  laserHead_->setPos(std::clamp(laserX, 10.0, kSceneWidth - 10.0),
                     std::clamp(laserY, 100.0, 170.0));

  // 挡板状态：从 Stopper 轴读取（>=0.5 = 上升）
  const double stopperVal = motion.getAxisPosition(MotionAxis::Stopper);
  stopperItem_->setRaised(stopperVal >= 0.5);

  // 报警指示灯
  alarmIndicator_->tick(deltaSec);
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

#endif

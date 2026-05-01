#ifdef AOI_HAS_QT_WIDGETS

#include "animation/AnimationWidget.h"
#include "transport/ITransportController.h"

#include <QDateTime>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QtMath>

AnimationWidget::AnimationWidget(QWidget *parent) : QGraphicsView(parent) {
  scene_ = new DeviceAnimationScene(this);
  setScene(scene_);

  setRenderHint(QPainter::Antialiasing, true);
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setFrameShape(QFrame::NoFrame);
  setMinimumHeight(200);
  setDragMode(QGraphicsView::NoDrag);
  setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  setResizeAnchor(QGraphicsView::AnchorUnderMouse);
  setStyleSheet(QStringLiteral(
      "QGraphicsView { background: #1a1a1e; border: 2px solid #334155; border-radius: 8px; }"));

  timer_ = new QTimer(this);
  timer_->setTimerType(Qt::PreciseTimer);
  connect(timer_, &QTimer::timeout, this, &AnimationWidget::tick);
}

void AnimationWidget::setMotionController(IMotionController *controller) {
  motion_ = controller;
}

void AnimationWidget::setTransportController(ITransportController *controller) {
  transport_ = controller;
}

DeviceAnimationScene *AnimationWidget::deviceScene() const { return scene_; }

void AnimationWidget::startAnimation() {
  lastTickNs_ = 0;
  timer_->start(16);
}

void AnimationWidget::stopAnimation() {
  timer_->stop();
}

void AnimationWidget::showBoard(bool visible) {
  scene_->setBoardLoaded(visible);
}

void AnimationWidget::setStopperUp(bool up) {
  scene_->setStopperRaised(up);
}

void AnimationWidget::setAlarm(bool active) {
  scene_->setAlarmActive(active);
}

void AnimationWidget::setDeviceStateText(const QString &text) {
  scene_->setStateText(text);
}

void AnimationWidget::zoomIn() {
  applyZoom(kZoomStep);
}

void AnimationWidget::zoomOut() {
  applyZoom(1.0 / kZoomStep);
}

void AnimationWidget::fitToWindow() {
  QGraphicsView::fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
  syncZoomLevel();
  emit zoomChanged(zoomPercent());
}

int AnimationWidget::zoomPercent() const {
  return qRound(zoomLevel_ * 100.0);
}

void AnimationWidget::wheelEvent(QWheelEvent *event) {
  const double factor = event->angleDelta().y() > 0 ? kZoomStep : 1.0 / kZoomStep;
  applyZoom(factor);
}

void AnimationWidget::resizeEvent(QResizeEvent *event) {
  QGraphicsView::resizeEvent(event);
  // Only auto-fit if user hasn't manually zoomed yet; otherwise keep current zoom
  if (firstShow_ && scene_ != nullptr) {
    QGraphicsView::fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
    syncZoomLevel();
  }
}

void AnimationWidget::showEvent(QShowEvent *event) {
  QGraphicsView::showEvent(event);
  if (firstShow_ && scene_ != nullptr) {
    QGraphicsView::fitInView(scene_->sceneRect(), Qt::KeepAspectRatio);
    syncZoomLevel();
    firstShow_ = false;
    emit zoomChanged(zoomPercent());
  }
}

void AnimationWidget::syncZoomLevel() {
  zoomLevel_ = transform().m11();
}

void AnimationWidget::applyZoom(double factor) {
  const double currentScale = transform().m11();
  const double newScale = currentScale * factor;
  if (newScale < kMinZoom || newScale > kMaxZoom) return;

  scale(factor, factor);
  syncZoomLevel();
  firstShow_ = false; // stop auto-fitting after manual zoom
  emit zoomChanged(zoomPercent());
}

void AnimationWidget::tick() {
  if (motion_ == nullptr) return;

  const qint64 nowNs = QDateTime::currentMSecsSinceEpoch();
  const double deltaSec = (lastTickNs_ > 0) ? static_cast<double>(nowNs - lastTickNs_) / 1000.0 : 0.016;
  lastTickNs_ = nowNs;

  const double dt = std::min(deltaSec, 0.05);

  if (transport_ != nullptr) {
    transport_->tick(dt);
  }

  motion_->tick(dt);

  scene_->updateFromMotion(*motion_, dt);
}

#endif

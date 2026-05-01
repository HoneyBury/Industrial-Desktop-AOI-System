#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include "animation/DeviceAnimationScene.h"
#include "motion/IMotionController.h"

#include <QGraphicsView>
#include <QTimer>

class ITransportController;

/// 设备动画 Widget — 封装 QGraphicsView + DeviceAnimationScene + 60fps Timer
///
/// 支持滚轮缩放、一键适应，可同时驱动运动控制器和运输控制器。
class AnimationWidget : public QGraphicsView {
  Q_OBJECT

public:
  explicit AnimationWidget(QWidget *parent = nullptr);

  void setMotionController(IMotionController *controller);
  void setTransportController(ITransportController *controller);

  [[nodiscard]] DeviceAnimationScene *deviceScene() const;

  void startAnimation();
  void stopAnimation();

  void showBoard(bool visible);
  void setStopperUp(bool up);
  void setAlarm(bool active);
  void setDeviceStateText(const QString &text);

  /// 缩放控制
  void zoomIn();
  void zoomOut();
  void fitToWindow();
  [[nodiscard]] int zoomPercent() const;

signals:
  void zoomChanged(int percent);

protected:
  void wheelEvent(QWheelEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void showEvent(QShowEvent *event) override;

private:
  void tick();
  void applyZoom(double factor);
  void syncZoomLevel();

  DeviceAnimationScene *scene_ {nullptr};
  IMotionController *motion_ {nullptr};
  ITransportController *transport_ {nullptr};
  QTimer *timer_ {nullptr};
  qint64 lastTickNs_ {0};
  double zoomLevel_ {1.0};
  bool firstShow_ {true};

  static constexpr double kMinZoom = 0.3;
  static constexpr double kMaxZoom = 6.0;
  static constexpr double kZoomStep = 1.25;
};

#endif

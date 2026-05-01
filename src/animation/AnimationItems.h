#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include <QGraphicsItem>
#include <QPainter>
#include <QTimer>
#include <QPen>

/// 传送带动画项 — 滚轮+传送带纹理，支持滚动动画
class ConveyorBeltItem : public QGraphicsItem {
public:
  explicit ConveyorBeltItem(QGraphicsItem *parent = nullptr);

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override;

  /// 设置传送带速度和方向（正=向右）
  void setSpeed(double pixelsPerSec);

  /// 每帧更新滚动偏移
  void tick(double deltaSec);

  /// 设置传送带尺寸
  void setBeltSize(double length, double width);

private:
  double beltLength_ = 800.0;
  double beltWidth_ = 30.0;
  double scrollOffset_ = 0.0;
  double speed_ = 0.0;
};

/// 板子项 — PCB 板子矩形，显示 Mark 点和 ROI 标记
class BoardItem : public QGraphicsItem {
public:
  explicit BoardItem(QGraphicsItem *parent = nullptr);

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override;

  void setBoardSize(double width, double height);
  void setHasMarks(bool hasMarks, int markCount = 2);
  void setHasROI(bool hasROI, int roiCount = 3);
  void setQRCodeArea(bool visible);

private:
  double boardWidth_ = 120.0;
  double boardHeight_ = 80.0;
  bool hasMarks_ = true;
  int markCount_ = 2;
  bool hasROI_ = true;
  int roiCount_ = 3;
  bool qrCodeArea_ = true;
};

/// 挡板项 — 可升降的定位挡块
class StopperItem : public QGraphicsItem {
public:
  explicit StopperItem(QGraphicsItem *parent = nullptr);

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override;

  /// true = 升起（阻挡），false = 下降（释放）
  void setRaised(bool raised);
  bool isRaised() const;

private:
  bool raised_ = false;
  static constexpr double kWidth = 10.0;
  static constexpr double kHeight = 50.0;
};

/// 相机头项 — 跟随 CameraX/Y 轴移动
class CameraHeadItem : public QGraphicsItem {
public:
  explicit CameraHeadItem(QGraphicsItem *parent = nullptr);

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override;

private:
  static constexpr double kSize = 24.0;
};

/// 激光头项 — 跟随 LaserX/Y 轴移动
class LaserHeadItem : public QGraphicsItem {
public:
  explicit LaserHeadItem(QGraphicsItem *parent = nullptr);

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override;

private:
  static constexpr double kSize = 20.0;
};

/// 报警指示灯 — 报警或急停时闪烁红色
class AlarmIndicatorItem : public QGraphicsItem {
public:
  explicit AlarmIndicatorItem(QGraphicsItem *parent = nullptr);

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override;

  void setAlarmActive(bool active);
  void tick(double deltaSec);

private:
  bool active_ = false;
  double blinkTimer_ = 0.0;
  bool visible_ = true;
  static constexpr double kRadius = 16.0;
};

#endif

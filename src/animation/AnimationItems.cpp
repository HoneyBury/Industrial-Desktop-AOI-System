#ifdef AOI_HAS_QT_WIDGETS

#include "animation/AnimationItems.h"

#include <QtMath>

// ── ConveyorBeltItem ──────────────────────────

ConveyorBeltItem::ConveyorBeltItem(QGraphicsItem *parent) : QGraphicsItem(parent) {
  setFlag(ItemIsSelectable, false);
}

QRectF ConveyorBeltItem::boundingRect() const {
  return QRectF(0, 0, beltLength_, beltWidth_ + 20);
}

void ConveyorBeltItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
  painter->setRenderHint(QPainter::Antialiasing, true);

  const double yMid = beltWidth_ / 2;
  const double rollerRadius = 10.0;

  // 传送带主体
  painter->setPen(Qt::NoPen);
  painter->setBrush(QColor(100, 100, 105));
  painter->drawRect(QRectF(0, yMid - 4, beltLength_, 8));

  // 传送带纹理线
  painter->setPen(QPen(QColor(130, 130, 135), 1));
  const double spacing = 20.0;
  double offset = std::fmod(scrollOffset_, spacing);
  for (double x = -spacing + offset; x < beltLength_ + spacing; x += spacing) {
    painter->drawLine(QPointF(x, yMid - 4), QPointF(x - 4, yMid + 4));
  }

  // 滚轮
  painter->setPen(QPen(QColor(80, 80, 85), 1.5));
  painter->setBrush(QColor(150, 150, 155));
  const int rollerCount = static_cast<int>(beltLength_ / 60.0) + 1;
  for (int i = 0; i < rollerCount; ++i) {
    const double cx = 30.0 + i * 60.0;
    painter->drawEllipse(QPointF(cx, yMid), rollerRadius, rollerRadius);
  }
}

void ConveyorBeltItem::setSpeed(double pixelsPerSec) { speed_ = pixelsPerSec; }

void ConveyorBeltItem::tick(double deltaSec) {
  scrollOffset_ += speed_ * deltaSec;
  if (scrollOffset_ > 1000.0) scrollOffset_ -= 1000.0;
  if (scrollOffset_ < 0.0) scrollOffset_ += 1000.0;
  update();
}

void ConveyorBeltItem::setBeltSize(double length, double width) {
  beltLength_ = length;
  beltWidth_ = width;
  prepareGeometryChange();
}

// ── BoardItem ─────────────────────────────────

BoardItem::BoardItem(QGraphicsItem *parent) : QGraphicsItem(parent) {
  setFlag(ItemIsMovable, false);
  setFlag(ItemIsSelectable, false);
}

QRectF BoardItem::boundingRect() const {
  return QRectF(-5, -5, boardWidth_ + 10, boardHeight_ + 10);
}

void BoardItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
  painter->setRenderHint(QPainter::Antialiasing, true);

  // 板体
  painter->setPen(QPen(QColor(50, 150, 50), 2));
  painter->setBrush(QColor(30, 100, 30, 180));
  painter->drawRoundedRect(QRectF(0, 0, boardWidth_, boardHeight_), 4, 4);

  // Mark 点（圆形 + 十字）
  if (hasMarks_) {
    painter->setPen(QPen(Qt::yellow, 1.5));
    painter->setBrush(Qt::NoBrush);

    // Mark 1: 左上
    {
      const double cx = boardWidth_ * 0.12, cy = boardHeight_ * 0.12;
      painter->drawEllipse(QPointF(cx, cy), 5, 5);
      painter->drawLine(QPointF(cx - 3, cy), QPointF(cx + 3, cy));
      painter->drawLine(QPointF(cx, cy - 3), QPointF(cx, cy + 3));
    }

    // Mark 2: 右上
    {
      const double cx = boardWidth_ * 0.88, cy = boardHeight_ * 0.88;
      painter->drawEllipse(QPointF(cx, cy), 5, 5);
      painter->drawLine(QPointF(cx - 3, cy), QPointF(cx + 3, cy));
      painter->drawLine(QPointF(cx, cy - 3), QPointF(cx, cy + 3));
    }
  }

  // ROI 矩形
  if (hasROI_) {
    painter->setPen(QPen(QColor(255, 165, 0), 1, Qt::DashLine));
    painter->setBrush(QColor(255, 165, 0, 40));

    // 3 个 ROI 区域
    const double roiW = boardWidth_ * 0.18;
    const double roiH = boardHeight_ * 0.35;
    const double xPositions[] = {boardWidth_ * 0.15, boardWidth_ * 0.41, boardWidth_ * 0.67};
    for (int i = 0; i < roiCount_ && i < 3; ++i) {
      painter->drawRect(QRectF(xPositions[i], boardHeight_ * 0.20, roiW, roiH));
    }
  }

  // QR 码区域
  if (qrCodeArea_) {
    painter->setPen(QPen(QColor(0, 200, 100), 1));
    painter->setBrush(QColor(0, 200, 100, 30));
    painter->drawRect(QRectF(boardWidth_ * 0.78, boardHeight_ * 0.05,
                              boardWidth_ * 0.15, boardHeight_ * 0.15));
    painter->setPen(QPen(QColor(0, 200, 100), 0.5));
    painter->drawText(QRectF(boardWidth_ * 0.78, boardHeight_ * 0.05,
                              boardWidth_ * 0.15, boardHeight_ * 0.15),
                      Qt::AlignCenter, QStringLiteral("QR"));
  }
}

void BoardItem::setBoardSize(double width, double height) {
  boardWidth_ = width;
  boardHeight_ = height;
  prepareGeometryChange();
}

void BoardItem::setHasMarks(bool hasMarks, int markCount) {
  hasMarks_ = hasMarks;
  markCount_ = markCount;
  update();
}

void BoardItem::setHasROI(bool hasROI, int roiCount) {
  hasROI_ = hasROI;
  roiCount_ = roiCount;
  update();
}

void BoardItem::setQRCodeArea(bool visible) {
  qrCodeArea_ = visible;
  update();
}

// ── StopperItem ───────────────────────────────

StopperItem::StopperItem(QGraphicsItem *parent) : QGraphicsItem(parent) {
  setFlag(ItemIsSelectable, false);
}

QRectF StopperItem::boundingRect() const {
  return QRectF(-kWidth / 2 - 2, -kHeight - 4, kWidth + 4, kHeight + 8);
}

void StopperItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
  painter->setRenderHint(QPainter::Antialiasing, true);

  const double shownHeight = raised_ ? kHeight : 5.0;
  painter->setPen(QPen(QColor(180, 50, 50), 2));
  painter->setBrush(raised_ ? QColor(200, 60, 60) : QColor(80, 80, 80));
  painter->drawRoundedRect(QRectF(-kWidth / 2, -shownHeight, kWidth, shownHeight), 2, 2);

  // 挡板标记线
  if (raised_) {
    painter->setPen(QPen(Qt::yellow, 1));
    painter->drawLine(QPointF(-kWidth / 2 + 2, -shownHeight + 6),
                       QPointF(kWidth / 2 - 2, -shownHeight + 6));
  }
}

void StopperItem::setRaised(bool raised) {
  raised_ = raised;
  update();
}

bool StopperItem::isRaised() const { return raised_; }

// ── CameraHeadItem ────────────────────────────

CameraHeadItem::CameraHeadItem(QGraphicsItem *parent) : QGraphicsItem(parent) {
  setFlag(ItemIsSelectable, false);
}

QRectF CameraHeadItem::boundingRect() const {
  return QRectF(-kSize, -kSize, kSize * 2, kSize * 2);
}

void CameraHeadItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
  painter->setRenderHint(QPainter::Antialiasing, true);

  // 镜头圆
  painter->setPen(QPen(QColor(50, 100, 200), 2));
  painter->setBrush(QColor(70, 130, 220, 100));
  painter->drawEllipse(QPointF(0, 0), kSize * 0.6, kSize * 0.6);
  painter->drawEllipse(QPointF(0, 0), kSize * 0.3, kSize * 0.3);

  // 十字线
  painter->setPen(QPen(QColor(50, 100, 200), 1));
  painter->drawLine(QPointF(-kSize, 0), QPointF(-kSize * 0.65, 0));
  painter->drawLine(QPointF(kSize * 0.65, 0), QPointF(kSize, 0));
  painter->drawLine(QPointF(0, -kSize), QPointF(0, -kSize * 0.65));
  painter->drawLine(QPointF(0, kSize * 0.65), QPointF(0, kSize));

  // 标签
  painter->setPen(QPen(QColor(50, 100, 200), 8));
  painter->drawText(QRectF(-kSize * 0.5, kSize * 0.4, kSize, 10),
                    Qt::AlignCenter, QStringLiteral("CAM"));
}

// ── LaserHeadItem ─────────────────────────────

LaserHeadItem::LaserHeadItem(QGraphicsItem *parent) : QGraphicsItem(parent) {
  setFlag(ItemIsSelectable, false);
}

QRectF LaserHeadItem::boundingRect() const {
  return QRectF(-kSize, -kSize, kSize * 2, kSize * 2);
}

void LaserHeadItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
  painter->setRenderHint(QPainter::Antialiasing, true);

  // 激光三角形
  QPolygonF triangle;
  triangle << QPointF(0, -kSize * 0.7) << QPointF(-kSize * 0.5, kSize * 0.5)
           << QPointF(kSize * 0.5, kSize * 0.5);
  painter->setPen(QPen(QColor(200, 50, 50), 2));
  painter->setBrush(QColor(220, 80, 80, 100));
  painter->drawPolygon(triangle);

  // 激光束
  painter->setPen(QPen(QColor(255, 50, 50, 180), 2));
  painter->drawLine(QPointF(0, -kSize * 0.7), QPointF(0, -kSize));

  // 标签
  painter->setPen(QPen(QColor(200, 50, 50), 8));
  painter->drawText(QRectF(-kSize * 0.5, kSize * 0.55, kSize, 10),
                    Qt::AlignCenter, QStringLiteral("LASER"));
}

// ── AlarmIndicatorItem ────────────────────────

AlarmIndicatorItem::AlarmIndicatorItem(QGraphicsItem *parent) : QGraphicsItem(parent) {
  setFlag(ItemIsSelectable, false);
  setVisible(false);
}

QRectF AlarmIndicatorItem::boundingRect() const {
  return QRectF(-kRadius - 4, -kRadius - 4, (kRadius + 4) * 2, (kRadius + 4) * 2);
}

void AlarmIndicatorItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
  if (!visible_) return;

  painter->setRenderHint(QPainter::Antialiasing, true);

  // 外圈光晕
  painter->setPen(Qt::NoPen);
  painter->setBrush(QColor(255, 0, 0, 60));
  painter->drawEllipse(QPointF(0, 0), kRadius + 4, kRadius + 4);

  // 主灯
  painter->setBrush(QColor(255, 30, 30));
  painter->drawEllipse(QPointF(0, 0), kRadius, kRadius);

  // 高光
  painter->setBrush(QColor(255, 150, 150, 150));
  painter->drawEllipse(QPointF(-kRadius * 0.3, -kRadius * 0.3), kRadius * 0.3, kRadius * 0.3);

  // ALARM 文字
  if (active_) {
    painter->setPen(QPen(Qt::white, 7));
    QFont font = painter->font();
    font.setBold(true);
    painter->setFont(font);
    painter->drawText(QRectF(-kRadius, -kRadius, kRadius * 2, kRadius * 2),
                      Qt::AlignCenter, QStringLiteral("!"));
  }
}

void AlarmIndicatorItem::setAlarmActive(bool active) {
  active_ = active;
  visible_ = active;
  setVisible(active);
  update();
}

void AlarmIndicatorItem::tick(double deltaSec) {
  if (!active_) return;
  blinkTimer_ += deltaSec;
  if (blinkTimer_ > 0.5) {
    blinkTimer_ = 0.0;
    visible_ = !visible_;
    update();
  }
}

#endif

#include "ui/CadRulerWidget.h"

#ifdef AOI_HAS_QT_WIDGETS

#include "ui/CadGraphicsView.h"

#include <QPainter>
#include <QScrollBar>

CadRulerWidget::CadRulerWidget(const Qt::Orientation orientation, QWidget *parent)
    : QWidget(parent), orientation_(orientation) {
  setAutoFillBackground(false);
}

void CadRulerWidget::setView(CadGraphicsView *view) {
  view_ = view;
  if (view_ == nullptr) {
    return;
  }

  connect(view_, &CadGraphicsView::viewTransformChanged, this, qOverload<>(&CadRulerWidget::update));
}

void CadRulerWidget::setCursorScenePosition(const QPointF &scenePos) {
  cursorScenePosition_ = scenePos;
  update();
}

void CadRulerWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.fillRect(rect(), QColor("#111827"));
  painter.setPen(QColor("#334155"));
  painter.drawRect(rect().adjusted(0, 0, -1, -1));

  if (view_ == nullptr || view_->scene() == nullptr) {
    return;
  }

  const QRectF visibleSceneRect = view_->mapToScene(view_->viewport()->rect()).boundingRect();
  const qreal step = tickStep();

  painter.setPen(QColor("#94a3b8"));
  painter.setFont(QFont(QStringLiteral("Menlo"), 8));

  if (orientation_ == Qt::Horizontal) {
    const qreal start = std::floor(visibleSceneRect.left() / step) * step;
    const qreal end = visibleSceneRect.right();
    for (qreal value = start; value <= end; value += step) {
      const int x = view_->mapFromScene(QPointF(value, 0.0)).x();
      painter.drawLine(x, height(), x, height() - 12);
      painter.drawText(x + 3, 11, QString::number(value, 'f', 0));
    }

    if (!cursorScenePosition_.isNull()) {
      const int x = view_->mapFromScene(cursorScenePosition_).x();
      painter.setPen(QColor("#facc15"));
      painter.drawLine(x, 0, x, height());
    }
  } else {
    const qreal start = std::floor(visibleSceneRect.top() / step) * step;
    const qreal end = visibleSceneRect.bottom();
    for (qreal value = start; value <= end; value += step) {
      const int y = view_->mapFromScene(QPointF(0.0, value)).y();
      painter.drawLine(width(), y, width() - 12, y);
      painter.save();
      painter.translate(2, y - 2);
      painter.rotate(-90.0);
      painter.drawText(0, 0, QString::number(value, 'f', 0));
      painter.restore();
    }

    if (!cursorScenePosition_.isNull()) {
      const int y = view_->mapFromScene(cursorScenePosition_).y();
      painter.setPen(QColor("#facc15"));
      painter.drawLine(0, y, width(), y);
    }
  }
}

QSize CadRulerWidget::minimumSizeHint() const {
  return orientation_ == Qt::Horizontal ? QSize(100, 26) : QSize(40, 100);
}

qreal CadRulerWidget::tickStep() const {
  if (view_ == nullptr) {
    return 100.0;
  }

  const qreal pixelsPerUnit = view_->transform().m11();
  if (pixelsPerUnit > 3.2) {
    return 20.0;
  }

  if (pixelsPerUnit > 1.8) {
    return 50.0;
  }

  if (pixelsPerUnit > 0.8) {
    return 100.0;
  }

  if (pixelsPerUnit > 0.45) {
    return 200.0;
  }

  return 500.0;
}

#endif

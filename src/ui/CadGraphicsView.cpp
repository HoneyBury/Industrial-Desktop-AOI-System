#include "ui/CadGraphicsView.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QGraphicsRectItem>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

CadGraphicsView::CadGraphicsView(QWidget *parent) : QGraphicsView(parent) {
  setDragMode(QGraphicsView::RubberBandDrag);
  setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  setResizeAnchor(QGraphicsView::AnchorViewCenter);
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  setMouseTracking(true);

  connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, &CadGraphicsView::viewTransformChanged);
  connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &CadGraphicsView::viewTransformChanged);
}

void CadGraphicsView::setDrawingEnabled(const bool enabled) {
  drawingEnabled_ = enabled;
  setDragMode(enabled ? QGraphicsView::NoDrag : QGraphicsView::RubberBandDrag);
}

bool CadGraphicsView::drawingEnabled() const { return drawingEnabled_; }

void CadGraphicsView::fitSceneContent() {
  if (scene() == nullptr || scene()->sceneRect().isNull()) {
    return;
  }

  resetTransform();
  fitInView(scene()->sceneRect(), Qt::KeepAspectRatio);
  zoomFactor_ = transform().m11();
  notifyViewChanged();
}

qreal CadGraphicsView::zoomFactor() const { return zoomFactor_; }

void CadGraphicsView::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::MiddleButton) {
    panning_ = true;
    lastPanPos_ = event->pos();
    viewport()->setCursor(Qt::ClosedHandCursor);
    event->accept();
    return;
  }

  if (drawingEnabled_ && event->button() == Qt::LeftButton && scene() != nullptr) {
    drawing_ = true;
    drawStartScenePos_ = mapToScene(event->pos());
    rubberBandItem_ = scene()->addRect(QRectF(drawStartScenePos_, drawStartScenePos_),
                                       QPen(QColor("#facc15"), 0, Qt::DashLine), QBrush(QColor(250, 204, 21, 18)));
    event->accept();
    return;
  }

  QGraphicsView::mousePressEvent(event);
}

void CadGraphicsView::mouseMoveEvent(QMouseEvent *event) {
  emit cursorScenePositionChanged(mapToScene(event->pos()));

  if (panning_) {
    const QPoint delta = event->pos() - lastPanPos_;
    lastPanPos_ = event->pos();
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    notifyViewChanged();
    event->accept();
    return;
  }

  if (drawing_) {
    updateRubberBandRect(mapToScene(event->pos()));
    event->accept();
    return;
  }

  QGraphicsView::mouseMoveEvent(event);
}

void CadGraphicsView::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::MiddleButton && panning_) {
    panning_ = false;
    viewport()->unsetCursor();
    event->accept();
    return;
  }

  if (event->button() == Qt::LeftButton && drawing_) {
    drawing_ = false;
    const QRectF finalRect(drawStartScenePos_, mapToScene(event->pos()));
    if (rubberBandItem_ != nullptr) {
      scene()->removeItem(rubberBandItem_);
      delete rubberBandItem_;
      rubberBandItem_ = nullptr;
    }

    if (finalRect.normalized().width() >= 4.0 && finalRect.normalized().height() >= 4.0) {
      emit regionDrawn(finalRect.normalized());
    }

    event->accept();
    return;
  }

  QGraphicsView::mouseReleaseEvent(event);
}

void CadGraphicsView::leaveEvent(QEvent *event) {
  emit cursorScenePositionChanged(QPointF());
  QGraphicsView::leaveEvent(event);
}

void CadGraphicsView::wheelEvent(QWheelEvent *event) {
  constexpr qreal scaleFactor = 1.15;
  constexpr qreal minZoom = 0.04;
  constexpr qreal maxZoom = 12.0;

  const qreal candidateZoom = zoomFactor_ * (event->angleDelta().y() > 0 ? scaleFactor : (1.0 / scaleFactor));
  if (candidateZoom < minZoom || candidateZoom > maxZoom) {
    event->accept();
    return;
  }

  if (event->angleDelta().y() > 0) {
    scale(scaleFactor, scaleFactor);
  } else {
    scale(1.0 / scaleFactor, 1.0 / scaleFactor);
  }

  zoomFactor_ = transform().m11();
  notifyViewChanged();
  event->accept();
}

void CadGraphicsView::resizeEvent(QResizeEvent *event) {
  QGraphicsView::resizeEvent(event);
  notifyViewChanged();
}

void CadGraphicsView::drawForeground(QPainter *painter, const QRectF &rect) {
  QGraphicsView::drawForeground(painter, rect);
  painter->save();
  painter->setPen(QPen(QColor(255, 255, 255, 30), 0));
  painter->drawRect(rect);
  painter->restore();
}

void CadGraphicsView::updateRubberBandRect(const QPointF &scenePos) {
  if (rubberBandItem_ == nullptr) {
    return;
  }

  rubberBandItem_->setRect(QRectF(drawStartScenePos_, scenePos).normalized());
}

void CadGraphicsView::notifyViewChanged() { emit viewTransformChanged(); }

#endif

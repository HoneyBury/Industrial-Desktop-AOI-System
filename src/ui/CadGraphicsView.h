#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include <QGraphicsView>

class QGraphicsRectItem;

class CadGraphicsView final : public QGraphicsView {
  Q_OBJECT

public:
  explicit CadGraphicsView(QWidget *parent = nullptr);

  void setDrawingEnabled(bool enabled);
  [[nodiscard]] bool drawingEnabled() const;
  void fitSceneContent();
  [[nodiscard]] qreal zoomFactor() const;

signals:
  void cursorScenePositionChanged(const QPointF &scenePos);
  void viewTransformChanged();
  void regionDrawn(const QRectF &sceneRect);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void drawForeground(QPainter *painter, const QRectF &rect) override;

private:
  void updateRubberBandRect(const QPointF &scenePos);
  void notifyViewChanged();

  bool drawingEnabled_ {false};
  bool panning_ {false};
  bool drawing_ {false};
  qreal zoomFactor_ {1.0};
  QPoint lastPanPos_;
  QPointF drawStartScenePos_;
  QGraphicsRectItem *rubberBandItem_ {nullptr};
};

#endif

#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include <QWidget>

class CadGraphicsView;

class CadRulerWidget final : public QWidget {
  Q_OBJECT

public:
  explicit CadRulerWidget(Qt::Orientation orientation, QWidget *parent = nullptr);

  void setView(CadGraphicsView *view);
  void setCursorScenePosition(const QPointF &scenePos);

protected:
  void paintEvent(QPaintEvent *event) override;
  QSize minimumSizeHint() const override;

private:
  [[nodiscard]] qreal tickStep() const;

  Qt::Orientation orientation_;
  CadGraphicsView *view_ {nullptr};
  QPointF cursorScenePosition_;
};

#endif

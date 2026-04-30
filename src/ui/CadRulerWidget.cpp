#include "ui/CadRulerWidget.h"

#ifdef AOI_HAS_QT_WIDGETS

#include "ui/CadGraphicsView.h"

#include <QPainter>
#include <QScrollBar>

namespace {
constexpr int kRulerBreadth = 32;
constexpr int kMajorTickLength = 14;
constexpr int kMinorTickLength = 8;
constexpr int kMinPixelsBetweenLabels = 38;
} // namespace

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
  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.fillRect(rect(), QColor("#0f172a"));
  painter.setPen(QColor("#334155"));
  painter.drawRect(rect().adjusted(0, 0, -1, -1));

  if (view_ == nullptr || view_->scene() == nullptr) {
    return;
  }

  const QRectF visibleSceneRect = view_->mapToScene(view_->viewport()->rect()).boundingRect();
  const qreal step = tickStep();
  const qreal minorStep = step / 5.0;

  QFont labelFont(QStringLiteral("SF Mono"), 9);
  labelFont.setStyleHint(QFont::Monospace);
  painter.setFont(labelFont);

  if (orientation_ == Qt::Horizontal) {
    const qreal start = std::floor(visibleSceneRect.left() / minorStep) * minorStep;
    const qreal end = visibleSceneRect.right();
    int lastLabelX = -999;

    for (qreal value = start; value <= end; value += minorStep) {
      const int x = view_->mapFromScene(QPointF(value, 0.0)).x();
      if (x < 0 || x > width()) {
        continue;
      }

      const bool isMajor = static_cast<int>(std::round(value / step)) * static_cast<int>(step) ==
                           static_cast<int>(std::round(value));
      if (isMajor) {
        painter.setPen(QColor("#64748b"));
        painter.drawLine(x, height(), x, height() - kMajorTickLength);

        if (x - lastLabelX >= kMinPixelsBetweenLabels) {
          painter.setPen(QColor("#cbd5e1"));
          const QString text = QString::number(value, 'f', 0);
          const int textX = x + 4;
          const int textY = height() - kMajorTickLength - 3;
          painter.drawText(textX, textY, text);
          lastLabelX = x;
        }
      } else {
        painter.setPen(QColor("#334155"));
        painter.drawLine(x, height(), x, height() - kMinorTickLength);
      }
    }

    if (!cursorScenePosition_.isNull()) {
      const int x = view_->mapFromScene(cursorScenePosition_).x();
      painter.setPen(QPen(QColor("#facc15"), 1.5));
      painter.drawLine(x, 0, x, height());
    }
  } else {
    const qreal start = std::floor(visibleSceneRect.top() / minorStep) * minorStep;
    const qreal end = visibleSceneRect.bottom();
    int lastLabelY = -999;

    for (qreal value = start; value <= end; value += minorStep) {
      const int y = view_->mapFromScene(QPointF(0.0, value)).y();
      if (y < 0 || y > height()) {
        continue;
      }

      const bool isMajor = static_cast<int>(std::round(value / step)) * static_cast<int>(step) ==
                           static_cast<int>(std::round(value));
      if (isMajor) {
        painter.setPen(QColor("#64748b"));
        painter.drawLine(width(), y, width() - kMajorTickLength, y);

        if (y - lastLabelY >= kMinPixelsBetweenLabels) {
          painter.setPen(QColor("#cbd5e1"));
          const QString text = QString::number(value, 'f', 0);
          painter.save();
          painter.translate(width() - kMajorTickLength - 4, y + 3);
          painter.rotate(-90.0);
          painter.drawText(0, 0, text);
          painter.restore();
          lastLabelY = y;
        }
      } else {
        painter.setPen(QColor("#334155"));
        painter.drawLine(width(), y, width() - kMinorTickLength, y);
      }
    }

    if (!cursorScenePosition_.isNull()) {
      const int y = view_->mapFromScene(cursorScenePosition_).y();
      painter.setPen(QPen(QColor("#facc15"), 1.5));
      painter.drawLine(0, y, width(), y);
    }
  }
}

QSize CadRulerWidget::minimumSizeHint() const {
  return orientation_ == Qt::Horizontal ? QSize(100, kRulerBreadth) : QSize(kRulerBreadth, 100);
}

QSize CadRulerWidget::sizeHint() const {
  return minimumSizeHint();
}

qreal CadRulerWidget::tickStep() const {
  if (view_ == nullptr) {
    return 100.0;
  }

  const qreal pixelsPerUnit = view_->transform().m11();
  if (pixelsPerUnit > 4.5) {
    return 10.0;
  }

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

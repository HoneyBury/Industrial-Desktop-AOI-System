#include "ui/CameraPreviewWidget.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

CameraPreviewWidget::CameraPreviewWidget(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  previewLabel_ = new QLabel(this);
  previewLabel_->setMinimumSize(480, 360);
  previewLabel_->setAlignment(Qt::AlignCenter);
  previewLabel_->setStyleSheet(QStringLiteral(
      "background: #1e293b; border: 1px solid #334155; border-radius: 6px; color: #64748b;"));
  previewLabel_->setText(QStringLiteral("等待相机…"));
  layout->addWidget(previewLabel_, 1);

  infoLabel_ = new QLabel(this);
  infoLabel_->setStyleSheet(QStringLiteral(
      "color: #94a3b8; font-size: 11px; padding: 2px 4px; background: #0f172a; "
      "border: 1px solid #334155; border-radius: 4px;"));
  infoLabel_->setText(QStringLiteral("未连接"));
  layout->addWidget(infoLabel_);

  refreshTimer_ = new QTimer(this);
  connect(refreshTimer_, &QTimer::timeout, this, &CameraPreviewWidget::refreshFrame);
}

void CameraPreviewWidget::setFrameProvider(FrameProvider provider) {
  frameProvider_ = std::move(provider);
}

void CameraPreviewWidget::setPoseInfoProvider(PoseInfoProvider provider) {
  poseInfoProvider_ = std::move(provider);
}

void CameraPreviewWidget::startRefresh(int intervalMs) {
  refreshTimer_->start(intervalMs);
}

void CameraPreviewWidget::stopRefresh() {
  refreshTimer_->stop();
}

void CameraPreviewWidget::refreshFrame() {
  if (!frameProvider_) {
    return;
  }

  const QImage frame = frameProvider_();
  if (frame.isNull()) {
    return;
  }

  currentFrame_ = frame;

  // Draw overlay on the frame
  QImage displayImage = frame.copy();
  QPainter painter(&displayImage);
  paintOverlay(painter, frame);
  painter.end();

  previewLabel_->setPixmap(
      QPixmap::fromImage(displayImage).scaled(previewLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

  updateInfo();
  emit frameUpdated(frame);
}

void CameraPreviewWidget::paintOverlay(QPainter &painter, const QImage & /*frame*/) {
  painter.setRenderHint(QPainter::Antialiasing);

  const int w = painter.device()->width();
  const int h = painter.device()->height();
  const int cx = w / 2;
  const int cy = h / 2;

  // Crosshair
  QPen crosshairPen(QColor(0, 255, 0, 180), 1);
  painter.setPen(crosshairPen);

  // Horizontal line
  painter.drawLine(0, cy, w, cy);
  // Vertical line
  painter.drawLine(cx, 0, cx, h);

  // Center circle
  painter.drawEllipse(QPointF(cx, cy), 4, 4);

  // Crosshair ticks
  constexpr int tickLen = 8;
  constexpr int tickGap = 20;
  painter.drawLine(cx - tickGap - tickLen, cy, cx - tickGap, cy);
  painter.drawLine(cx + tickGap, cy, cx + tickGap + tickLen, cy);
  painter.drawLine(cx, cy - tickGap - tickLen, cx, cy - tickGap);
  painter.drawLine(cx, cy + tickGap, cx, cy + tickGap + tickLen);

  // Center pixel coordinate label
  painter.setPen(QColor(255, 255, 0, 200));
  QFont labelFont = painter.font();
  labelFont.setPointSize(9);
  painter.setFont(labelFont);
  painter.drawText(cx + 8, cy - 6, QStringLiteral("(%1, %2)").arg(cx).arg(cy));
}

void CameraPreviewWidget::updateInfo() {
  if (!poseInfoProvider_) {
    return;
  }

  const auto &info = poseInfoProvider_();
  QString text;
  if (info.boardVisible) {
    text = QStringLiteral("板可见 | ");
    if (info.centerInsideBoard) {
      text += QStringLiteral("板内中心");
    } else {
      text += QStringLiteral("板外");
    }
    text += QStringLiteral(" | 物理卡 X=%1 Y=%2")
                .arg(info.physicalCardXmm, 0, 'f', 2)
                .arg(info.physicalCardYmm, 0, 'f', 2);
  } else {
    text = QStringLiteral("板不可见");
  }
  text += QStringLiteral(" | 运输: ");
  switch (info.transportState) {
  case BoardTransportState::Idle:
    text += QStringLiteral("空闲");
    break;
  case BoardTransportState::Loading:
    text += QStringLiteral("进板中");
    break;
  case BoardTransportState::BoardReady:
    text += QStringLiteral("板就绪");
    break;
  case BoardTransportState::Unloading:
    text += QStringLiteral("出板中");
    break;
  }
  infoLabel_->setText(text);
  emit positionInfoChanged(text);
}

#endif

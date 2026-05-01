#include "ui/CalibrationCaptureWidget.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QString defaultEmptyPreviewText() {
  return QStringLiteral("等待相机实时画面\n可在主界面先点击“开始实时采图”");
}

} // namespace

CalibrationCaptureWidget::CalibrationCaptureWidget(QWidget *parent) : QWidget(parent) {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(10);

  titleLabel_ = new QLabel(QStringLiteral("校正取图"), this);
  titleLabel_->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: 700; color: #f8fafc;"));

  instructionLabel_ = new QLabel(QStringLiteral("使用实时画面完成取图、冻结与确认。"), this);
  instructionLabel_->setWordWrap(true);
  instructionLabel_->setStyleSheet(QStringLiteral("color: #cbd5e1;"));

  statusLabel_ = new QLabel(QStringLiteral("状态：等待相机"), this);
  statusLabel_->setStyleSheet(QStringLiteral(
      "padding: 6px 10px; border-radius: 8px; background: rgba(30,41,59,0.92); color: #93c5fd;"));

  previewLabel_ = new QLabel(this);
  previewLabel_->setMinimumSize(360, 220);
  previewLabel_->setAlignment(Qt::AlignCenter);
  previewLabel_->setWordWrap(true);
  previewLabel_->setText(defaultEmptyPreviewText());
  previewLabel_->setStyleSheet(QStringLiteral(
      "QLabel { background: #020617; border: 1px solid #334155; border-radius: 12px; color: #94a3b8; }"));

  auto *buttonRow = new QHBoxLayout;
  buttonRow->setSpacing(8);

  freezeButton_ = new QPushButton(QStringLiteral("取图并冻结"), this);
  resumeButton_ = new QPushButton(QStringLiteral("恢复实时"), this);
  useFrameButton_ = new QPushButton(QStringLiteral("确认使用当前帧"), this);

  resumeButton_->setEnabled(false);
  useFrameButton_->setEnabled(false);

  buttonRow->addWidget(freezeButton_);
  buttonRow->addWidget(resumeButton_);
  buttonRow->addStretch();
  buttonRow->addWidget(useFrameButton_);

  rootLayout->addWidget(titleLabel_);
  rootLayout->addWidget(instructionLabel_);
  rootLayout->addWidget(statusLabel_);
  rootLayout->addWidget(previewLabel_, 1);
  rootLayout->addLayout(buttonRow);

  refreshTimer_ = new QTimer(this);
  refreshTimer_->setInterval(90);
  connect(refreshTimer_, &QTimer::timeout, this, [this] { refreshPreview(); });
  refreshTimer_->start();

  connect(freezeButton_, &QPushButton::clicked, this, [this] {
    if (!liveFrame_.isNull()) {
      capturedFrame_ = liveFrame_;
      setFrozen(true);
      updatePreviewPixmap();
    }
  });

  connect(resumeButton_, &QPushButton::clicked, this, [this] {
    clearCapturedFrame();
    setFrozen(false);
    refreshPreview();
  });

  connect(useFrameButton_, &QPushButton::clicked, this, [this] {
    if (!capturedFrame_.isNull()) {
      emit frameAccepted(capturedFrame_);
    }
  });
}

void CalibrationCaptureWidget::setPanelTitle(const QString &title) { titleLabel_->setText(title); }

void CalibrationCaptureWidget::setInstructionText(const QString &text) { instructionLabel_->setText(text); }

void CalibrationCaptureWidget::setFrameProvider(FrameProvider frameProvider, CameraRunningProvider runningProvider) {
  frameProvider_ = std::move(frameProvider);
  runningProvider_ = std::move(runningProvider);
  refreshPreview();
}

QImage CalibrationCaptureWidget::capturedFrame() const { return capturedFrame_; }

bool CalibrationCaptureWidget::hasCapturedFrame() const { return !capturedFrame_.isNull(); }

void CalibrationCaptureWidget::clearCapturedFrame() { capturedFrame_ = QImage(); }

void CalibrationCaptureWidget::refreshPreview() {
  const bool cameraRunning = runningProvider_ ? runningProvider_() : false;
  if (!cameraRunning) {
    if (!frozen_) {
      liveFrame_ = QImage();
      previewLabel_->setPixmap(QPixmap());
      previewLabel_->setText(defaultEmptyPreviewText());
      statusLabel_->setText(QStringLiteral("状态：等待相机"));
      freezeButton_->setEnabled(false);
      resumeButton_->setEnabled(false);
      useFrameButton_->setEnabled(!capturedFrame_.isNull());
    }
    return;
  }

  if (!frozen_ && frameProvider_) {
    liveFrame_ = frameProvider_();
  }

  freezeButton_->setEnabled(!liveFrame_.isNull() && !frozen_);
  resumeButton_->setEnabled(frozen_);
  useFrameButton_->setEnabled(!capturedFrame_.isNull());
  statusLabel_->setText(frozen_ ? QStringLiteral("状态：已冻结，可确认使用当前帧")
                                : QStringLiteral("状态：实时预览中"));
  updatePreviewPixmap();
}

void CalibrationCaptureWidget::updatePreviewPixmap() {
  const QImage frame = frozen_ ? capturedFrame_ : liveFrame_;
  if (frame.isNull()) {
    if (!frozen_) {
      previewLabel_->setPixmap(QPixmap());
      previewLabel_->setText(defaultEmptyPreviewText());
    }
    return;
  }

  previewLabel_->setText({});
  const QSize targetSize = previewLabel_->size() - QSize(10, 10);
  previewLabel_->setPixmap(QPixmap::fromImage(frame).scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void CalibrationCaptureWidget::setFrozen(const bool frozen) {
  frozen_ = frozen;
  emit frozenStateChanged(frozen_);
}

#endif

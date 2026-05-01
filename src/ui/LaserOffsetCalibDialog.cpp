#include "ui/LaserOffsetCalibDialog.h"

#ifdef AOI_HAS_QT_WIDGETS

#include "calibration/LaserOffsetCalibrator.h"
#include "ui/CalibrationJogPanel.h"
#include "ui/CameraPreviewWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

LaserOffsetCalibDialog::LaserOffsetCalibDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("激光偏移校正"));
  resize(960, 700);

  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(12, 12, 12, 12);
  rootLayout->setSpacing(8);

  auto *splitter = new QSplitter(Qt::Horizontal, this);

  cameraPreview_ = new CameraPreviewWidget(this);
  cameraPreview_->setMinimumWidth(480);
  splitter->addWidget(cameraPreview_);

  auto *rightPanel = new QWidget(this);
  auto *rightLayout = new QVBoxLayout(rightPanel);
  rightLayout->setContentsMargins(8, 0, 0, 0);
  rightLayout->setSpacing(8);

  jogPanel_ = new CalibrationJogPanel(this);
  rightLayout->addWidget(jogPanel_);

  auto *actionGroup = new QGroupBox(QStringLiteral("偏移校正动作"), this);
  actionGroup->setStyleSheet(QStringLiteral(
      "QGroupBox { color: #e2e8f0; font-weight: 600; border: 1px solid #334155; "
      "border-radius: 6px; margin-top: 8px; padding-top: 14px; }"
      "QGroupBox::title { subcontrol-origin: margin; left: 10px; }"));
  auto *actionLayout = new QVBoxLayout(actionGroup);
  actionLayout->setSpacing(6);

  const QString primaryBtn = QStringLiteral(
      "QPushButton { background: #2563eb; color: #f8fafc; padding: 8px; border-radius: 6px; font-weight: 600; }"
      "QPushButton:hover { background: #1d4ed8; }"
      "QPushButton:disabled { background: #334155; color: #64748b; }");
  const QString successBtn = QStringLiteral(
      "QPushButton { background: #16a34a; color: #f8fafc; padding: 8px; border-radius: 6px; font-weight: 600; }"
      "QPushButton:hover { background: #15803d; }"
      "QPushButton:disabled { background: #334155; color: #64748b; }");
  const QString secondaryBtn = QStringLiteral(
      "QPushButton { background: #1e293b; color: #e2e8f0; padding: 8px; border-radius: 6px; }"
      "QPushButton:hover { background: #334155; }");

  recordCameraBtn_ = new QPushButton(QStringLiteral("1. 记录相机参考点"), this);
  recordCameraBtn_->setStyleSheet(primaryBtn);
  actionLayout->addWidget(recordCameraBtn_);

  recordLaserBtn_ = new QPushButton(QStringLiteral("2. 记录镭射参考点"), this);
  recordLaserBtn_->setStyleSheet(primaryBtn);
  recordLaserBtn_->setEnabled(false);
  actionLayout->addWidget(recordLaserBtn_);

  computeOffsetBtn_ = new QPushButton(QStringLiteral("3. 计算偏移"), this);
  computeOffsetBtn_->setStyleSheet(primaryBtn);
  computeOffsetBtn_->setEnabled(false);
  actionLayout->addWidget(computeOffsetBtn_);

  applyOffsetBtn_ = new QPushButton(QStringLiteral("4. 应用镭射偏移"), this);
  applyOffsetBtn_->setStyleSheet(successBtn);
  applyOffsetBtn_->setEnabled(false);
  actionLayout->addWidget(applyOffsetBtn_);

  clearBtn_ = new QPushButton(QStringLiteral("清空本次记录"), this);
  clearBtn_->setStyleSheet(QStringLiteral(
      "QPushButton { background: #dc2626; color: #f8fafc; padding: 8px; border-radius: 6px; font-weight: 600; }"
      "QPushButton:hover { background: #b91c1c; }"
      "QPushButton:disabled { background: #334155; color: #64748b; }"));
  clearBtn_->setEnabled(false);
  actionLayout->addWidget(clearBtn_);

  rightLayout->addWidget(actionGroup);

  resultSummaryLabel_ = new QLabel(
      QStringLiteral("操作步骤：\n"
                     "1. 点动相机到某个特征点，点击「记录相机参考点」\n"
                     "2. 想象镭射打到同一特征点，点击「记录镭射参考点」\n"
                     "3. 点击「计算偏移」得到 camera→laser 偏移量\n"
                     "4. 确认后点击「应用镭射偏移」写入程序"),
      this);
  resultSummaryLabel_->setWordWrap(true);
  resultSummaryLabel_->setStyleSheet(QStringLiteral(
      "padding: 10px; border-radius: 6px; background: rgba(30,41,59,0.92); color: #94a3b8; font-size: 12px;"));
  rightLayout->addWidget(resultSummaryLabel_);

  auto *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
  closeBtn->setStyleSheet(secondaryBtn);
  rightLayout->addWidget(closeBtn);

  splitter->addWidget(rightPanel);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 2);
  rootLayout->addWidget(splitter, 1);

  connect(recordCameraBtn_, &QPushButton::clicked, this, &LaserOffsetCalibDialog::onRecordCameraPoint);
  connect(recordLaserBtn_, &QPushButton::clicked, this, &LaserOffsetCalibDialog::onRecordLaserPoint);
  connect(computeOffsetBtn_, &QPushButton::clicked, this, &LaserOffsetCalibDialog::onComputeOffset);
  connect(applyOffsetBtn_, &QPushButton::clicked, this, &LaserOffsetCalibDialog::onApplyOffset);
  connect(clearBtn_, &QPushButton::clicked, this, &LaserOffsetCalibDialog::onClearRecords);
  connect(closeBtn, &QPushButton::clicked, this, [this] { maybeReject(); });
  connect(jogPanel_, &CalibrationJogPanel::jogged, this, &LaserOffsetCalibDialog::onJog);
}

void LaserOffsetCalibDialog::setFrameProvider(FrameProvider frameProvider) {
  frameProvider_ = frameProvider;
  cameraPreview_->setFrameProvider(std::move(frameProvider));
}

void LaserOffsetCalibDialog::setJogProvider(JogProvider jogProvider) {
  jogProvider_ = std::move(jogProvider);
  jogPanel_->setJogCallback([this](double dx, double dy) {
    if (jogProvider_) jogProvider_(dx, dy);
  });
}

void LaserOffsetCalibDialog::setPoseProvider(PoseProvider poseProvider) {
  poseProvider_ = std::move(poseProvider);
}

void LaserOffsetCalibDialog::setPoseInfoProvider(PoseInfoProvider poseInfoProvider) {
  poseInfoProvider_ = std::move(poseInfoProvider);
  cameraPreview_->setPoseInfoProvider(poseInfoProvider_);
}

void LaserOffsetCalibDialog::setProgramContext(const ProgramModel &program) {
  laserModule_.setPixelScale(program.pixelScaleCalibration);
}

void LaserOffsetCalibDialog::onRecordCameraPoint() {
  if (!poseProvider_) return;
  laserModule_.recordCameraPoint(poseProvider_());
  recordCameraBtn_->setEnabled(false);
  recordLaserBtn_->setEnabled(true);
  clearBtn_->setEnabled(true);
  updateResultDisplay();
}

void LaserOffsetCalibDialog::onRecordLaserPoint() {
  if (!poseProvider_) return;
  laserModule_.recordLaserPoint(poseProvider_());
  recordLaserBtn_->setEnabled(false);
  computeOffsetBtn_->setEnabled(true);
  updateResultDisplay();
}

void LaserOffsetCalibDialog::onComputeOffset() {
  if (!laserModule_.hasCameraPoint() || !laserModule_.hasLaserPoint()) return;

  const LaserOffsetCalibration result = laserModule_.computeOffset();
  if (result.calibrated) {
    computeOffsetBtn_->setEnabled(false);
    applyOffsetBtn_->setEnabled(true);
    updateResultDisplay();
  }
}

void LaserOffsetCalibDialog::onApplyOffset() {
  if (!laserModule_.hasComputedResult()) return;

  const LaserOffsetCalibration cal = laserModule_.apply();
  emit calibrationApplied(cal);
  hasUnappliedResult_ = false;

  applyOffsetBtn_->setEnabled(false);
  recordCameraBtn_->setEnabled(true);

  resultSummaryLabel_->setText(
      QStringLiteral("✓ 镭射偏移已应用。\ncamera→laser dX=%1 mm, dY=%2 mm")
          .arg(cal.cameraToLaserDxMm, 0, 'f', 4)
          .arg(cal.cameraToLaserDyMm, 0, 'f', 4));
  resultSummaryLabel_->setStyleSheet(QStringLiteral(
      "padding: 10px; border-radius: 6px; background: rgba(22,163,74,0.18); color: #86efac; font-size: 12px;"));
}

void LaserOffsetCalibDialog::onClearRecords() {
  laserModule_.clear();
  hasUnappliedResult_ = false;

  recordCameraBtn_->setEnabled(true);
  recordLaserBtn_->setEnabled(false);
  computeOffsetBtn_->setEnabled(false);
  applyOffsetBtn_->setEnabled(false);
  clearBtn_->setEnabled(false);

  resultSummaryLabel_->setText(QStringLiteral("已清空本次暂存记录。程序未修改。"));
  resultSummaryLabel_->setStyleSheet(QStringLiteral(
      "padding: 10px; border-radius: 6px; background: rgba(30,41,59,0.92); color: #94a3b8; font-size: 12px;"));
}

void LaserOffsetCalibDialog::onJog(double /*dx*/, double /*dy*/) {
  if (poseProvider_) {
    jogPanel_->setCurrentMechanicalPose(poseProvider_());
  }
  emit cameraJogged(0.0, 0.0);
}

void LaserOffsetCalibDialog::updateResultDisplay() {
  QString text;
  if (laserModule_.hasCameraPoint()) {
    const auto p = laserModule_.cameraReferencePose();
    text += QStringLiteral("相机参考点：X=%1 mm, Y=%2 mm, Z=%3 mm\n")
                .arg(p.x, 0, 'f', 3).arg(p.y, 0, 'f', 3).arg(p.z, 0, 'f', 3);
  }
  if (laserModule_.hasLaserPoint()) {
    const auto p = laserModule_.laserReferencePose();
    text += QStringLiteral("镭射参考点：X=%1 mm, Y=%2 mm, Z=%3 mm\n")
                .arg(p.x, 0, 'f', 3).arg(p.y, 0, 'f', 3).arg(p.z, 0, 'f', 3);
  }
  if (laserModule_.hasComputedResult()) {
    const auto cal = laserModule_.computedResult(); // we need this accessor
    text += QStringLiteral("\n偏移结果：dX=%1 mm, dY=%2 mm")
                .arg(cal.cameraToLaserDxMm, 0, 'f', 4)
                .arg(cal.cameraToLaserDyMm, 0, 'f', 4);
  }
  if (text.isEmpty()) {
    text = QStringLiteral("等待记录参考点…");
  }
  resultSummaryLabel_->setText(text);
  resultSummaryLabel_->setStyleSheet(QStringLiteral(
      "padding: 10px; border-radius: 6px; background: rgba(30,41,59,0.92); color: #facc15; font-size: 12px;"));
}

void LaserOffsetCalibDialog::maybeReject() {
  if (hasUnappliedResult_ || laserModule_.hasUnappliedResult()) {
    auto result = QMessageBox::question(
        this, QStringLiteral("未应用的镭射偏移"),
        QStringLiteral("当前有已计算但未应用的镭射偏移结果。\n\n"
                       "选择「是」丢弃并关闭，选择「否」继续编辑。"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (result != QMessageBox::Yes) return;
  }
  cameraPreview_->stopRefresh();
  accept();
}

void LaserOffsetCalibDialog::showEvent(QShowEvent * /*event*/) {
  cameraPreview_->startRefresh(50);
  if (poseProvider_) {
    jogPanel_->setCurrentMechanicalPose(poseProvider_());
  }
}

#endif

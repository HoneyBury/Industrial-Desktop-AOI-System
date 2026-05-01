#include "ui/OriginCalibDialog.h"

#ifdef AOI_HAS_QT_WIDGETS

#include "calibration/OriginCalibrator.h"
#include "ui/CalibrationJogPanel.h"
#include "ui/CameraPreviewWidget.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

OriginCalibDialog::OriginCalibDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("机械原点校正"));
  resize(960, 680);

  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(12, 12, 12, 12);
  rootLayout->setSpacing(8);

  // ── 左右分割 ──
  auto *splitter = new QSplitter(Qt::Horizontal, this);

  // 左侧：实时相机预览
  cameraPreview_ = new CameraPreviewWidget(this);
  cameraPreview_->setMinimumWidth(480);
  splitter->addWidget(cameraPreview_);

  // 右侧：控制面板
  auto *rightPanel = new QWidget(this);
  auto *rightLayout = new QVBoxLayout(rightPanel);
  rightLayout->setContentsMargins(8, 0, 0, 0);
  rightLayout->setSpacing(8);

  jogPanel_ = new CalibrationJogPanel(this);
  rightLayout->addWidget(jogPanel_);

  // ── 原点校正专用按钮 ──
  auto *actionGroup = new QGroupBox(QStringLiteral("原点校正动作"), this);
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
  const QString dangerBtn = QStringLiteral(
      "QPushButton { background: #dc2626; color: #f8fafc; padding: 8px; border-radius: 6px; font-weight: 600; }"
      "QPushButton:hover { background: #b91c1c; }"
      "QPushButton:disabled { background: #334155; color: #64748b; }");
  const QString secondaryBtn = QStringLiteral(
      "QPushButton { background: #1e293b; color: #e2e8f0; padding: 8px; border-radius: 6px; }"
      "QPushButton:hover { background: #334155; }");

  setReferenceButton_ = new QPushButton(QStringLiteral("设置当前点为原点参考位"), this);
  setReferenceButton_->setStyleSheet(primaryBtn);
  actionLayout->addWidget(setReferenceButton_);

  applyOriginButton_ = new QPushButton(QStringLiteral("应用原点"), this);
  applyOriginButton_->setStyleSheet(primaryBtn);
  applyOriginButton_->setEnabled(false);
  actionLayout->addWidget(applyOriginButton_);

  resetButton_ = new QPushButton(QStringLiteral("重置本次原点结果"), this);
  resetButton_->setStyleSheet(dangerBtn);
  resetButton_->setEnabled(false);
  actionLayout->addWidget(resetButton_);

  rightLayout->addWidget(actionGroup);

  // ── 结果摘要 ──
  resultSummaryLabel_ = new QLabel(
      QStringLiteral("尚未设置原点参考位。\n"
                     "将相机移动到挡板右下参考角点后点击\"设置当前点为原点参考位\"。"),
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

  // ── 信号连接 ──
  connect(setReferenceButton_, &QPushButton::clicked, this, &OriginCalibDialog::onSetOriginReference);
  connect(applyOriginButton_, &QPushButton::clicked, this, &OriginCalibDialog::onApplyOrigin);
  connect(resetButton_, &QPushButton::clicked, this, &OriginCalibDialog::onReset);
  connect(closeBtn, &QPushButton::clicked, this, [this] { maybeReject(); });
  connect(jogPanel_, &CalibrationJogPanel::jogged, this, &OriginCalibDialog::onJog);

  connect(cameraPreview_, &CameraPreviewWidget::frameUpdated, this, [this](const QImage &frame) {
    if (frame.isNull()) return;
    jogPanel_->setCurrentPixelCenter(frame.width() / 2.0, frame.height() / 2.0);
  });
}

void OriginCalibDialog::setFrameProvider(FrameProvider frameProvider) {
  frameProvider_ = frameProvider;
  cameraPreview_->setFrameProvider(std::move(frameProvider));
}

void OriginCalibDialog::setJogProvider(JogProvider jogProvider) {
  jogProvider_ = std::move(jogProvider);
  jogPanel_->setJogCallback([this](double dx, double dy) {
    if (jogProvider_) jogProvider_(dx, dy);
  });
}

void OriginCalibDialog::setPoseProvider(PoseProvider poseProvider) {
  poseProvider_ = std::move(poseProvider);
}

void OriginCalibDialog::setPoseInfoProvider(PoseInfoProvider poseInfoProvider) {
  poseInfoProvider_ = std::move(poseInfoProvider);
  cameraPreview_->setPoseInfoProvider(poseInfoProvider_);
}

void OriginCalibDialog::setProgramContext(const ProgramModel &program) {
  originModule_.setBoardDefinition(program.boardDefinition.boardLengthMm,
                                   program.boardDefinition.boardWidthMm);
}

void OriginCalibDialog::onSetOriginReference() {
  if (!poseProvider_) return;

  const MechanicalPose currentPose = poseProvider_();
  originModule_.setReferencePose(currentPose);
  hasUnappliedChanges_ = true;

  // Capture pixel center from latest frame
  if (frameProvider_) {
    QImage frame = frameProvider_();
    if (!frame.isNull()) {
      originModule_.setReferencePixel(frame.width() / 2.0, frame.height() / 2.0);
    }
  }

  applyOriginButton_->setEnabled(true);
  resetButton_->setEnabled(true);
  setReferenceButton_->setEnabled(false);
  updateResultDisplay();
}

void OriginCalibDialog::onApplyOrigin() {
  if (!originModule_.hasReference()) return;

  const OriginCalibration cal = originModule_.apply();
  const MechanicalPose logicalOrigin = originModule_.calculateLogicalOrigin();
  emit originApplied(logicalOrigin.x, logicalOrigin.y, logicalOrigin.z, logicalOrigin.r);
  hasUnappliedChanges_ = false;

  applyOriginButton_->setEnabled(false);
  resetButton_->setEnabled(false);
  setReferenceButton_->setEnabled(true);

  resultSummaryLabel_->setText(
      QStringLiteral("✓ 原点已应用。\n逻辑原点：X=%1 mm, Y=%2 mm, Z=%3 mm, R=%4°\n"
                     "（基于挡板右下参考位换算）")
          .arg(logicalOrigin.x, 0, 'f', 3)
          .arg(logicalOrigin.y, 0, 'f', 3)
          .arg(logicalOrigin.z, 0, 'f', 3)
          .arg(logicalOrigin.r, 0, 'f', 3));
  resultSummaryLabel_->setStyleSheet(QStringLiteral(
      "padding: 10px; border-radius: 6px; background: rgba(22,163,74,0.18); color: #86efac; font-size: 12px;"));
}

void OriginCalibDialog::onReset() {
  originModule_.reset();
  hasUnappliedChanges_ = false;

  applyOriginButton_->setEnabled(false);
  resetButton_->setEnabled(false);
  setReferenceButton_->setEnabled(true);

  resultSummaryLabel_->setText(QStringLiteral("已重置本次原点结果。程序未修改。"));
  resultSummaryLabel_->setStyleSheet(QStringLiteral(
      "padding: 10px; border-radius: 6px; background: rgba(30,41,59,0.92); color: #94a3b8; font-size: 12px;"));
}

void OriginCalibDialog::onJog(double /*dx*/, double /*dy*/) {
  if (poseProvider_) {
    jogPanel_->setCurrentMechanicalPose(poseProvider_());
  }
  emit cameraJogged(0.0, 0.0);
}

void OriginCalibDialog::updateResultDisplay() {
  if (!originModule_.hasReference()) return;

  const MechanicalPose refPose = originModule_.referenceMachinePose();
  const MechanicalPose logicalOrigin = originModule_.calculateLogicalOrigin();

  resultSummaryLabel_->setText(
      QStringLiteral("挡板右下参考位（机械坐标）：\n"
                     "  X=%1 mm, Y=%2 mm, Z=%3 mm, R=%4°\n"
                     "参考像素中心：（%5, %6）px\n"
                     "换算后逻辑原点：\n"
                     "  X=%7 mm, Y=%8 mm\n\n"
                     "点击「应用原点」将换算结果写入程序。")
          .arg(refPose.x, 0, 'f', 3)
          .arg(refPose.y, 0, 'f', 3)
          .arg(refPose.z, 0, 'f', 3)
          .arg(refPose.r, 0, 'f', 3)
          .arg(originModule_.referencePixelPx(), 0, 'f', 1)
          .arg(originModule_.referencePixelPy(), 0, 'f', 1)
          .arg(logicalOrigin.x, 0, 'f', 3)
          .arg(logicalOrigin.y, 0, 'f', 3));
  resultSummaryLabel_->setStyleSheet(QStringLiteral(
      "padding: 10px; border-radius: 6px; background: rgba(30,41,59,0.92); color: #facc15; font-size: 12px;"));
}

void OriginCalibDialog::maybeReject() {
  if (hasUnappliedChanges_ || originModule_.hasUnappliedChanges()) {
    auto result = QMessageBox::question(
        this, QStringLiteral("未应用的原点校正"),
        QStringLiteral("当前有未应用的原点校正结果。\n\n"
                       "选择「是」丢弃并关闭，选择「否」继续编辑。"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (result != QMessageBox::Yes) return;
  }
  cameraPreview_->stopRefresh();
  accept();
}

void OriginCalibDialog::showEvent(QShowEvent * /*event*/) {
  cameraPreview_->startRefresh(50);
  if (poseProvider_) {
    jogPanel_->setCurrentMechanicalPose(poseProvider_());
  }
}

#endif

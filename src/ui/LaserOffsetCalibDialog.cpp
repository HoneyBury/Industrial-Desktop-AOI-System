#include "ui/LaserOffsetCalibDialog.h"

#ifdef AOI_HAS_QT_WIDGETS

#include "calibration/LaserOffsetCalibrator.h"
#include "ui/CalibrationCaptureWidget.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

LaserOffsetCalibDialog::LaserOffsetCalibDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("激光偏移校正"));
  resize(760, 700);

  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(20, 20, 20, 20);
  rootLayout->setSpacing(14);

  auto *introLabel = new QLabel(
      QStringLiteral("冻结包含激光十字与参考 Mark 的画面，输入激光十字在图像中的像素坐标，"
                     "系统将计算相机光心到激光焦点的物理偏移量（mm）。"),
      this);
  introLabel->setWordWrap(true);
  introLabel->setStyleSheet(QStringLiteral("color: #cbd5e1; font-size: 13px;"));
  rootLayout->addWidget(introLabel);

  captureWidget_ = new CalibrationCaptureWidget(this);
  captureWidget_->setPanelTitle(QStringLiteral("激光偏移取图"));
  captureWidget_->setInstructionText(QStringLiteral("确保激光十字与参考 Mark 同时可见后冻结画面。"));
  rootLayout->addWidget(captureWidget_, 1);

  auto *paramsGroup = new QGroupBox(QStringLiteral("像素坐标输入"), this);
  paramsGroup->setStyleSheet(QStringLiteral(
      "QGroupBox { color: #e2e8f0; font-weight: 600; border: 1px solid #334155; "
      "border-radius: 8px; margin-top: 10px; padding-top: 14px; }"
      "QGroupBox::title { subcontrol-origin: margin; left: 12px; }"));
  auto *paramsLayout = new QFormLayout(paramsGroup);
  paramsLayout->setSpacing(8);

  laserCrossXSpinBox_ = new QDoubleSpinBox(this);
  laserCrossXSpinBox_->setDecimals(1);
  laserCrossXSpinBox_->setRange(0.0, 99999.0);
  laserCrossXSpinBox_->setSuffix(QStringLiteral(" px"));
  laserCrossXSpinBox_->setStyleSheet(QStringLiteral("color: #f8fafc; background: #0f172a;"));
  paramsLayout->addRow(QStringLiteral("激光十字 X (px):"), laserCrossXSpinBox_);

  laserCrossYSpinBox_ = new QDoubleSpinBox(this);
  laserCrossYSpinBox_->setDecimals(1);
  laserCrossYSpinBox_->setRange(0.0, 99999.0);
  laserCrossYSpinBox_->setSuffix(QStringLiteral(" px"));
  laserCrossYSpinBox_->setStyleSheet(QStringLiteral("color: #f8fafc; background: #0f172a;"));
  paramsLayout->addRow(QStringLiteral("激光十字 Y (px):"), laserCrossYSpinBox_);

  opticalCenterValueLabel_ = new QLabel(QStringLiteral("—"), this);
  opticalCenterValueLabel_->setStyleSheet(QStringLiteral("color: #93c5fd; font-weight: 600;"));
  paramsLayout->addRow(QStringLiteral("参考光心 (px):"), opticalCenterValueLabel_);

  rootLayout->addWidget(paramsGroup);

  resultSummaryLabel_ = new QLabel(QStringLiteral("等待输入参数并计算…"), this);
  resultSummaryLabel_->setWordWrap(true);
  resultSummaryLabel_->setStyleSheet(QStringLiteral(
      "padding: 10px 14px; border-radius: 8px; background: rgba(30,41,59,0.92); color: #94a3b8;"));
  rootLayout->addWidget(resultSummaryLabel_);

  auto *buttonBox = new QDialogButtonBox(this);
  applyButton_ = buttonBox->addButton(QStringLiteral("应用校正"), QDialogButtonBox::AcceptRole);
  applyButton_->setEnabled(false);
  applyButton_->setStyleSheet(QStringLiteral(
      "QPushButton { background: #16a34a; color: #f8fafc; padding: 8px 18px; border-radius: 8px; }"
      "QPushButton:disabled { background: #334155; color: #64748b; }"));
  auto *closeButton = buttonBox->addButton(QDialogButtonBox::Close);
  closeButton->setStyleSheet(QStringLiteral(
      "QPushButton { background: #334155; color: #f8fafc; padding: 8px 18px; border-radius: 8px; }"));
  rootLayout->addWidget(buttonBox);

  connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
  connect(applyButton_, &QPushButton::clicked, this, [this] {
    if (hasComputedCalibration_) {
      emit calibrationApplied(computedCalibration_);
      accept();
    }
  });

  connect(laserCrossXSpinBox_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, [this](double) { computeOffset(); });
  connect(laserCrossYSpinBox_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, [this](double) { computeOffset(); });

  connect(captureWidget_, &CalibrationCaptureWidget::frameAccepted, this, [this](const QImage &image) {
    laserCrossXSpinBox_->setValue(image.width() / 2.0);
    laserCrossYSpinBox_->setValue(image.height() / 2.0);
    computeOffset();
  });
}

void LaserOffsetCalibDialog::setFrameProvider(std::function<QImage()> frameProvider,
                                              std::function<bool()> runningProvider) {
  captureWidget_->setFrameProvider(std::move(frameProvider), std::move(runningProvider));
}

void LaserOffsetCalibDialog::setCalibrationContext(const PixelScaleCalibration &pixelScale,
                                                   const PixelPoint &opticalCenter) {
  pixelScale_ = pixelScale;
  opticalCenter_ = opticalCenter;
  opticalCenterValueLabel_->setText(
      QStringLiteral("(%1, %2)").arg(opticalCenter.x, 0, 'f', 1).arg(opticalCenter.y, 0, 'f', 1));
  computeOffset();
}

void LaserOffsetCalibDialog::computeOffset() {
  if (pixelScale_.pixelToMillimeterX <= 0.0 && pixelScale_.pixelToMillimeterY <= 0.0) {
    hasComputedCalibration_ = false;
    resultSummaryLabel_->setText(QStringLiteral("像素比例未标定，请先完成像素比例校正。"));
    updateApplyButtonState();
    return;
  }

  const PixelPoint laserCross {laserCrossXSpinBox_->value(), laserCrossYSpinBox_->value()};

  calibration::LaserOffsetCalibrator calibrator;
  const auto result = calibrator.calibrate(calibration::LaserOffsetCalibrationInput {
      laserCross, opticalCenter_, pixelScale_,
  });

  if (!result) {
    hasComputedCalibration_ = false;
    resultSummaryLabel_->setText(
        QStringLiteral("计算失败：%1").arg(QString::fromStdString(result.message)));
    updateApplyButtonState();
    return;
  }

  computedCalibration_ = result.value.calibration;
  hasComputedCalibration_ = true;

  const double dx = computedCalibration_.cameraToLaserDxMm;
  const double dy = computedCalibration_.cameraToLaserDyMm;
  resultSummaryLabel_->setText(
      QStringLiteral("相机 -> 激光偏移：dX = %1 mm, dY = %2 mm\n"
                     "像素偏移：(%3, %4) px")
          .arg(dx, 0, 'f', 4)
          .arg(dy, 0, 'f', 4)
          .arg(result.value.pixelOffset.x, 0, 'f', 1)
          .arg(result.value.pixelOffset.y, 0, 'f', 1));
  resultSummaryLabel_->setStyleSheet(QStringLiteral(
      "padding: 10px 14px; border-radius: 8px; background: rgba(22,163,74,0.18); "
      "color: #86efac; font-weight: 500;"));
  updateApplyButtonState();
}

void LaserOffsetCalibDialog::updateApplyButtonState() {
  applyButton_->setEnabled(hasComputedCalibration_);
}

#endif

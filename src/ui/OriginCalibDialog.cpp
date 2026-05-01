#include "ui/OriginCalibDialog.h"

#ifdef AOI_HAS_QT_WIDGETS

#include "calibration/OriginCalibrator.h"
#include "ui/CalibrationCaptureWidget.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

OriginCalibDialog::OriginCalibDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("机械原点校正"));
  resize(760, 640);

  auto *layout = new QVBoxLayout(this);
  auto *introLabel = new QLabel(
      QStringLiteral("在这里对机械原点参考位置进行取图与冻结，后续将基于视觉参考点修正设备原点姿态。"),
      this);
  introLabel->setWordWrap(true);
  layout->addWidget(introLabel);

  captureWidget_ = new CalibrationCaptureWidget(this);
  captureWidget_->setPanelTitle(QStringLiteral("原点校正取图"));
  captureWidget_->setInstructionText(
      QStringLiteral("1. 将参考原点位置移动到当前 FOV。\n2. 点击“取图并冻结”。\n3. 确认该帧可作为原点校正输入。"));
  layout->addWidget(captureWidget_, 1);

  resultSummaryLabel_ = new QLabel(QStringLiteral("当前尚未确认任何原点校正图像。"), this);
  resultSummaryLabel_->setWordWrap(true);
  layout->addWidget(resultSummaryLabel_);

  auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
  applyPoseButton_ = new QPushButton(QStringLiteral("应用建议姿态"), this);
  applyPoseButton_->setEnabled(false);
  buttonBox->addButton(applyPoseButton_, QDialogButtonBox::ActionRole);
  layout->addWidget(buttonBox);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(applyPoseButton_, &QPushButton::clicked, this, [this] {
    if (hasCorrectedPose_) {
      emit correctedPoseApplied(correctedPose_.x, correctedPose_.y, correctedPose_.z, correctedPose_.r);
    }
  });

  connect(captureWidget_, &CalibrationCaptureWidget::frameAccepted, this, [this](const QImage &image) {
    calibration::OriginCalibrator calibrator;
    const PixelPoint measuredPoint {image.width() / 2.0, image.height() / 2.0};
    const PixelPoint opticalCenter {calibrationData_.cx > 0.0 ? calibrationData_.cx : image.width() / 2.0,
                                    calibrationData_.cy > 0.0 ? calibrationData_.cy : image.height() / 2.0};
    const auto result = calibrator.calibrate(calibration::OriginCalibrationInput {
        measuredPoint,
        opticalCenter,
        pixelScaleCalibration_,
        currentPose_,
    });
    if (!result) {
      hasCorrectedPose_ = false;
      applyPoseButton_->setEnabled(false);
      resultSummaryLabel_->setText(QStringLiteral("原点校正计算失败：%1").arg(QString::fromStdString(result.message)));
      return;
    }

    correctedPose_ = result.value.correctedPose;
    hasCorrectedPose_ = true;
    applyPoseButton_->setEnabled(true);

    resultSummaryLabel_->setText(
        QStringLiteral("已确认原点校正输入图像：%1 x %2。\n当前机械姿态：X=%3 mm, Y=%4 mm, Z=%5 mm, R=%6°\n视觉偏移：dX=%7 px, dY=%8 px | 约合 %9 mm, %10 mm\n建议补偿后姿态：X=%11 mm, Y=%12 mm, Z=%13 mm, R=%14°")
            .arg(image.width())
            .arg(image.height())
            .arg(currentPose_.x, 0, 'f', 3)
            .arg(currentPose_.y, 0, 'f', 3)
            .arg(currentPose_.z, 0, 'f', 3)
            .arg(currentPose_.r, 0, 'f', 3)
            .arg(result.value.pixelOffset.x, 0, 'f', 2)
            .arg(result.value.pixelOffset.y, 0, 'f', 2)
            .arg(result.value.millimeterOffset.x, 0, 'f', 4)
            .arg(result.value.millimeterOffset.y, 0, 'f', 4)
            .arg(correctedPose_.x, 0, 'f', 3)
            .arg(correctedPose_.y, 0, 'f', 3)
            .arg(correctedPose_.z, 0, 'f', 3)
            .arg(correctedPose_.r, 0, 'f', 3));
  });
}

void OriginCalibDialog::setFrameProvider(std::function<QImage()> frameProvider,
                                         std::function<bool()> runningProvider) {
  if (captureWidget_ != nullptr) {
    captureWidget_->setFrameProvider(std::move(frameProvider), std::move(runningProvider));
  }
}

void OriginCalibDialog::setCalibrationContext(const CameraCalibrationData &calibrationData,
                                              const MechanicalPose &currentPose) {
  calibrationData_ = calibrationData;
  pixelScaleCalibration_.calibrated = true;
  pixelScaleCalibration_.pixelToMillimeterX = calibrationData.pixelToMillimeterX;
  pixelScaleCalibration_.pixelToMillimeterY = calibrationData.pixelToMillimeterY;
  currentPose_ = currentPose;
}

QImage OriginCalibDialog::capturedFrame() const {
  return captureWidget_ != nullptr ? captureWidget_->capturedFrame() : QImage();
}

#endif

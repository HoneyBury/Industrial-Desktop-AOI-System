#include "ui/MarkOffsetDialog.h"

#ifdef AOI_HAS_QT_WIDGETS

#include "alignment/MarkAlignmentSolver.h"
#include "alignment/MarkDetector.h"
#include "ui/CalibrationCaptureWidget.h"

#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

MarkOffsetDialog::MarkOffsetDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("Mark 点校正"));
  resize(760, 640);

  auto *layout = new QVBoxLayout(this);
  auto *introLabel = new QLabel(
      QStringLiteral("在这里对标准 Mark 点进行取图和冻结，后续将用于双 Mark 偏移、角度修正与机械补偿。"),
      this);
  introLabel->setWordWrap(true);
  layout->addWidget(introLabel);

  captureWidget_ = new CalibrationCaptureWidget(this);
  captureWidget_->setPanelTitle(QStringLiteral("Mark 校正取图"));
  captureWidget_->setInstructionText(
      QStringLiteral("1. 调整工位让 Mark 进入 FOV。\n2. 点击“取图并冻结”。\n3. 确认该帧可作为 Mark 校正输入。"));
  layout->addWidget(captureWidget_, 1);

  resultSummaryLabel_ = new QLabel(QStringLiteral("当前尚未确认任何 Mark 校正图像。"), this);
  resultSummaryLabel_->setWordWrap(true);
  layout->addWidget(resultSummaryLabel_);

  auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
  applyCompensationButton_ = new QPushButton(QStringLiteral("应用到虚拟运控"), this);
  applyCompensationButton_->setEnabled(false);
  buttonBox->addButton(applyCompensationButton_, QDialogButtonBox::ActionRole);
  layout->addWidget(buttonBox);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(applyCompensationButton_, &QPushButton::clicked, this, [this] {
    if (hasComputedCompensation_) {
      emit compensationApplied(computedDeltaXmm_, computedDeltaYmm_, computedRotationDegrees_);
    }
  });

  connect(captureWidget_, &CalibrationCaptureWidget::frameAccepted, this, [this](const QImage &image) {
    if (referenceMarks_.empty()) {
      hasComputedCompensation_ = false;
      applyCompensationButton_->setEnabled(false);
      resultSummaryLabel_->setText(
          QStringLiteral("已确认图像：%1 x %2，但当前程序里还没有参考 Mark，暂时无法计算视觉补偿。")
              .arg(image.width())
              .arg(image.height()));
      return;
    }

    const QString tempPath = QDir::temp().filePath(
        QStringLiteral("mark_calibration_%1.png").arg(QDateTime::currentMSecsSinceEpoch()));
    image.save(tempPath);

    alignment::MarkDetector detector;
    const MarkAlgorithm preferredAlgorithm =
        referenceMarks_.front().algorithm == MarkAlgorithm::BinaryGeometry ? MarkAlgorithm::BinaryGeometry
                                                                           : MarkAlgorithm::ColorBrushTemplate;
    const auto detectResult = detector.detect(tempPath.toStdString(), preferredAlgorithm);
    const std::size_t requiredCount = std::min<std::size_t>(referenceMarks_.size(), 2);
    if (!detectResult || detectResult.value.size() < requiredCount) {
      resultSummaryLabel_->setText(QStringLiteral("已确认图像，但当前未能得到足够的 Mark 检测结果。"));
      return;
    }

    alignment::MarkAlignmentSolver solver;
    const auto solveResult = solver.solve(alignment::MarkAlignmentInput {
        referenceMarks_,
        detectResult.value,
        pixelScaleCalibration_,
    });
    if (!solveResult) {
      hasComputedCompensation_ = false;
      applyCompensationButton_->setEnabled(false);
      resultSummaryLabel_->setText(QStringLiteral("Mark 校正计算失败：%1").arg(QString::fromStdString(solveResult.message)));
      return;
    }

    const auto &refA = referenceMarks_[0];
    computedDeltaXmm_ = solveResult.value.millimeterOffset.x;
    computedDeltaYmm_ = solveResult.value.millimeterOffset.y;
    computedRotationDegrees_ = solveResult.value.rotationDegrees;
    hasComputedCompensation_ = true;
    applyCompensationButton_->setEnabled(true);

    if (solveResult.value.mode == alignment::AlignmentMode::DualMarkRigid) {
      const auto &refB = referenceMarks_[1];
      resultSummaryLabel_->setText(
          QStringLiteral("已确认 Mark 校正输入图像：%1 x %2。\n参考 Mark：%3 / %4\n检测偏移：dX=%5 px, dY=%6 px | 约合 %7 mm, %8 mm\n旋转补偿：%9°")
              .arg(image.width())
              .arg(image.height())
              .arg(QString::fromStdString(refA.name))
              .arg(QString::fromStdString(refB.name))
              .arg(solveResult.value.pixelOffset.x, 0, 'f', 2)
              .arg(solveResult.value.pixelOffset.y, 0, 'f', 2)
              .arg(solveResult.value.millimeterOffset.x, 0, 'f', 4)
              .arg(solveResult.value.millimeterOffset.y, 0, 'f', 4)
              .arg(solveResult.value.rotationDegrees, 0, 'f', 3));
    } else {
      resultSummaryLabel_->setText(
          QStringLiteral("已确认单 Mark 校正输入图像：%1 x %2。\n参考 Mark：%3\n检测偏移：dX=%4 px, dY=%5 px | 约合 %6 mm, %7 mm\n旋转补偿：未启用（单 Mark 模式）")
              .arg(image.width())
              .arg(image.height())
              .arg(QString::fromStdString(refA.name))
              .arg(solveResult.value.pixelOffset.x, 0, 'f', 2)
              .arg(solveResult.value.pixelOffset.y, 0, 'f', 2)
              .arg(solveResult.value.millimeterOffset.x, 0, 'f', 4)
              .arg(solveResult.value.millimeterOffset.y, 0, 'f', 4));
    }
  });
}

void MarkOffsetDialog::setFrameProvider(std::function<QImage()> frameProvider,
                                        std::function<bool()> runningProvider) {
  if (captureWidget_ != nullptr) {
    captureWidget_->setFrameProvider(std::move(frameProvider), std::move(runningProvider));
  }
}

void MarkOffsetDialog::setAlignmentContext(const std::vector<MarkPoint> &marks,
                                           const PixelScaleCalibration &pixelScale) {
  referenceMarks_ = marks;
  pixelScaleCalibration_ = pixelScale;
  if (!pixelScaleCalibration_.calibrated) {
    pixelScaleCalibration_.calibrated = true;
    pixelScaleCalibration_.pixelToMillimeterX = 0.01;
    pixelScaleCalibration_.pixelToMillimeterY = 0.01;
  }
}

QImage MarkOffsetDialog::capturedFrame() const {
  return captureWidget_ != nullptr ? captureWidget_->capturedFrame() : QImage();
}

#endif

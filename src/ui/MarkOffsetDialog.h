#pragma once

#ifdef AOI_HAS_QT_WIDGETS
#include <QDialog>

#include "calibration/CalibrationTypes.h"
#include "vision/MarkDetector.h"

#include <QImage>

#include <functional>

class CalibrationCaptureWidget;
class QLabel;
class QPushButton;

class MarkOffsetDialog final : public QDialog {
  Q_OBJECT

public:
  explicit MarkOffsetDialog(QWidget *parent = nullptr);

  void setFrameProvider(std::function<QImage()> frameProvider,
                        std::function<bool()> runningProvider);
  void setAlignmentContext(const std::vector<MarkPoint> &marks,
                           const PixelScaleCalibration &pixelScale);
  [[nodiscard]] QImage capturedFrame() const;

signals:
  void compensationApplied(double deltaXmm, double deltaYmm, double rotationDegrees);

private:
  std::vector<MarkPoint> referenceMarks_;
  PixelScaleCalibration pixelScaleCalibration_ {};
  CalibrationCaptureWidget *captureWidget_ {nullptr};
  QLabel *resultSummaryLabel_ {nullptr};
  QPushButton *applyCompensationButton_ {nullptr};
  double computedDeltaXmm_ {0.0};
  double computedDeltaYmm_ {0.0};
  double computedRotationDegrees_ {0.0};
  bool hasComputedCompensation_ {false};
};
#endif

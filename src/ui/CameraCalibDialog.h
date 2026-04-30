#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include <QDialog>
#include <QString>

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;

class CameraCalibDialog final : public QDialog {
  Q_OBJECT

public:
  explicit CameraCalibDialog(QWidget *parent = nullptr);

  void setDeviceIndex(int deviceIndex);
  [[nodiscard]] int deviceIndex() const;
  [[nodiscard]] double exposureTimeMs() const;
  [[nodiscard]] double gainValue() const;
  [[nodiscard]] QString resolutionPreset() const;

private:
  QSpinBox *deviceIndexSpinBox_ {nullptr};
  QDoubleSpinBox *exposureSpinBox_ {nullptr};
  QDoubleSpinBox *gainSpinBox_ {nullptr};
  QComboBox *resolutionComboBox_ {nullptr};
};

#endif

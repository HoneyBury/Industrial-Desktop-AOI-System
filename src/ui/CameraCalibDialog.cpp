#include "ui/CameraCalibDialog.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

CameraCalibDialog::CameraCalibDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("相机配置"));
  resize(460, 320);

  auto *rootLayout = new QVBoxLayout(this);

  auto *summaryLabel = new QLabel(
      QStringLiteral("配置当前演示环境使用的虚拟整板相机参数。系统会根据整板图、当前 FOV 和运控坐标实时生成画面。"),
      this);
  summaryLabel->setWordWrap(true);
  rootLayout->addWidget(summaryLabel);

  auto *configGroupBox = new QGroupBox(QStringLiteral("基础参数"), this);
  auto *formLayout = new QFormLayout(configGroupBox);

  deviceIndexSpinBox_ = new QSpinBox(configGroupBox);
  deviceIndexSpinBox_->setRange(0, 9);
  exposureSpinBox_ = new QDoubleSpinBox(configGroupBox);
  exposureSpinBox_->setRange(0.1, 1000.0);
  exposureSpinBox_->setDecimals(1);
  exposureSpinBox_->setValue(12.0);
  gainSpinBox_ = new QDoubleSpinBox(configGroupBox);
  gainSpinBox_->setRange(0.0, 48.0);
  gainSpinBox_->setDecimals(1);
  gainSpinBox_->setValue(0.0);
  resolutionComboBox_ = new QComboBox(configGroupBox);
  resolutionComboBox_->addItems(
      {QStringLiteral("640 x 360"), QStringLiteral("1280 x 720"), QStringLiteral("1920 x 1080")});

  formLayout->addRow(QStringLiteral("设备索引"), deviceIndexSpinBox_);
  formLayout->addRow(QStringLiteral("曝光时间(ms)"), exposureSpinBox_);
  formLayout->addRow(QStringLiteral("增益(dB)"), gainSpinBox_);
  formLayout->addRow(QStringLiteral("分辨率预设"), resolutionComboBox_);
  rootLayout->addWidget(configGroupBox);

  auto *noteGroupBox = new QGroupBox(QStringLiteral("说明"), this);
  auto *noteLayout = new QVBoxLayout(noteGroupBox);
  noteLayout->addWidget(new QLabel(QStringLiteral("1. 分辨率会直接影响虚拟相机输出帧大小和 FOV 预览效果。"), noteGroupBox));
  noteLayout->addWidget(new QLabel(QStringLiteral("2. 设备索引在虚拟模式下仅保留兼容字段，当前不会访问系统摄像头。"), noteGroupBox));
  rootLayout->addWidget(noteGroupBox);

  auto *buttonBox =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  rootLayout->addWidget(buttonBox);
}

void CameraCalibDialog::setDeviceIndex(const int deviceIndex) { deviceIndexSpinBox_->setValue(deviceIndex); }

void CameraCalibDialog::setExposureTimeMs(const double exposureTimeMs) { exposureSpinBox_->setValue(exposureTimeMs); }

void CameraCalibDialog::setGainValue(const double gainValue) { gainSpinBox_->setValue(gainValue); }

void CameraCalibDialog::setResolutionPreset(const QString &preset) {
  const int index = resolutionComboBox_->findText(preset);
  if (index >= 0) {
    resolutionComboBox_->setCurrentIndex(index);
  }
}

int CameraCalibDialog::deviceIndex() const { return deviceIndexSpinBox_->value(); }

double CameraCalibDialog::exposureTimeMs() const { return exposureSpinBox_->value(); }

double CameraCalibDialog::gainValue() const { return gainSpinBox_->value(); }

QString CameraCalibDialog::resolutionPreset() const { return resolutionComboBox_->currentText(); }

#endif

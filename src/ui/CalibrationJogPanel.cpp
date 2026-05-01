#include "ui/CalibrationJogPanel.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

CalibrationJogPanel::CalibrationJogPanel(QWidget *parent) : QWidget(parent) {
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(10);

  // ── Step Size ──
  auto *stepGroup = new QGroupBox(QStringLiteral("步长"), this);
  stepGroup->setStyleSheet(QStringLiteral(
      "QGroupBox { color: #e2e8f0; font-weight: 600; border: 1px solid #334155; "
      "border-radius: 6px; margin-top: 8px; padding-top: 14px; }"
      "QGroupBox::title { subcontrol-origin: margin; left: 10px; }"));
  auto *stepLayout = new QVBoxLayout(stepGroup);
  stepLayout->setSpacing(6);

  stepSizeSpinBox_ = new QDoubleSpinBox(this);
  stepSizeSpinBox_->setDecimals(3);
  stepSizeSpinBox_->setRange(0.001, 100.0);
  stepSizeSpinBox_->setValue(1.0);
  stepSizeSpinBox_->setSuffix(QStringLiteral(" mm"));
  stepSizeSpinBox_->setStyleSheet(QStringLiteral("color: #f8fafc; background: #0f172a; padding: 4px;"));
  connect(stepSizeSpinBox_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](double val) {
            currentStep_ = val;
            emit stepSizeChanged(val);
          });
  stepLayout->addWidget(stepSizeSpinBox_);

  auto *presetLayout = new QGridLayout;
  presetLayout->setSpacing(4);
  step01Button_ = new QPushButton(QStringLiteral("0.01"), this);
  step1Button_ = new QPushButton(QStringLiteral("0.10"), this);
  step1_0Button_ = new QPushButton(QStringLiteral("1.00"), this);
  step5_0Button_ = new QPushButton(QStringLiteral("5.00"), this);
  const QString btnStyle = QStringLiteral(
      "QPushButton { background: #1e293b; color: #e2e8f0; border: 1px solid #475569; "
      "border-radius: 4px; padding: 6px; font-size: 12px; }"
      "QPushButton:hover { background: #334155; }");
  for (auto *btn : {step01Button_, step1Button_, step1_0Button_, step5_0Button_}) {
    btn->setStyleSheet(btnStyle);
  }
  connect(step01Button_, &QPushButton::clicked, this, [this] { onPresetStep(0.01); });
  connect(step1Button_, &QPushButton::clicked, this, [this] { onPresetStep(0.1); });
  connect(step1_0Button_, &QPushButton::clicked, this, [this] { onPresetStep(1.0); });
  connect(step5_0Button_, &QPushButton::clicked, this, [this] { onPresetStep(5.0); });
  presetLayout->addWidget(step01Button_, 0, 0);
  presetLayout->addWidget(step1Button_, 0, 1);
  presetLayout->addWidget(step1_0Button_, 0, 2);
  presetLayout->addWidget(step5_0Button_, 0, 3);
  stepLayout->addLayout(presetLayout);
  root->addWidget(stepGroup);

  // ── Jog Controls ──
  auto *jogGroup = new QGroupBox(QStringLiteral("点动控制"), this);
  jogGroup->setStyleSheet(stepGroup->styleSheet());
  auto *jogLayout = new QGridLayout(jogGroup);
  jogLayout->setSpacing(4);

  const QString jogBtnStyle = QStringLiteral(
      "QPushButton { background: #0f172a; color: #e2e8f0; border: 1px solid #475569; "
      "border-radius: 4px; padding: 10px 16px; font-size: 14px; font-weight: bold; }"
      "QPushButton:hover { background: #1e293b; border-color: #60a5fa; }"
      "QPushButton:pressed { background: #1d4ed8; }");

  jogUpButton_ = new QPushButton(QStringLiteral("▲"), this);
  jogDownButton_ = new QPushButton(QStringLiteral("▼"), this);
  jogLeftButton_ = new QPushButton(QStringLiteral("◀"), this);
  jogRightButton_ = new QPushButton(QStringLiteral("▶"), this);
  for (auto *btn : {jogUpButton_, jogDownButton_, jogLeftButton_, jogRightButton_}) {
    btn->setStyleSheet(jogBtnStyle);
  }

  jogLayout->addWidget(jogUpButton_, 0, 1);
  jogLayout->addWidget(jogLeftButton_, 1, 0);
  jogLayout->addWidget(jogRightButton_, 1, 2);
  jogLayout->addWidget(jogDownButton_, 2, 1);

  connect(jogUpButton_, &QPushButton::clicked, this, &CalibrationJogPanel::onJogUp);
  connect(jogDownButton_, &QPushButton::clicked, this, &CalibrationJogPanel::onJogDown);
  connect(jogLeftButton_, &QPushButton::clicked, this, &CalibrationJogPanel::onJogLeft);
  connect(jogRightButton_, &QPushButton::clicked, this, &CalibrationJogPanel::onJogRight);
  root->addWidget(jogGroup);

  // ── Coordinate Display ──
  auto *coordGroup = new QGroupBox(QStringLiteral("坐标"), this);
  coordGroup->setStyleSheet(stepGroup->styleSheet());
  auto *coordLayout = new QFormLayout(coordGroup);
  coordLayout->setSpacing(4);

  const QString valStyle = QStringLiteral("color: #93c5fd; font-weight: 600; font-size: 13px;");
  const QString labelStyle = QStringLiteral("color: #94a3b8; font-size: 12px;");

  machineXLabel_ = new QLabel(QStringLiteral("—"), this);
  machineXLabel_->setStyleSheet(valStyle);
  machineYLabel_ = new QLabel(QStringLiteral("—"), this);
  machineYLabel_->setStyleSheet(valStyle);
  machineZLabel_ = new QLabel(QStringLiteral("—"), this);
  machineZLabel_->setStyleSheet(valStyle);
  machineRLabel_ = new QLabel(QStringLiteral("—"), this);
  machineRLabel_->setStyleSheet(valStyle);

  auto *mk = new QLabel(QStringLiteral("机械 X (mm):"), this);
  mk->setStyleSheet(labelStyle);
  coordLayout->addRow(mk, machineXLabel_);
  auto *my = new QLabel(QStringLiteral("机械 Y (mm):"), this);
  my->setStyleSheet(labelStyle);
  coordLayout->addRow(my, machineYLabel_);
  auto *mz = new QLabel(QStringLiteral("机械 Z (mm):"), this);
  mz->setStyleSheet(labelStyle);
  coordLayout->addRow(mz, machineZLabel_);
  auto *mr = new QLabel(QStringLiteral("机械 R (°):"), this);
  mr->setStyleSheet(labelStyle);
  coordLayout->addRow(mr, machineRLabel_);

  pixelPxLabel_ = new QLabel(QStringLiteral("—"), this);
  pixelPxLabel_->setStyleSheet(valStyle);
  pixelPyLabel_ = new QLabel(QStringLiteral("—"), this);
  pixelPyLabel_->setStyleSheet(valStyle);

  auto *ppx = new QLabel(QStringLiteral("像素 Px:"), this);
  ppx->setStyleSheet(labelStyle);
  coordLayout->addRow(ppx, pixelPxLabel_);
  auto *ppy = new QLabel(QStringLiteral("像素 Py:"), this);
  ppy->setStyleSheet(labelStyle);
  coordLayout->addRow(ppy, pixelPyLabel_);

  root->addWidget(coordGroup);
  root->addStretch(1);
}

void CalibrationJogPanel::setJogCallback(JogCallback callback) {
  jogCallback_ = std::move(callback);
}

void CalibrationJogPanel::setCurrentMechanicalPose(const MechanicalPose &pose) {
  currentPose_ = pose;
  updateCoordinateDisplay();
}

void CalibrationJogPanel::setCurrentPixelCenter(double px, double py) {
  currentPx_ = px;
  currentPy_ = py;
  updateCoordinateDisplay();
}

void CalibrationJogPanel::setStepSize(double mm) {
  currentStep_ = mm;
  stepSizeSpinBox_->setValue(mm);
}

double CalibrationJogPanel::stepSize() const {
  return currentStep_;
}

void CalibrationJogPanel::onPresetStep(double mm) {
  setStepSize(mm);
  emit stepSizeChanged(mm);
}

void CalibrationJogPanel::onJogUp() {
  // Y+ moves camera south (down) in FOV; negate so ▲ moves view upward
  if (jogCallback_) {
    jogCallback_(0.0, -currentStep_);
  }
  emit jogged(0.0, -currentStep_);
}

void CalibrationJogPanel::onJogDown() {
  // Y- moves camera north (up) in FOV; negate so ▼ moves view downward
  if (jogCallback_) {
    jogCallback_(0.0, currentStep_);
  }
  emit jogged(0.0, currentStep_);
}

void CalibrationJogPanel::onJogLeft() {
  if (jogCallback_) {
    jogCallback_(-currentStep_, 0.0);
  }
  emit jogged(-currentStep_, 0.0);
}

void CalibrationJogPanel::onJogRight() {
  if (jogCallback_) {
    jogCallback_(currentStep_, 0.0);
  }
  emit jogged(currentStep_, 0.0);
}

void CalibrationJogPanel::updateCoordinateDisplay() {
  machineXLabel_->setText(QString::number(currentPose_.x, 'f', 3));
  machineYLabel_->setText(QString::number(currentPose_.y, 'f', 3));
  machineZLabel_->setText(QString::number(currentPose_.z, 'f', 3));
  machineRLabel_->setText(QString::number(currentPose_.r, 'f', 3));
  pixelPxLabel_->setText(QString::number(currentPx_, 'f', 1));
  pixelPyLabel_->setText(QString::number(currentPy_, 'f', 1));
}

#endif

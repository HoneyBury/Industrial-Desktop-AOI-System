#include "ui/MotionControlDialog.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QDateTime>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

QString formatAxisValue(const MotionAxis axis, const std::optional<double> &position) {
  if (!position.has_value()) {
    return QStringLiteral("--");
  }

  return axis == MotionAxis::R ? QStringLiteral("%1 deg").arg(position.value(), 0, 'f', 3)
                               : QStringLiteral("%1 mm").arg(position.value(), 0, 'f', 3);
}

QDoubleSpinBox *buildTargetSpinBox(const MotionAxis axis) {
  auto *spinBox = new QDoubleSpinBox;
  spinBox->setDecimals(3);
  spinBox->setRange(axis == MotionAxis::R ? -360.0 : -9999.0, axis == MotionAxis::R ? 360.0 : 9999.0);
  return spinBox;
}

QDoubleSpinBox *buildStepSpinBox(const MotionAxis axis) {
  auto *spinBox = new QDoubleSpinBox;
  spinBox->setDecimals(3);
  spinBox->setRange(0.001, axis == MotionAxis::R ? 360.0 : 999.0);
  spinBox->setValue(1.0);
  return spinBox;
}

} // namespace

MotionControlDialog::MotionControlDialog(VirtualMotionController *controller, QWidget *parent)
    : QDialog(parent), controller_(controller) {
  setWindowTitle(QStringLiteral("虚拟运动控制面板"));
  resize(920, 720);
  buildUi();
  refreshUi();
  appendLog(QStringLiteral("运控面板已打开。"));
}

void MotionControlDialog::buildUi() {
  auto *rootLayout = new QVBoxLayout(this);

  auto *summaryGroupBox = new QGroupBox(QStringLiteral("全局控制"), this);
  auto *summaryLayout = new QHBoxLayout(summaryGroupBox);
  emergencyStopButton_ = new QPushButton(QStringLiteral("急停"), summaryGroupBox);
  emergencyStopButton_->setStyleSheet(
      QStringLiteral("QPushButton { background: #b42318; color: white; font-weight: 700; "
                     "padding: 8px 18px; border-radius: 8px; }"));
  resetStopButton_ = new QPushButton(QStringLiteral("复位急停"), summaryGroupBox);
  resetStopButton_->setStyleSheet(
      QStringLiteral("QPushButton { background: #175cd3; color: white; font-weight: 700; "
                     "padding: 8px 18px; border-radius: 8px; }"));
  summaryLayout->addWidget(new QLabel(QStringLiteral("对当前虚拟 X/Y/Z/R 平台执行调试控制。"), summaryGroupBox));
  summaryLayout->addStretch();
  summaryLayout->addWidget(emergencyStopButton_);
  summaryLayout->addWidget(resetStopButton_);
  rootLayout->addWidget(summaryGroupBox);

  auto *gridLayout = new QGridLayout;
  rootLayout->addLayout(gridLayout);

  const auto buildAxisPanel = [this](const QString &title,
                                     QLabel *&positionLabel,
                                     QLabel *&stateLabel,
                                     QDoubleSpinBox *&targetSpinBox,
                                     QDoubleSpinBox *&stepSpinBox,
                                     MotionAxis axis) {
    auto *groupBox = new QGroupBox(title, this);
    auto *layout = new QGridLayout(groupBox);

    positionLabel = new QLabel(QStringLiteral("0.000"), groupBox);
    positionLabel->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 700; color: #111827;"));
    stateLabel = new QLabel(QStringLiteral("可操作"), groupBox);
    targetSpinBox = buildTargetSpinBox(axis);
    stepSpinBox = buildStepSpinBox(axis);

    auto *absoluteButton = new QPushButton(QStringLiteral("执行绝对移动"), groupBox);
    auto *negativeButton = new QPushButton(QStringLiteral("负向点动"), groupBox);
    auto *positiveButton = new QPushButton(QStringLiteral("正向点动"), groupBox);
    auto *homeButton = new QPushButton(QStringLiteral("回零"), groupBox);

    layout->addWidget(new QLabel(QStringLiteral("当前位置"), groupBox), 0, 0);
    layout->addWidget(positionLabel, 0, 1);
    layout->addWidget(new QLabel(QStringLiteral("轴状态"), groupBox), 1, 0);
    layout->addWidget(stateLabel, 1, 1);
    layout->addWidget(new QLabel(QStringLiteral("绝对目标"), groupBox), 2, 0);
    layout->addWidget(targetSpinBox, 2, 1);
    layout->addWidget(absoluteButton, 3, 0, 1, 2);
    layout->addWidget(new QLabel(QStringLiteral("点动步长"), groupBox), 4, 0);
    layout->addWidget(stepSpinBox, 4, 1);
    layout->addWidget(negativeButton, 5, 0);
    layout->addWidget(positiveButton, 5, 1);
    layout->addWidget(homeButton, 6, 0, 1, 2);

    connect(absoluteButton, &QPushButton::clicked, this, [this, axis] { moveAbsolute(axis); });
    connect(negativeButton, &QPushButton::clicked, this, [this, axis] { jog(axis, -1.0); });
    connect(positiveButton, &QPushButton::clicked, this, [this, axis] { jog(axis, 1.0); });
    connect(homeButton, &QPushButton::clicked, this, [this, axis] { home(axis); });

    return groupBox;
  };

  gridLayout->addWidget(buildAxisPanel(QStringLiteral("X 轴"), xPositionValueLabel_, xStateValueLabel_,
                                       xTargetSpinBox_, xStepSpinBox_, MotionAxis::X),
                        0, 0);
  gridLayout->addWidget(buildAxisPanel(QStringLiteral("Y 轴"), yPositionValueLabel_, yStateValueLabel_,
                                       yTargetSpinBox_, yStepSpinBox_, MotionAxis::Y),
                        0, 1);
  gridLayout->addWidget(buildAxisPanel(QStringLiteral("Z 轴"), zPositionValueLabel_, zStateValueLabel_,
                                       zTargetSpinBox_, zStepSpinBox_, MotionAxis::Z),
                        1, 0);
  gridLayout->addWidget(buildAxisPanel(QStringLiteral("R 轴"), rPositionValueLabel_, rStateValueLabel_,
                                       rTargetSpinBox_, rStepSpinBox_, MotionAxis::R),
                        1, 1);

  auto *logGroupBox = new QGroupBox(QStringLiteral("运控日志"), this);
  auto *logLayout = new QVBoxLayout(logGroupBox);
  logTextEdit_ = new QTextEdit(logGroupBox);
  logTextEdit_->setReadOnly(true);
  logLayout->addWidget(logTextEdit_);
  rootLayout->addWidget(logGroupBox, 1);

  connect(emergencyStopButton_, &QPushButton::clicked, this, [this] {
    controller_->emergencyStop();
    appendLog(QStringLiteral("已执行急停。"));
    refreshUi();
    emit motionStateChanged();
    emit motionLogGenerated(QStringLiteral("已执行急停。"));
  });

  connect(resetStopButton_, &QPushButton::clicked, this, [this] {
    controller_->resetEmergencyStop();
    appendLog(QStringLiteral("已复位急停。"));
    refreshUi();
    emit motionStateChanged();
    emit motionLogGenerated(QStringLiteral("已复位急停。"));
  });
}

void MotionControlDialog::refreshUi() {
  const bool enabled = controller_ != nullptr && !controller_->isStopped();

  for (const MotionAxis axis : {MotionAxis::X, MotionAxis::Y, MotionAxis::Z, MotionAxis::R}) {
    positionLabel(axis)->setText(formatAxisValue(axis, controller_->position(axis)));
    stateLabel(axis)->setText(enabled ? QStringLiteral("可操作") : QStringLiteral("急停锁定"));
    targetSpinBox(axis)->setEnabled(enabled);
    stepSpinBox(axis)->setEnabled(enabled);
  }

  emergencyStopButton_->setEnabled(enabled);
  resetStopButton_->setEnabled(!enabled);
}

void MotionControlDialog::appendLog(const QString &message) {
  const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
  logTextEdit_->append(QStringLiteral("[%1] %2").arg(timestamp, message));
}

void MotionControlDialog::moveAbsolute(const MotionAxis axis) {
  const double target = targetSpinBox(axis)->value();
  const bool ok = controller_ != nullptr && controller_->moveAbsolute(axis, target);
  appendLog(ok ? QStringLiteral("%1 轴绝对移动到 %2。").arg(axisName(axis)).arg(target, 0, 'f', 3)
               : QStringLiteral("%1 轴绝对移动失败。").arg(axisName(axis)));
  refreshUi();
  emit motionStateChanged();
  emit motionLogGenerated(ok ? QStringLiteral("%1 轴绝对移动到 %2。").arg(axisName(axis)).arg(target, 0, 'f', 3)
                             : QStringLiteral("%1 轴绝对移动失败。").arg(axisName(axis)));
}

void MotionControlDialog::jog(const MotionAxis axis, const double direction) {
  const double delta = stepSpinBox(axis)->value() * direction;
  const bool ok = controller_ != nullptr && controller_->moveRelative(axis, delta);
  appendLog(ok ? QStringLiteral("%1 轴相对移动 %2。").arg(axisName(axis)).arg(delta, 0, 'f', 3)
               : QStringLiteral("%1 轴点动失败。").arg(axisName(axis)));
  refreshUi();
  emit motionStateChanged();
  emit motionLogGenerated(ok ? QStringLiteral("%1 轴相对移动 %2。").arg(axisName(axis)).arg(delta, 0, 'f', 3)
                             : QStringLiteral("%1 轴点动失败。").arg(axisName(axis)));
}

void MotionControlDialog::home(const MotionAxis axis) {
  const bool ok = controller_ != nullptr && controller_->home(axis);
  appendLog(ok ? QStringLiteral("%1 轴已回零。").arg(axisName(axis))
               : QStringLiteral("%1 轴回零失败。").arg(axisName(axis)));
  refreshUi();
  emit motionStateChanged();
  emit motionLogGenerated(ok ? QStringLiteral("%1 轴已回零。").arg(axisName(axis))
                             : QStringLiteral("%1 轴回零失败。").arg(axisName(axis)));
}

QString MotionControlDialog::axisName(const MotionAxis axis) const {
  switch (axis) {
  case MotionAxis::X:
    return QStringLiteral("X");
  case MotionAxis::Y:
    return QStringLiteral("Y");
  case MotionAxis::Z:
    return QStringLiteral("Z");
  case MotionAxis::R:
    return QStringLiteral("R");
  }

  return QStringLiteral("Unknown");
}

QDoubleSpinBox *MotionControlDialog::targetSpinBox(const MotionAxis axis) const {
  switch (axis) {
  case MotionAxis::X:
    return xTargetSpinBox_;
  case MotionAxis::Y:
    return yTargetSpinBox_;
  case MotionAxis::Z:
    return zTargetSpinBox_;
  case MotionAxis::R:
    return rTargetSpinBox_;
  }

  return xTargetSpinBox_;
}

QDoubleSpinBox *MotionControlDialog::stepSpinBox(const MotionAxis axis) const {
  switch (axis) {
  case MotionAxis::X:
    return xStepSpinBox_;
  case MotionAxis::Y:
    return yStepSpinBox_;
  case MotionAxis::Z:
    return zStepSpinBox_;
  case MotionAxis::R:
    return rStepSpinBox_;
  }

  return xStepSpinBox_;
}

QLabel *MotionControlDialog::positionLabel(const MotionAxis axis) const {
  switch (axis) {
  case MotionAxis::X:
    return xPositionValueLabel_;
  case MotionAxis::Y:
    return yPositionValueLabel_;
  case MotionAxis::Z:
    return zPositionValueLabel_;
  case MotionAxis::R:
    return rPositionValueLabel_;
  }

  return xPositionValueLabel_;
}

QLabel *MotionControlDialog::stateLabel(const MotionAxis axis) const {
  switch (axis) {
  case MotionAxis::X:
    return xStateValueLabel_;
  case MotionAxis::Y:
    return yStateValueLabel_;
  case MotionAxis::Z:
    return zStateValueLabel_;
  case MotionAxis::R:
    return rStateValueLabel_;
  }

  return xStateValueLabel_;
}

#endif

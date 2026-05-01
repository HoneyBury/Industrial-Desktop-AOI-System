#include "ui/MotionControlDialog.h"

#ifdef AOI_HAS_QT_WIDGETS

#include "animation/AnimationWidget.h"
#include "transport/VirtualTransportController.h"

#include <array>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTextEdit>
#include <QTimer>
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

QString boardStateText(const BoardTransportState state) {
  switch (state) {
  case BoardTransportState::Idle:        return QStringLiteral("待进板");
  case BoardTransportState::Loading:     return QStringLiteral("进板中...");
  case BoardTransportState::BoardReady:  return QStringLiteral("到位");
  case BoardTransportState::Unloading:   return QStringLiteral("出板中...");
  }
  return QStringLiteral("未知");
}

constexpr const char *kActionBtnStyle =
    "QPushButton { background: #1e3a5f; color: #e2e8f0; border: 1px solid #334155; border-radius: 8px; "
    "  padding: 10px 18px; min-height: 36px; font-weight: 600; font-size: 13px; }"
    "QPushButton:hover { background: #2563eb; }"
    "QPushButton:pressed { background: #1d4ed8; }";

constexpr const char *kGroupBoxStyle =
    "QGroupBox { color: #e2e8f0; font-weight: 600; border: 1px solid #334155; border-radius: 10px; "
    "  margin-top: 14px; padding-top: 18px; }"
    "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; color: #cbd5e1; }";

constexpr const char *kValueLabelStyle =
    "font-weight: 700; font-size: 14px; color: #e2e8f0;";

constexpr const char *kHintLabelStyle =
    "color: #94a3b8; font-size: 11px;";

} // namespace

MotionControlDialog::MotionControlDialog(VirtualMotionSystem *motionSystem,
                                         QWidget *parent)
    : QDialog(parent),
      motionSystem_(motionSystem),
      motion_(motionSystem != nullptr ? &motionSystem->motionController() : nullptr),
      transport_(motionSystem != nullptr ? &motionSystem->transportController() : nullptr) {
  setWindowTitle(QStringLiteral("运动控制与虚拟设备调试"));
  resize(1100, 900);

  // 暗色主题对话框
  setStyleSheet(QStringLiteral(
      "QDialog { background: #0f172a; }"
      "QLabel { color: #e2e8f0; }"
      "QPushButton { color: #e2e8f0; }"
      "QTextEdit { background: #020617; color: #e2e8f0; border: 1px solid #334155; border-radius: 8px; "
      "  font-family: 'SF Mono', 'Menlo', monospace; font-size: 12px; }"
      "QDoubleSpinBox { background: #1e293b; color: #e2e8f0; border: 1px solid #334155; border-radius: 6px; "
      "  padding: 4px 8px; min-height: 28px; }"
      "QDoubleSpinBox:focus { border-color: #3b82f6; }"));

  buildUi();

  animationWidget_->startAnimation();
  refreshTimer_ = new QTimer(this);
  refreshTimer_->setInterval(100);
  connect(refreshTimer_, &QTimer::timeout, this, &MotionControlDialog::refreshUi);
  refreshTimer_->start();

  refreshUi();
  appendLog(QStringLiteral("运控面板已打开，动画已启动。"));
}

void MotionControlDialog::buildUi() {
  auto *outerLayout = new QVBoxLayout(this);
  outerLayout->setContentsMargins(0, 0, 0, 0);
  outerLayout->setSpacing(0);

  // ── 滚动区域 ──
  scrollArea_ = new QScrollArea(this);
  scrollArea_->setWidgetResizable(true);
  scrollArea_->setFrameShape(QFrame::NoFrame);
  scrollArea_->setStyleSheet(QStringLiteral(
      "QScrollArea { background: #0f172a; border: none; }"
      "QScrollBar:vertical { background: #1e293b; width: 8px; border-radius: 4px; }"
      "QScrollBar::handle:vertical { background: #475569; border-radius: 4px; min-height: 32px; }"
      "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"));

  auto *contentWidget = new QWidget(scrollArea_);
  contentWidget->setStyleSheet(QStringLiteral("background: #0f172a;"));
  scrollArea_->setWidget(contentWidget);

  auto *rootLayout = new QVBoxLayout(contentWidget);
  rootLayout->setContentsMargins(12, 12, 12, 12);
  rootLayout->setSpacing(10);

  outerLayout->addWidget(scrollArea_);

  // ── 缩放工具栏 ──
  auto *zoomBar = new QHBoxLayout;
  zoomBar->setSpacing(8);

  auto *zoomInBtn = new QPushButton(QStringLiteral("放大"), contentWidget);
  zoomInBtn->setStyleSheet(kActionBtnStyle);
  auto *zoomOutBtn = new QPushButton(QStringLiteral("缩小"), contentWidget);
  zoomOutBtn->setStyleSheet(kActionBtnStyle);
  auto *fitBtn = new QPushButton(QStringLiteral("适应"), contentWidget);
  fitBtn->setStyleSheet(kActionBtnStyle);

  zoomValueLabel_ = new QLabel(QStringLiteral("100%"), contentWidget);
  zoomValueLabel_->setStyleSheet(QStringLiteral("font-weight: 700; font-size: 14px; color: #facc15; min-width: 56px;"));

  zoomBar->addWidget(new QLabel(QStringLiteral("设备动画"), contentWidget));
  zoomBar->addStretch();
  zoomBar->addWidget(zoomOutBtn);
  zoomBar->addWidget(zoomValueLabel_);
  zoomBar->addWidget(zoomInBtn);
  zoomBar->addWidget(fitBtn);

  rootLayout->addLayout(zoomBar);

  // ── 设备动画区 ──
  animationWidget_ = new AnimationWidget(contentWidget);
  animationWidget_->setMinimumHeight(420);
  animationWidget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  animationWidget_->setMotionController(motion_);
  animationWidget_->setTransportController(transport_);

  connect(zoomInBtn, &QPushButton::clicked, animationWidget_, &AnimationWidget::zoomIn);
  connect(zoomOutBtn, &QPushButton::clicked, animationWidget_, &AnimationWidget::zoomOut);
  connect(fitBtn, &QPushButton::clicked, animationWidget_, &AnimationWidget::fitToWindow);
  connect(animationWidget_, &AnimationWidget::zoomChanged, this, [this](int pct) {
    zoomValueLabel_->setText(QStringLiteral("%1%").arg(pct));
  });

  rootLayout->addWidget(animationWidget_, 1);

  auto *boardGroup = new QGroupBox(QStringLiteral("轨道调宽 / 板尺寸"), contentWidget);
  boardGroup->setStyleSheet(kGroupBoxStyle);
  auto *boardLayout = new QVBoxLayout(boardGroup);
  boardLayout->setSpacing(10);

  auto *boardForm = new QFormLayout;
  boardForm->setSpacing(8);
  boardLengthSpinBox_ = buildTargetSpinBox(MotionAxis::CameraX);
  boardLengthSpinBox_->setRange(50.0, 1500.0);
  boardLengthSpinBox_->setValue(programBoardDefinition_.boardLengthMm);
  boardLengthSpinBox_->setSuffix(QStringLiteral(" mm"));
  boardWidthSpinBox_ = buildTargetSpinBox(MotionAxis::CameraY);
  boardWidthSpinBox_->setRange(10.0, 1000.0);
  boardWidthSpinBox_->setValue(programBoardDefinition_.boardWidthMm);
  boardWidthSpinBox_->setSuffix(QStringLiteral(" mm"));
  railWidthSpinBox_ = buildTargetSpinBox(MotionAxis::Stopper);
  railWidthSpinBox_->setRange(5.0, 300.0);
  railWidthSpinBox_->setValue(programBoardDefinition_.railWidthMm);
  railWidthSpinBox_->setSuffix(QStringLiteral(" mm"));
  boardForm->addRow(QStringLiteral("板长"), boardLengthSpinBox_);
  boardForm->addRow(QStringLiteral("板宽"), boardWidthSpinBox_);
  boardForm->addRow(QStringLiteral("轨道宽度"), railWidthSpinBox_);
  boardLayout->addLayout(boardForm);

  auto *boardButtonRow = new QHBoxLayout;
  auto *restoreProgramBoardBtn = new QPushButton(QStringLiteral("按当前程序尺寸"), contentWidget);
  restoreProgramBoardBtn->setStyleSheet(kActionBtnStyle);
  boardButtonRow->addWidget(restoreProgramBoardBtn);
  boardButtonRow->addStretch();
  boardLayout->addLayout(boardButtonRow);

  boardDefinitionHintLabel_ = new QLabel(boardGroup);
  boardDefinitionHintLabel_->setWordWrap(true);
  boardDefinitionHintLabel_->setStyleSheet(QLatin1StringView(kHintLabelStyle));
  boardLayout->addWidget(boardDefinitionHintLabel_);

  rootLayout->addWidget(boardGroup);

  // ── IO 控制面板 ──
  auto *ioGroup = new QGroupBox(QStringLiteral("IO 控制 — 进板/出板/挡板"), contentWidget);
  ioGroup->setStyleSheet(kGroupBoxStyle);
  auto *ioLayout = new QVBoxLayout(ioGroup);
  ioLayout->setSpacing(10);

  // IO 按钮行
  auto *ioBtnLayout = new QHBoxLayout;
  ioBtnLayout->setSpacing(10);

  auto *stopperUpBtn = new QPushButton(QStringLiteral("挡板上升"), contentWidget);
  stopperUpBtn->setStyleSheet(kActionBtnStyle);

  auto *stopperDownBtn = new QPushButton(QStringLiteral("挡板下降"), contentWidget);
  stopperDownBtn->setStyleSheet(kActionBtnStyle);

  auto *loadBoardBtn = new QPushButton(QStringLiteral("进板"), contentWidget);
  loadBoardBtn->setStyleSheet(QStringLiteral(
      "QPushButton { background: #166534; color: #f8fafc; border: 1px solid #22c55e; border-radius: 8px; "
      "  padding: 10px 24px; min-height: 36px; font-weight: 700; font-size: 14px; }"
      "QPushButton:hover { background: #22c55e; }"
      "QPushButton:pressed { background: #15803d; }"));

  auto *unloadBoardBtn = new QPushButton(QStringLiteral("出板"), contentWidget);
  unloadBoardBtn->setStyleSheet(QStringLiteral(
      "QPushButton { background: #991b1b; color: #f8fafc; border: 1px solid #ef4444; border-radius: 8px; "
      "  padding: 10px 24px; min-height: 36px; font-weight: 700; font-size: 14px; }"
      "QPushButton:hover { background: #ef4444; }"
      "QPushButton:pressed { background: #7f1d1d; }"));

  auto *resetBoardBtn = new QPushButton(QStringLiteral("复位信号"), contentWidget);
  resetBoardBtn->setStyleSheet(kActionBtnStyle);

  ioBtnLayout->addWidget(loadBoardBtn);
  ioBtnLayout->addWidget(unloadBoardBtn);
  ioBtnLayout->addWidget(stopperUpBtn);
  ioBtnLayout->addWidget(stopperDownBtn);
  ioBtnLayout->addStretch();
  ioBtnLayout->addWidget(resetBoardBtn);

  ioLayout->addLayout(ioBtnLayout);

  // IO 状态指示行
  auto *ioStatusLayout = new QHBoxLayout;
  ioStatusLayout->setSpacing(32);

  auto makeIoLabel = [contentWidget](const QString &title) {
    auto *lbl = new QLabel(title, contentWidget);
    lbl->setStyleSheet(QLatin1StringView(kHintLabelStyle));
    return lbl;
  };

  boardStateValueLabel_ = new QLabel(QStringLiteral("待进板"), contentWidget);
  boardStateValueLabel_->setStyleSheet(QLatin1StringView(kValueLabelStyle));

  boardPosValueLabel_ = new QLabel(QStringLiteral("0.0 mm"), contentWidget);
  boardPosValueLabel_->setStyleSheet(QLatin1StringView(kValueLabelStyle));

  stopperStateValueLabel_ = new QLabel(QStringLiteral("下降"), contentWidget);
  stopperStateValueLabel_->setStyleSheet(QLatin1StringView(kValueLabelStyle));

  conveyorSpeedValueLabel_ = new QLabel(QStringLiteral("0.0 mm/s"), contentWidget);
  conveyorSpeedValueLabel_->setStyleSheet(QLatin1StringView(kValueLabelStyle));

  auto addIoStatus = [&](const QString &title, QLabel *value) {
    auto *vbox = new QVBoxLayout;
    vbox->setSpacing(2);
    vbox->addWidget(makeIoLabel(title));
    vbox->addWidget(value);
    ioStatusLayout->addLayout(vbox);
  };

  addIoStatus(QStringLiteral("板状态"), boardStateValueLabel_);
  addIoStatus(QStringLiteral("板位置"), boardPosValueLabel_);
  addIoStatus(QStringLiteral("挡板状态"), stopperStateValueLabel_);
  addIoStatus(QStringLiteral("传送带速度"), conveyorSpeedValueLabel_);
  ioStatusLayout->addStretch();

  ioLayout->addLayout(ioStatusLayout);
  rootLayout->addWidget(ioGroup);

  // ── 轴控制面板 (2x2 grid) ──
  auto *axisGroup = new QGroupBox(QStringLiteral("轴控制"), contentWidget);
  axisGroup->setStyleSheet(kGroupBoxStyle);
  auto *gridLayout = new QGridLayout(axisGroup);
  gridLayout->setSpacing(10);

  const auto buildAxisPanel = [this, contentWidget](const QString &title,
                                                    QLabel *&positionLabel,
                                                    QLabel *&stateLabel,
                                                    QDoubleSpinBox *&targetSpinBox,
                                                    QDoubleSpinBox *&stepSpinBox,
                                                    MotionAxis axis) {
    auto *groupBox = new QGroupBox(title, contentWidget);
    groupBox->setStyleSheet(QStringLiteral(
        "QGroupBox { color: #cbd5e1; font-weight: 600; border: 1px solid #1e293b; border-radius: 8px; "
        "  margin-top: 12px; padding-top: 16px; background: #0a0f1a; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"));
    auto *layout = new QGridLayout(groupBox);
    layout->setSpacing(6);

    positionLabel = new QLabel(QStringLiteral("0.000"), groupBox);
    positionLabel->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 700; color: #facc15;"));
    stateLabel = new QLabel(QStringLiteral("可操作"), groupBox);
    stateLabel->setStyleSheet(QStringLiteral("color: #22c55e; font-weight: 600;"));

    targetSpinBox = buildTargetSpinBox(axis);
    stepSpinBox = buildStepSpinBox(axis);

    auto *absoluteButton = new QPushButton(QStringLiteral("绝对移动"), groupBox);
    absoluteButton->setStyleSheet(kActionBtnStyle);
    auto *negativeButton = new QPushButton(QStringLiteral(" 负向 "), groupBox);
    negativeButton->setStyleSheet(kActionBtnStyle);
    auto *positiveButton = new QPushButton(QStringLiteral(" 正向 "), groupBox);
    positiveButton->setStyleSheet(kActionBtnStyle);
    auto *homeButton = new QPushButton(QStringLiteral("回零"), groupBox);
    homeButton->setStyleSheet(kActionBtnStyle);

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

  gridLayout->addWidget(buildAxisPanel(QStringLiteral("相机 X 轴"), xPositionValueLabel_, xStateValueLabel_,
                                       xTargetSpinBox_, xStepSpinBox_, MotionAxis::CameraX),
                        0, 0);
  gridLayout->addWidget(buildAxisPanel(QStringLiteral("相机 Y 轴"), yPositionValueLabel_, yStateValueLabel_,
                                       yTargetSpinBox_, yStepSpinBox_, MotionAxis::CameraY),
                        0, 1);
  gridLayout->addWidget(buildAxisPanel(QStringLiteral("Z 轴"), zPositionValueLabel_, zStateValueLabel_,
                                       zTargetSpinBox_, zStepSpinBox_, MotionAxis::Z),
                        1, 0);
  gridLayout->addWidget(buildAxisPanel(QStringLiteral("R 轴"), rPositionValueLabel_, rStateValueLabel_,
                                       rTargetSpinBox_, rStepSpinBox_, MotionAxis::R),
                        1, 1);

  rootLayout->addWidget(axisGroup);

  // ── 全局控制 ──
  auto *globalGroup = new QGroupBox(QStringLiteral("全局安全"), contentWidget);
  globalGroup->setStyleSheet(kGroupBoxStyle);
  auto *globalLayout = new QHBoxLayout(globalGroup);
  emergencyStopButton_ = new QPushButton(QStringLiteral("急停"), contentWidget);
  emergencyStopButton_->setStyleSheet(QStringLiteral(
      "QPushButton { background: #b42318; color: white; font-weight: 700; "
      "padding: 10px 24px; border-radius: 8px; font-size: 14px; }"
      "QPushButton:hover { background: #dc2626; }"
      "QPushButton:disabled { background: #1e293b; color: #475569; }"));
  resetStopButton_ = new QPushButton(QStringLiteral("复位急停"), contentWidget);
  resetStopButton_->setStyleSheet(QStringLiteral(
      "QPushButton { background: #175cd3; color: white; font-weight: 700; "
      "padding: 10px 24px; border-radius: 8px; font-size: 14px; }"
      "QPushButton:hover { background: #2563eb; }"
      "QPushButton:disabled { background: #1e293b; color: #475569; }"));
  globalLayout->addWidget(new QLabel(QStringLiteral("急停将停止所有轴运动并触发报警灯。"), contentWidget));
  globalLayout->addStretch();
  globalLayout->addWidget(emergencyStopButton_);
  globalLayout->addWidget(resetStopButton_);
  rootLayout->addWidget(globalGroup);

  // ── 运控日志 ──
  auto *logGroup = new QGroupBox(QStringLiteral("运控日志"), contentWidget);
  logGroup->setStyleSheet(kGroupBoxStyle);
  auto *logLayout = new QVBoxLayout(logGroup);
  logTextEdit_ = new QTextEdit(logGroup);
  logTextEdit_->setReadOnly(true);
  logTextEdit_->setPlaceholderText(QStringLiteral("运控操作日志将在此显示..."));
  logTextEdit_->setMinimumHeight(140);
  logTextEdit_->setMaximumHeight(240);
  logLayout->addWidget(logTextEdit_);
  rootLayout->addWidget(logGroup);

  // ── 信号连接 ──

  connect(stopperUpBtn, &QPushButton::clicked, this, &MotionControlDialog::raiseStopper);
  connect(stopperDownBtn, &QPushButton::clicked, this, &MotionControlDialog::lowerStopper);
  connect(loadBoardBtn, &QPushButton::clicked, this, &MotionControlDialog::loadBoard);
  connect(unloadBoardBtn, &QPushButton::clicked, this, &MotionControlDialog::unloadBoard);
  connect(resetBoardBtn, &QPushButton::clicked, this, &MotionControlDialog::resetBoard);
  connect(restoreProgramBoardBtn, &QPushButton::clicked, this, &MotionControlDialog::restoreProgramBoardDefinition);
  connect(boardLengthSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged),
          this, [this](double) { handleBoardDefinitionInputsChanged(); });
  connect(boardWidthSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged),
          this, [this](double) { handleBoardDefinitionInputsChanged(); });
  connect(railWidthSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged),
          this, [this](double) { handleBoardDefinitionInputsChanged(); });

  connect(emergencyStopButton_, &QPushButton::clicked, this, [this] {
    motion_->emergencyStop();
    animationWidget_->setAlarm(true);
    appendLog(QStringLiteral("已执行急停。"));
    refreshUi();
    emit motionStateChanged();
    emit motionLogGenerated(QStringLiteral("已执行急停。"));
  });

  connect(resetStopButton_, &QPushButton::clicked, this, [this] {
    motion_->resetEmergencyStop();
    animationWidget_->setAlarm(false);
    appendLog(QStringLiteral("已复位急停。"));
    refreshUi();
    emit motionStateChanged();
    emit motionLogGenerated(QStringLiteral("已复位急停。"));
  });

  applyBoardDefinition(programBoardDefinition_, true, false);
}

void MotionControlDialog::refreshUi() {
  const bool enabled = motion_ != nullptr && !motion_->isStopped();

  const std::array axes = {MotionAxis::CameraX, MotionAxis::CameraY, MotionAxis::Z, MotionAxis::R};
  for (const MotionAxis axis : axes) {
    positionLabel(axis)->setText(formatAxisValue(axis, motion_->position(axis)));
    stateLabel(axis)->setText(enabled ? QStringLiteral("可操作") : QStringLiteral("急停锁定"));
    targetSpinBox(axis)->setEnabled(enabled);
    stepSpinBox(axis)->setEnabled(enabled);
  }

  emergencyStopButton_->setEnabled(enabled);
  resetStopButton_->setEnabled(!enabled);

  // IO 状态（动画场景从运动轴自动同步挡板/板位置，这里只更新文本）
  if (transport_ != nullptr) {
    const auto state = transport_->state();
    boardStateValueLabel_->setText(boardStateText(state));
    boardPosValueLabel_->setText(QStringLiteral("%1 mm").arg(transport_->boardPosition(), 0, 'f', 1));
    stopperStateValueLabel_->setText(transport_->isStopperRaised() ? QStringLiteral("上升") : QStringLiteral("下降"));
    conveyorSpeedValueLabel_->setText(QStringLiteral("%1 mm/s").arg(transport_->conveyorSpeed(), 0, 'f', 1));

    // 只同步板可见性和状态文本（挡板/板位置由场景 updateFromMotion 处理）
    animationWidget_->showBoard(state != BoardTransportState::Idle);
    animationWidget_->setDeviceStateText(boardStateText(state));
  }
}

void MotionControlDialog::appendLog(const QString &message) {
  const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
  logTextEdit_->append(QStringLiteral("[%1] %2").arg(timestamp, message));
}

// ── IO 操作 ──

void MotionControlDialog::raiseStopper() {
  if (motionSystem_ != nullptr && motionSystem_->raiseStopper()) {
    appendLog(QStringLiteral("挡板上升。"));
  } else {
    appendLog(QStringLiteral("挡板上升失败。"));
  }
  refreshUi();
}

void MotionControlDialog::lowerStopper() {
  if (motionSystem_ != nullptr && motionSystem_->lowerStopper()) {
    appendLog(QStringLiteral("挡板下降。"));
  } else {
    appendLog(QStringLiteral("挡板下降失败。"));
  }
  refreshUi();
}

void MotionControlDialog::loadBoard() {
  if (motionSystem_ != nullptr && motionSystem_->loadBoard()) {
    const bool ready = motionSystem_->waitForBoardReady(8.0);
    appendLog(ready ? QStringLiteral("进板完成，板已到位。")
                    : QStringLiteral("进板已启动，但等待到位超时。"));
  } else {
    appendLog(QStringLiteral("进板失败：%1").arg(
        transport_ != nullptr ? QString::fromStdString(transport_->lastSignalMessage())
                              : QStringLiteral("无运输控制器")));
  }
  refreshUi();
}

void MotionControlDialog::unloadBoard() {
  if (motionSystem_ != nullptr && motionSystem_->unloadBoard()) {
    const bool idle = motionSystem_->waitForTransportIdle(8.0);
    appendLog(idle ? QStringLiteral("出板完成，运输机构已回到待机。")
                   : QStringLiteral("出板已启动，但等待待机超时。"));
  } else {
    appendLog(QStringLiteral("出板失败：%1").arg(
        transport_ != nullptr ? QString::fromStdString(transport_->lastSignalMessage())
                              : QStringLiteral("无运输控制器")));
  }
  refreshUi();
}

void MotionControlDialog::resetBoard() {
  if (motionSystem_ != nullptr) {
    motionSystem_->resetBoardTransport();
    appendLog(QStringLiteral("已复位板到位信号。"));
  }
  refreshUi();
}

void MotionControlDialog::restoreProgramBoardDefinition() {
  boardDefinitionManualOverride_ = false;
  applyBoardDefinition(programBoardDefinition_, true, false);
  appendLog(QStringLiteral("已恢复到当前程序的板尺寸与轨道宽度。"));
}

void MotionControlDialog::handleBoardDefinitionInputsChanged() {
  applyBoardDefinition(boardDefinitionFromInputs(), false, true);
}

// ── 轴操作 ──

void MotionControlDialog::moveAbsolute(const MotionAxis axis) {
  const double target = targetSpinBox(axis)->value();
  const bool ok = motionSystem_ != nullptr ? motionSystem_->moveAxis(axis, target)
                                           : (motion_ != nullptr && motion_->moveAbsolute(axis, target));
  appendLog(ok ? QStringLiteral("%1 轴绝对移动到 %2。").arg(axisName(axis)).arg(target, 0, 'f', 3)
               : QStringLiteral("%1 轴绝对移动失败。").arg(axisName(axis)));
  refreshUi();
  emit motionStateChanged();
  emit motionLogGenerated(ok ? QStringLiteral("%1 轴绝对移动到 %2。").arg(axisName(axis)).arg(target, 0, 'f', 3)
                             : QStringLiteral("%1 轴绝对移动失败。").arg(axisName(axis)));
}

void MotionControlDialog::jog(const MotionAxis axis, const double direction) {
  const double delta = stepSpinBox(axis)->value() * direction;
  const bool ok = motionSystem_ != nullptr ? motionSystem_->jogAxis(axis, delta)
                                           : (motion_ != nullptr && motion_->moveRelative(axis, delta));
  appendLog(ok ? QStringLiteral("%1 轴相对移动 %2。").arg(axisName(axis)).arg(delta, 0, 'f', 3)
               : QStringLiteral("%1 轴点动失败。").arg(axisName(axis)));
  refreshUi();
  emit motionStateChanged();
  emit motionLogGenerated(ok ? QStringLiteral("%1 轴相对移动 %2。").arg(axisName(axis)).arg(delta, 0, 'f', 3)
                             : QStringLiteral("%1 轴点动失败。").arg(axisName(axis)));
}

void MotionControlDialog::home(const MotionAxis axis) {
  const bool ok = motionSystem_ != nullptr ? motionSystem_->homeAxis(axis)
                                           : (motion_ != nullptr && motion_->home(axis));
  appendLog(ok ? QStringLiteral("%1 轴已回零。").arg(axisName(axis))
               : QStringLiteral("%1 轴回零失败。").arg(axisName(axis)));
  refreshUi();
  emit motionStateChanged();
  emit motionLogGenerated(ok ? QStringLiteral("%1 轴已回零。").arg(axisName(axis))
                             : QStringLiteral("%1 轴回零失败。").arg(axisName(axis)));
}

QString MotionControlDialog::axisName(const MotionAxis axis) const {
  switch (axis) {
  case MotionAxis::Conveyor: return QStringLiteral("传送带");
  case MotionAxis::Stopper:  return QStringLiteral("挡板");
  case MotionAxis::CameraX:  return QStringLiteral("相机X");
  case MotionAxis::CameraY:  return QStringLiteral("相机Y");
  case MotionAxis::LaserX:   return QStringLiteral("激光X");
  case MotionAxis::LaserY:   return QStringLiteral("激光Y");
  case MotionAxis::Z:        return QStringLiteral("Z");
  case MotionAxis::R:        return QStringLiteral("R");
  }
  return QStringLiteral("Unknown");
}

QDoubleSpinBox *MotionControlDialog::targetSpinBox(const MotionAxis axis) const {
  switch (axis) {
  case MotionAxis::CameraX: return xTargetSpinBox_;
  case MotionAxis::CameraY: return yTargetSpinBox_;
  case MotionAxis::Z:       return zTargetSpinBox_;
  case MotionAxis::R:       return rTargetSpinBox_;
  case MotionAxis::Conveyor:
  case MotionAxis::Stopper:
  case MotionAxis::LaserX:
  case MotionAxis::LaserY:  break;
  }
  return xTargetSpinBox_;
}

QDoubleSpinBox *MotionControlDialog::stepSpinBox(const MotionAxis axis) const {
  switch (axis) {
  case MotionAxis::CameraX: return xStepSpinBox_;
  case MotionAxis::CameraY: return yStepSpinBox_;
  case MotionAxis::Z:       return zStepSpinBox_;
  case MotionAxis::R:       return rStepSpinBox_;
  case MotionAxis::Conveyor:
  case MotionAxis::Stopper:
  case MotionAxis::LaserX:
  case MotionAxis::LaserY:  break;
  }
  return xStepSpinBox_;
}

QLabel *MotionControlDialog::positionLabel(const MotionAxis axis) const {
  switch (axis) {
  case MotionAxis::CameraX: return xPositionValueLabel_;
  case MotionAxis::CameraY: return yPositionValueLabel_;
  case MotionAxis::Z:       return zPositionValueLabel_;
  case MotionAxis::R:       return rPositionValueLabel_;
  default:                  break;
  }
  return xPositionValueLabel_;
}

QLabel *MotionControlDialog::stateLabel(const MotionAxis axis) const {
  switch (axis) {
  case MotionAxis::CameraX: return xStateValueLabel_;
  case MotionAxis::CameraY: return yStateValueLabel_;
  case MotionAxis::Z:       return zStateValueLabel_;
  case MotionAxis::R:       return rStateValueLabel_;
  default:                  break;
  }
  return xStateValueLabel_;
}

void MotionControlDialog::setProgramBoardDefinition(const BoardDefinition &definition) {
  programBoardDefinition_ = definition;
  if (!boardDefinitionManualOverride_) {
    applyBoardDefinition(programBoardDefinition_, true, false);
  } else {
    updateBoardDefinitionHint();
  }
}

BoardDefinition MotionControlDialog::boardDefinitionFromInputs() const {
  return BoardDefinition {
      boardLengthSpinBox_ != nullptr ? boardLengthSpinBox_->value() : programBoardDefinition_.boardLengthMm,
      boardWidthSpinBox_ != nullptr ? boardWidthSpinBox_->value() : programBoardDefinition_.boardWidthMm,
      railWidthSpinBox_ != nullptr ? railWidthSpinBox_->value() : programBoardDefinition_.railWidthMm,
  };
}

void MotionControlDialog::applyBoardDefinition(const BoardDefinition &definition,
                                               const bool syncInputs,
                                               const bool markManualOverride) {
  boardDefinitionManualOverride_ = markManualOverride;
  if (syncInputs) {
    const QSignalBlocker blockerLength(boardLengthSpinBox_);
    const QSignalBlocker blockerWidth(boardWidthSpinBox_);
    const QSignalBlocker blockerRail(railWidthSpinBox_);
    boardLengthSpinBox_->setValue(definition.boardLengthMm);
    boardWidthSpinBox_->setValue(definition.boardWidthMm);
    railWidthSpinBox_->setValue(definition.railWidthMm);
  }

  if (animationWidget_ != nullptr) {
    animationWidget_->setBoardDefinition(definition);
  }
  updateBoardDefinitionHint();
}

void MotionControlDialog::updateBoardDefinitionHint() {
  if (boardDefinitionHintLabel_ == nullptr) return;

  const BoardDefinition activeDefinition = boardDefinitionFromInputs();
  const QString activeSource = boardDefinitionManualOverride_ ? QStringLiteral("手动调宽预览中")
                                                              : QStringLiteral("已跟随当前程序");
  boardDefinitionHintLabel_->setText(
      QStringLiteral("%1：板长=%2 mm，板宽=%3 mm，轨道=%4 mm。程序基准：长=%5 / 宽=%6 / 轨=%7 mm")
          .arg(activeSource)
          .arg(activeDefinition.boardLengthMm, 0, 'f', 1)
          .arg(activeDefinition.boardWidthMm, 0, 'f', 1)
          .arg(activeDefinition.railWidthMm, 0, 'f', 1)
          .arg(programBoardDefinition_.boardLengthMm, 0, 'f', 1)
          .arg(programBoardDefinition_.boardWidthMm, 0, 'f', 1)
          .arg(programBoardDefinition_.railWidthMm, 0, 'f', 1));
}

#endif

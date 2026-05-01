#include "ui/RunModeWidget.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QBoxLayout>
#include <QChart>
#include <QChartView>
#include <QDateTime>
#include <QFrame>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineSeries>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QTextEdit>
#include <QTimer>
#include <QValueAxis>
#include <QVBoxLayout>

namespace {

constexpr int kPreviewWidth = 1280;
constexpr int kPreviewHeight = 720;

QImage buildPlaceholderPreview() {
  QImage image(kPreviewWidth, kPreviewHeight, QImage::Format_ARGB32_Premultiplied);
  image.fill(QColor("#020617"));

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  painter.setPen(QPen(QColor("#1e293b"), 1));
  for (int x = 0; x <= image.width(); x += 60) {
    painter.drawLine(x, 0, x, image.height());
  }
  for (int y = 0; y <= image.height(); y += 60) {
    painter.drawLine(0, y, image.width(), y);
  }

  painter.setPen(QPen(QColor("#0ea5e9"), 2, Qt::DashLine));
  painter.drawRoundedRect(QRectF(120, 60, image.width() - 240, image.height() - 120), 12, 12);

  painter.setPen(QColor("#94a3b8"));
  QFont font = painter.font();
  font.setPointSize(18);
  painter.setFont(font);
  painter.drawText(image.rect(), Qt::AlignCenter, QString::fromUtf8("等待启动运行\n相机预览将在运行后显示"));

  return image;
}

} // namespace

RunModeWidget::RunModeWidget(QWidget *parent) : QFrame(parent) {
  setStyleSheet(QStringLiteral(
      "RunModeWidget { background: #030712; }"
      "QGroupBox { color: #e2e8f0; font-weight: 600; border: 1px solid #334155; border-radius: 10px; "
      "  margin-top: 14px; padding-top: 18px; }"
      "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; color: #cbd5e1; }"
      "QLabel { color: #e2e8f0; }"
      "QPushButton { background: #1e3a5f; color: #e2e8f0; border: 1px solid #334155; border-radius: 8px; "
      "  padding: 8px 18px; min-height: 32px; font-weight: 600; }"
      "QPushButton:hover { background: #2563eb; }"
      "QPushButton:pressed { background: #1d4ed8; }"
      "QPushButton:disabled { background: #1e293b; color: #475569; }"
      "QProgressBar { background: #1e293b; border: 1px solid #334155; border-radius: 6px; "
      "  text-align: center; color: #e2e8f0; min-height: 22px; }"
      "QProgressBar::chunk { background: #2563eb; border-radius: 5px; }"
      "QTextEdit { background: #0f172a; color: #e2e8f0; border: 1px solid #334155; border-radius: 8px; "
      "  font-family: 'SF Mono', 'Menlo', 'Courier New', monospace; font-size: 12px; }"));

  previewTimer_ = new QTimer(this);
  previewTimer_->setInterval(100);
  connect(previewTimer_, &QTimer::timeout, this, &RunModeWidget::refreshPreview);

  buildUi();
  refreshDashboard();
}

RunModeWidget::~RunModeWidget() {
  previewTimer_->stop();
}

void RunModeWidget::setFrameProvider(FrameProvider provider) { frameProvider_ = std::move(provider); }

void RunModeWidget::setBoardCountProvider(BoardCountProvider provider) { boardCountProvider_ = std::move(provider); }

void RunModeWidget::setStatusTextProvider(StatusTextProvider provider) {
  statusTextProvider_ = std::move(provider);
  refreshPreviewOverlay();
}

void RunModeWidget::setTotalBoards(const int count) {
  totalBoards_ = count;
  refreshDashboard();
}

void RunModeWidget::fitPreviewContent() {
  if (previewView_ == nullptr || previewScene_ == nullptr) {
    return;
  }

  previewView_->fitInView(previewScene_->sceneRect(), Qt::KeepAspectRatio);
}

void RunModeWidget::appendProductionLog(const QString &message) {
  if (productionLogEdit_ == nullptr) {
    return;
  }

  const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
  productionLogEdit_->append(QStringLiteral("[%1] %2").arg(timestamp, message));

  auto *scrollBar = productionLogEdit_->verticalScrollBar();
  if (scrollBar != nullptr) {
    scrollBar->setValue(scrollBar->maximum());
  }

  refreshPreviewOverlay();
}

void RunModeWidget::updateStepProgress(const QString &stepName, const int stepIndex, const int totalSteps) {
  currentStepIndex_ = stepIndex;
  totalSteps_ = totalSteps;

  if (stepProgressLabel_ != nullptr) {
    stepProgressLabel_->setText(stepName);
  }

  if (stepProgressBar_ != nullptr) {
    stepProgressBar_->setRange(0, totalSteps);
    stepProgressBar_->setValue(stepIndex);
  }

  if (workflowStateLabel_ != nullptr) {
    workflowStateLabel_->setText(stepName);
  }

  refreshPreviewOverlay();
}

void RunModeWidget::recordBoardResult(const bool ok) {
  if (ok) {
    ++okCount_;
  } else {
    ++ngCount_;
  }

  ++currentBoardIndex_;
  refreshDashboard();

  // Update yield trend chart
  if (yieldSeries_ != nullptr) {
    const int total = okCount_ + ngCount_;
    const double yield = total > 0 ? 100.0 * static_cast<double>(okCount_) / static_cast<double>(total) : 0.0;
    yieldSeries_->append(currentBoardIndex_, yield);
    ++yieldDataPointCount_;

    // Auto-adjust X axis range as data grows
    if (yieldChart_ != nullptr) {
      const auto axes = yieldChart_->axes(Qt::Horizontal);
      if (!axes.isEmpty()) {
        auto *axisX = qobject_cast<QValueAxis *>(axes.first());
        if (axisX != nullptr) {
          const double maxX = static_cast<double>(qMax(10, currentBoardIndex_ + 2));
          axisX->setRange(0, maxX);
        }
      }
    }
  }

  const QString resultText = ok ? QStringLiteral("OK") : QStringLiteral("NG");
  appendProductionLog(QStringLiteral("板 #%1 检测完成，结果：%2").arg(currentBoardIndex_).arg(resultText));
  refreshPreviewOverlay();
}

int RunModeWidget::okCount() const { return okCount_; }

int RunModeWidget::ngCount() const { return ngCount_; }

int RunModeWidget::currentBoardIndex() const { return currentBoardIndex_; }

void RunModeWidget::buildUi() {
  auto *rootLayout = new QHBoxLayout(this);
  rootLayout->setContentsMargins(8, 8, 8, 8);
  rootLayout->setSpacing(10);

  auto *splitter = new QSplitter(Qt::Horizontal, this);
  splitter->setChildrenCollapsible(false);

  auto *leftWidget = new QWidget(splitter);
  auto *leftLayout = new QVBoxLayout(leftWidget);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->setSpacing(8);
  buildLockedPreview(leftLayout);

  auto *rightWidget = new QWidget(splitter);
  auto *rightLayout = new QVBoxLayout(rightWidget);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(8);

  dashboardScrollArea_ = new QScrollArea(rightWidget);
  dashboardScrollArea_->setWidgetResizable(true);
  dashboardScrollArea_->setFrameShape(QFrame::NoFrame);
  dashboardScrollArea_->setStyleSheet(QStringLiteral(
      "QScrollArea { background: transparent; border: none; }"
      "QScrollBar:vertical { background: #0f172a; width: 8px; border-radius: 4px; }"
      "QScrollBar::handle:vertical { background: #334155; border-radius: 4px; min-height: 24px; }"
      "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"));
  auto *dashboardContent = new QWidget(dashboardScrollArea_);
  dashboardContent->setStyleSheet(QStringLiteral("background: transparent;"));
  auto *dashboardContentLayout = new QVBoxLayout(dashboardContent);
  dashboardContentLayout->setContentsMargins(0, 0, 0, 0);
  dashboardContentLayout->setSpacing(8);
  buildDashboard(dashboardContentLayout);
  dashboardContentLayout->addStretch();
  dashboardScrollArea_->setWidget(dashboardContent);
  rightLayout->addWidget(dashboardScrollArea_);

  splitter->addWidget(leftWidget);
  splitter->addWidget(rightWidget);
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 2);
  splitter->setSizes({900, 420});
  splitter->setHandleWidth(4);

  rightWidget->setMinimumWidth(340);

  rootLayout->addWidget(splitter);
}

void RunModeWidget::buildLockedPreview(QBoxLayout *parentLayout) {
  // 相机预览区
  auto *previewGroup = new QGroupBox(QString::fromUtf8("实时预览（锁定）"), this);
  auto *previewLayout = new QVBoxLayout(previewGroup);
  previewLayout->setContentsMargins(4, 8, 4, 4);
  auto *previewFrame = new QFrame(previewGroup);
  auto *previewFrameLayout = new QGridLayout(previewFrame);
  previewFrameLayout->setContentsMargins(0, 0, 0, 0);

  previewScene_ = new QGraphicsScene(previewGroup);
  previewView_ = new QGraphicsView(previewScene_, previewGroup);
  previewView_->setRenderHint(QPainter::Antialiasing, true);
  previewView_->setRenderHint(QPainter::SmoothPixmapTransform, true);
  previewView_->setStyleSheet(QStringLiteral(
      "QGraphicsView { background: #020617; border: 2px solid #1e293b; border-radius: 8px; }"));
  previewView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  previewView_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  previewView_->setDragMode(QGraphicsView::NoDrag);
  previewView_->setInteractive(false);
  previewView_->viewport()->setCursor(Qt::ArrowCursor);

  const QImage placeholder = buildPlaceholderPreview();
  previewPixmapItem_ = previewScene_->addPixmap(QPixmap::fromImage(placeholder));
  previewScene_->setSceneRect(0, 0, kPreviewWidth, kPreviewHeight);
  fitPreviewContent();

  // Overlay status text
  previewOverlayLabel_ = new QLabel(QString::fromUtf8("等待运行数据"), previewFrame);
  previewOverlayLabel_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  previewOverlayLabel_->setWordWrap(true);
  previewOverlayLabel_->setMinimumWidth(300);
  previewOverlayLabel_->setMaximumWidth(380);
  previewOverlayLabel_->setMargin(0);
  previewOverlayLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
  previewOverlayLabel_->setStyleSheet(QStringLiteral(
      "QLabel { background: rgba(15, 23, 42, 0.85); color: #facc15; padding: 4px 12px; "
      "  border-radius: 6px; font-size: 11px; font-weight: 600; line-height: 1.4; }"));

  previewFrameLayout->addWidget(previewView_, 0, 0);
  previewFrameLayout->addWidget(previewOverlayLabel_, 0, 0, Qt::AlignTop | Qt::AlignRight);
  previewLayout->addWidget(previewFrame);
  refreshPreviewOverlay();

  parentLayout->addWidget(previewGroup);
}

void RunModeWidget::buildDashboard(QBoxLayout *parentLayout) {
  auto *headerGroup = new QGroupBox(QString::fromUtf8("运行总览"), this);
  auto *headerLayout = new QVBoxLayout(headerGroup);
  headerLayout->setContentsMargins(12, 18, 12, 12);
  headerLayout->setSpacing(4);
  auto *headerTitle = new QLabel(QString::fromUtf8("生产运行界面"), headerGroup);
  headerTitle->setStyleSheet(QStringLiteral("color: #f8fafc; font-size: 18px; font-weight: 700;"));
  headerLayout->addWidget(headerTitle);
  parentLayout->addWidget(headerGroup);

  // Production stats
  auto *statsGroup = new QGroupBox(QString::fromUtf8("生产数据看板"), this);
  auto *statsLayout = new QGridLayout(statsGroup);
  statsLayout->setContentsMargins(12, 18, 12, 12);
  statsLayout->setSpacing(10);

  auto makeStatCard = [this](const QString &title, const QString &color, const QString &icon) {
    auto *card = new QFrame(this);
    card->setStyleSheet(QStringLiteral(
        "QFrame { background: #0f172a; border: 1px solid #334155; border-radius: 10px; }"
        "QLabel { color: %1; }").arg(color));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(4);

    auto *iconLabel = new QLabel(icon, card);
    iconLabel->setStyleSheet(QStringLiteral("font-size: 14px;"));
    auto *titleLabel = new QLabel(title, card);
    titleLabel->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 11px;"));
    auto *valueLabel = new QLabel(QStringLiteral("0"), card);
    valueLabel->setStyleSheet(QStringLiteral("font-size: 32px; font-weight: 700;"));

    layout->addWidget(iconLabel);
    layout->addWidget(valueLabel);
    layout->addWidget(titleLabel);
    return std::make_tuple(card, valueLabel);
  };

  auto [okCard, okVal] = makeStatCard(QString::fromUtf8("OK 数量"), QStringLiteral("#22c55e"), QString::fromUtf8("PASS"));
  auto [ngCard, ngVal] = makeStatCard(QString::fromUtf8("NG 数量"), QStringLiteral("#ef4444"), QString::fromUtf8("FAIL"));
  auto [boardCard, boardVal] = makeStatCard(QString::fromUtf8("当前板号"), QStringLiteral("#3b82f6"), QString::fromUtf8("BOARD"));
  auto [totalCard, totalVal] = makeStatCard(QString::fromUtf8("总镭射板数"), QStringLiteral("#facc15"), QString::fromUtf8("TOTAL"));

  okCountLabel_ = okVal;
  ngCountLabel_ = ngVal;
  currentBoardLabel_ = boardVal;
  totalBoardsLabel_ = totalVal;

  statsLayout->addWidget(okCard, 0, 0);
  statsLayout->addWidget(ngCard, 0, 1);
  statsLayout->addWidget(boardCard, 1, 0);
  statsLayout->addWidget(totalCard, 1, 1);
  parentLayout->addWidget(statsGroup);

  // Yield trend chart
  auto *yieldGroup = new QGroupBox(QString::fromUtf8("良率趋势"), this);
  auto *yieldLayout = new QVBoxLayout(yieldGroup);
  yieldLayout->setContentsMargins(4, 16, 4, 4);

  yieldSeries_ = new QLineSeries(this);
  yieldSeries_->setName(QString::fromUtf8("良率 %"));
  yieldSeries_->setColor(QColor("#22c55e"));
  yieldSeries_->setPen(QPen(QColor("#22c55e"), 2));

  yieldChart_ = new QChart();
  yieldChart_->addSeries(yieldSeries_);
  yieldChart_->setTitle(QString::fromUtf8("实时良率趋势"));
  yieldChart_->setTitleBrush(QBrush(QColor("#94a3b8")));
  yieldChart_->setBackgroundBrush(QBrush(QColor("#0f172a")));
  yieldChart_->setPlotAreaBackgroundBrush(QBrush(QColor("#020617")));
  yieldChart_->setPlotAreaBackgroundVisible(true);
  yieldChart_->legend()->setVisible(false);
  yieldChart_->setMargins(QMargins(0, 0, 0, 0));

  auto *axisX = new QValueAxis(this);
  axisX->setTitleText(QString::fromUtf8("板号"));
  axisX->setTitleBrush(QBrush(QColor("#64748b")));
  axisX->setLabelsColor(QColor("#94a3b8"));
  axisX->setGridLineColor(QColor("#1e293b"));
  axisX->setRange(0, 10);
  axisX->setLabelFormat("%d");
  yieldChart_->addAxis(axisX, Qt::AlignBottom);
  yieldSeries_->attachAxis(axisX);

  auto *axisY = new QValueAxis(this);
  axisY->setTitleText(QStringLiteral("%"));
  axisY->setTitleBrush(QBrush(QColor("#64748b")));
  axisY->setLabelsColor(QColor("#94a3b8"));
  axisY->setGridLineColor(QColor("#1e293b"));
  axisY->setRange(0, 100);
  axisY->setLabelFormat("%.0f");
  yieldChart_->addAxis(axisY, Qt::AlignLeft);
  yieldSeries_->attachAxis(axisY);

  yieldChartView_ = new QChartView(yieldChart_, yieldGroup);
  yieldChartView_->setRenderHint(QPainter::Antialiasing, true);
  yieldChartView_->setStyleSheet(QStringLiteral(
      "QChartView { background: #0f172a; border: 1px solid #334155; border-radius: 8px; }"));
  yieldChartView_->setMinimumHeight(160);
  yieldChartView_->setMaximumHeight(220);

  yieldLayout->addWidget(yieldChartView_);
  parentLayout->addWidget(yieldGroup);

  // Step progress
  auto *progressGroup = new QGroupBox(QString::fromUtf8("流程进度"), this);
  auto *progressLayout = new QVBoxLayout(progressGroup);
  progressLayout->setContentsMargins(12, 18, 12, 12);
  progressLayout->setSpacing(8);

  workflowStateLabel_ = new QLabel(QString::fromUtf8("等待启动"), progressGroup);
  workflowStateLabel_->setStyleSheet(QStringLiteral(
      "color: #facc15; font-weight: 700; font-size: 14px; padding: 4px 0;"));

  stepProgressLabel_ = new QLabel(QString::fromUtf8("就绪"), progressGroup);
  stepProgressLabel_->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 12px;"));

  stepProgressBar_ = new QProgressBar(progressGroup);
  stepProgressBar_->setRange(0, 1);
  stepProgressBar_->setValue(0);

  progressLayout->addWidget(workflowStateLabel_);
  progressLayout->addWidget(stepProgressLabel_);
  progressLayout->addWidget(stepProgressBar_);
  parentLayout->addWidget(progressGroup);

  // Control buttons
  auto *controlGroup = new QGroupBox(QString::fromUtf8("运行控制"), this);
  auto *controlLayout = new QHBoxLayout(controlGroup);
  controlLayout->setContentsMargins(12, 18, 12, 12);
  controlLayout->setSpacing(8);

  startButton_ = new QPushButton(QString::fromUtf8("启动运行"), controlGroup);
  startButton_->setStyleSheet(QStringLiteral(
      "QPushButton { background: #16a34a; color: #f8fafc; border: 1px solid #22c55e; border-radius: 8px; "
      "  padding: 8px 20px; min-height: 34px; font-weight: 700; font-size: 13px; }"
      "QPushButton:hover { background: #22c55e; }"
      "QPushButton:disabled { background: #1e293b; color: #475569; border-color: #334155; }"));

  stopButton_ = new QPushButton(QString::fromUtf8("停止运行"), controlGroup);
  stopButton_->setStyleSheet(QStringLiteral(
      "QPushButton { background: #b91c1c; color: #f8fafc; border: 1px solid #ef4444; border-radius: 8px; "
      "  padding: 8px 20px; min-height: 34px; font-weight: 700; font-size: 13px; }"
      "QPushButton:hover { background: #ef4444; }"
      "QPushButton:disabled { background: #1e293b; color: #475569; border-color: #334155; }"));

  pauseButton_ = new QPushButton(QString::fromUtf8("暂停"), controlGroup);
  pauseButton_->setStyleSheet(QStringLiteral(
      "QPushButton { background: #ca8a04; color: #f8fafc; border: 1px solid #eab308; border-radius: 8px; "
      "  padding: 8px 20px; min-height: 34px; font-weight: 700; font-size: 13px; }"
      "QPushButton:hover { background: #eab308; }"
      "QPushButton:disabled { background: #1e293b; color: #475569; border-color: #334155; }"));

  singleStepButton_ = new QPushButton(QString::fromUtf8("单步"), controlGroup);
  singleStepButton_->setStyleSheet(QStringLiteral(
      "QPushButton { background: #1e3a5f; color: #e2e8f0; border: 1px solid #334155; border-radius: 8px; "
      "  padding: 8px 16px; min-height: 34px; font-weight: 600; font-size: 13px; }"
      "QPushButton:hover { background: #2563eb; }"
      "QPushButton:disabled { background: #1e293b; color: #475569; }"));

  controlLayout->addWidget(startButton_);
  controlLayout->addWidget(pauseButton_);
  controlLayout->addWidget(singleStepButton_);
  controlLayout->addWidget(stopButton_);
  parentLayout->addWidget(controlGroup);

  // Production log
  auto *logGroup = new QGroupBox(QString::fromUtf8("生产日志"), this);
  auto *logLayout = new QVBoxLayout(logGroup);
  logLayout->setContentsMargins(8, 16, 8, 8);

  productionLogEdit_ = new QTextEdit(logGroup);
  productionLogEdit_->setReadOnly(true);
  productionLogEdit_->setPlaceholderText(QString::fromUtf8("运行日志将在此显示..."));
  productionLogEdit_->setMinimumHeight(150);
  logLayout->addWidget(productionLogEdit_);
  parentLayout->addWidget(logGroup, 1);

  // Connections
  connect(startButton_, &QPushButton::clicked, this, [this] {
    running_ = true;
    previewTimer_->start();
    startButton_->setEnabled(false);
    stopButton_->setEnabled(true);
    pauseButton_->setEnabled(true);
    singleStepButton_->setEnabled(true);
    appendProductionLog(QString::fromUtf8("运行已启动"));
    refreshPreviewOverlay();
    emit startRequested();
  });

  auto stopRun = [this] {
    running_ = false;
    previewTimer_->stop();
    startButton_->setEnabled(true);
    stopButton_->setEnabled(false);
    pauseButton_->setEnabled(false);
    singleStepButton_->setEnabled(false);
    appendProductionLog(QString::fromUtf8("运行已停止"));
    refreshPreviewOverlay();
    emit stopRequested();
  };

  connect(stopButton_, &QPushButton::clicked, this, stopRun);

  connect(pauseButton_, &QPushButton::clicked, this, [this] {
    running_ = false;
    previewTimer_->stop();
    startButton_->setEnabled(true);
    pauseButton_->setEnabled(false);
    appendProductionLog(QString::fromUtf8("运行已暂停"));
    refreshPreviewOverlay();
    emit pauseRequested();
  });

  connect(singleStepButton_, &QPushButton::clicked, this, [this] {
    appendProductionLog(QString::fromUtf8("执行单步..."));
    refreshPreviewOverlay();
    emit singleStepRequested();
  });

  stopButton_->setEnabled(false);
  pauseButton_->setEnabled(false);
  singleStepButton_->setEnabled(false);
}

void RunModeWidget::refreshPreview() {
  if (!frameProvider_ || previewPixmapItem_ == nullptr) {
    return;
  }

  QImage frame = frameProvider_();
  if (frame.isNull()) {
    return;
  }

  lastPreviewFrame_ = frame;
  const QPixmap pixmap = QPixmap::fromImage(
      frame.scaled(kPreviewWidth, kPreviewHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  previewPixmapItem_->setPixmap(pixmap);
  fitPreviewContent();
  refreshPreviewOverlay();
}

void RunModeWidget::refreshPreviewOverlay() {
  if (previewOverlayLabel_ == nullptr) {
    return;
  }

  const QString overlayText = statusTextProvider_ ? statusTextProvider_().trimmed()
                                                  : QStringLiteral("等待运行数据");
  previewOverlayLabel_->setText(overlayText.isEmpty() ? QStringLiteral("等待运行数据") : overlayText);
}

void RunModeWidget::refreshDashboard() {
  if (okCountLabel_ != nullptr) {
    okCountLabel_->setText(QString::number(okCount_));
  }

  if (ngCountLabel_ != nullptr) {
    ngCountLabel_->setText(QString::number(ngCount_));
  }

  if (currentBoardLabel_ != nullptr) {
    currentBoardLabel_->setText(QStringLiteral("#%1").arg(currentBoardIndex_));
  }

  if (totalBoardsLabel_ != nullptr) {
    totalBoardsLabel_->setText(QString::number(totalBoards_));
  }

  refreshPreviewOverlay();
}

#endif

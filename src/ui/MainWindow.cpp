#include "ui/MainWindow.h"

#ifdef AOI_HAS_QT_WIDGETS

#include "ui/CadGraphicsView.h"
#include "ui/CadRulerWidget.h"
#include "ui/CameraCalibDialog.h"
#include "ui/MotionControlDialog.h"
#include "ui/ProgramEditDialog.h"
#include "ui_MainWindow.h"

#include "vision/CodeReader.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include <QAction>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QDoubleSpinBox>

namespace {

constexpr int kWorkbenchImageWidth = 1800;
constexpr int kWorkbenchImageHeight = 1200;

QString markShapeDisplayText(const MarkShape shape) {
  switch (shape) {
  case MarkShape::Rectangle:
    return QStringLiteral("矩形");
  case MarkShape::Circle:
    return QStringLiteral("圆形");
  case MarkShape::Diamond:
    return QStringLiteral("菱形");
  case MarkShape::Cross:
    return QStringLiteral("十字");
  }

  return QStringLiteral("矩形");
}

QString markAlgorithmDisplayText(const MarkAlgorithm algorithm) {
  switch (algorithm) {
  case MarkAlgorithm::ColorBrushTemplate:
    return QStringLiteral("画笔抽色");
  case MarkAlgorithm::BinaryGeometry:
    return QStringLiteral("黑白几何");
  }

  return QStringLiteral("画笔抽色");
}

QString roiShapeDisplayText(const RoiShape shape) {
  switch (shape) {
  case RoiShape::Rectangle:
    return QStringLiteral("矩形");
  case RoiShape::Circle:
    return QStringLiteral("圆形");
  }

  return QStringLiteral("矩形");
}

MarkShape markShapeFromDisplayText(const QString &text) {
  if (text == QStringLiteral("圆形")) {
    return MarkShape::Circle;
  }

  if (text == QStringLiteral("菱形")) {
    return MarkShape::Diamond;
  }

  if (text == QStringLiteral("十字")) {
    return MarkShape::Cross;
  }

  return MarkShape::Rectangle;
}

MarkAlgorithm markAlgorithmFromDisplayText(const QString &text) {
  if (text == QStringLiteral("黑白几何")) {
    return MarkAlgorithm::BinaryGeometry;
  }

  return MarkAlgorithm::ColorBrushTemplate;
}

RoiShape roiShapeFromDisplayText(const QString &text) {
  if (text == QStringLiteral("圆形")) {
    return RoiShape::Circle;
  }

  return RoiShape::Rectangle;
}

double normalizeScore(const double value) {
  return std::clamp(value, 0.0, 1.0);
}

double calculatePreviewScore(const MarkPoint &mark) {
  const double areaScore =
      std::clamp((mark.width * mark.height) / 2800.0, 0.18, 0.96);
  const double shapeBias = mark.shape == MarkShape::Cross ? 0.05 : (mark.shape == MarkShape::Circle ? 0.03 : 0.0);
  const double algorithmBias = mark.algorithm == MarkAlgorithm::ColorBrushTemplate ? 0.08 : 0.04;
  const double colorBias = mark.algorithm == MarkAlgorithm::ColorBrushTemplate ? (mark.colorTolerance / 400.0) : 0.0;
  return normalizeScore(0.56 + areaScore * 0.22 + algorithmBias + shapeBias - colorBias);
}

double calculateLiveScore(const MarkPoint &mark, const QSize &frameSize) {
  const double frameBias = std::clamp((frameSize.width() + frameSize.height()) / 4000.0, 0.04, 0.22);
  const double stabilityBias = std::max(0.0, mark.previewScore - mark.minimumScore) * 0.55;
  const double geometryBias = mark.algorithm == MarkAlgorithm::BinaryGeometry ? 0.05 : 0.02;
  return normalizeScore(mark.minimumScore - 0.03 + frameBias + stabilityBias + geometryBias);
}

QColor colorFromMark(const MarkPoint &mark) {
  const QColor candidate(QString::fromStdString(mark.sampledColor));
  return candidate.isValid() ? candidate : QColor("#ef4444");
}

QImage buildWorkbenchImage(const std::optional<ProgramModel> &program, const QString &cameraMode,
                           const QSize &fovSize) {
  QImage image(kWorkbenchImageWidth, kWorkbenchImageHeight, QImage::Format_ARGB32_Premultiplied);
  image.fill(QColor("#050b15"));

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, true);

  painter.fillRect(QRect(0, 0, image.width(), image.height()), QColor("#08111f"));

  painter.setPen(QPen(QColor("#13243a"), 1));
  for (int x = 0; x <= image.width(); x += 50) {
    painter.drawLine(x, 0, x, image.height());
  }
  for (int y = 0; y <= image.height(); y += 50) {
    painter.drawLine(0, y, image.width(), y);
  }

  painter.setPen(QPen(QColor("#1e3a5f"), 2));
  for (int x = 0; x <= image.width(); x += 250) {
    painter.drawLine(x, 0, x, image.height());
  }
  for (int y = 0; y <= image.height(); y += 250) {
    painter.drawLine(0, y, image.width(), y);
  }

  painter.setPen(QPen(QColor("#38bdf8"), 2));
  painter.drawLine(0, 0, image.width(), 0);
  painter.drawLine(0, 0, 0, image.height());
  painter.setPen(QColor("#94a3b8"));
  painter.drawText(QRect(24, 18, 980, 30),
                   QStringLiteral("CAD 式 AOI 工位图 | 相机模式：%1 | 当前 FOV：%2 x %3 | 中键拖拽 / 滚轮缩放 / 左键框选")
                       .arg(cameraMode)
                       .arg(fovSize.width())
                       .arg(fovSize.height()));

  painter.setBrush(QColor(14, 116, 144, 28));
  painter.setPen(QPen(QColor("#0ea5e9"), 2));
  painter.drawRoundedRect(QRectF(180.0, 120.0, 1320.0, 880.0), 18.0, 18.0);
  painter.drawText(QRectF(210.0, 136.0, 300.0, 28.0), QStringLiteral("整板拼接区"));

  painter.setBrush(QColor(34, 197, 94, 36));
  painter.setPen(QPen(QColor("#22c55e"), 2));
  painter.drawRoundedRect(QRectF(320.0, 250.0, 420.0, 260.0), 10.0, 10.0);
  painter.drawText(QRectF(340.0, 264.0, 220.0, 28.0), QStringLiteral("检测单元 A"));
  painter.drawRoundedRect(QRectF(860.0, 250.0, 420.0, 260.0), 10.0, 10.0);
  painter.drawText(QRectF(880.0, 264.0, 220.0, 28.0), QStringLiteral("检测单元 B"));

  painter.setBrush(QColor(250, 204, 21, 40));
  painter.setPen(QPen(QColor("#facc15"), 2, Qt::DashLine));
  painter.drawRoundedRect(QRectF(530.0, 680.0, 660.0, 210.0), 12.0, 12.0);
  painter.drawText(QRectF(554.0, 696.0, 280.0, 28.0), QStringLiteral("动态 FOV 漫游轨迹区"));

  if (!program.has_value()) {
    painter.setPen(QColor("#cbd5e1"));
    painter.drawText(QRect(0, 0, image.width(), image.height()), Qt::AlignCenter,
                     QStringLiteral("当前尚未加载程序\n请通过“文件”菜单新建或打开程序后，在左侧画布中框选 Mark 与 ROI。"));
    return image;
  }

  painter.setPen(QColor("#cbd5e1"));
  painter.drawText(QRect(24, 1138, 1560, 30),
                   QStringLiteral("程序：%1 | Mark：%2 | ROI：%3 | 读码区域：%4")
                       .arg(QString::fromStdString(program->name))
                       .arg(static_cast<int>(program->marks.size()))
                       .arg(static_cast<int>(program->rois.size()))
                       .arg(QString::fromStdString(program->codeRegionName)));

  return image;
}

class ProgramShapeItem final : public QGraphicsObject {
public:
  enum class Domain {
    Mark,
    Roi,
  };

  ProgramShapeItem(const Domain domain, const int index, const QRectF &localRect, const QString &label,
                   const QColor &baseColor, const MarkShape markShape = MarkShape::Rectangle,
                   const RoiShape roiShape = RoiShape::Rectangle)
      : domain_(domain),
        index_(index),
        localRect_(localRect),
        label_(label),
        baseColor_(baseColor),
        markShape_(markShape),
        roiShape_(roiShape) {
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
  }

  QRectF boundingRect() const override {
    return localRect_.adjusted(-8.0, -26.0, 8.0, 8.0);
  }

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(isSelected() ? QColor("#facc15") : baseColor_, isSelected() ? 3.0 : 2.0));
    painter->setBrush(domain_ == Domain::Mark ? QColor(baseColor_.red(), baseColor_.green(), baseColor_.blue(), 55)
                                              : QColor(baseColor_.red(), baseColor_.green(), baseColor_.blue(), 34));

    if (domain_ == Domain::Mark) {
      switch (markShape_) {
      case MarkShape::Rectangle:
        painter->drawRect(localRect_);
        break;
      case MarkShape::Circle:
        painter->drawEllipse(localRect_);
        break;
      case MarkShape::Diamond: {
        QPolygonF polygon;
        polygon << QPointF(localRect_.center().x(), localRect_.top())
                << QPointF(localRect_.right(), localRect_.center().y())
                << QPointF(localRect_.center().x(), localRect_.bottom())
                << QPointF(localRect_.left(), localRect_.center().y());
        painter->drawPolygon(polygon);
        break;
      }
      case MarkShape::Cross:
        painter->drawLine(QPointF(localRect_.left(), localRect_.center().y()),
                          QPointF(localRect_.right(), localRect_.center().y()));
        painter->drawLine(QPointF(localRect_.center().x(), localRect_.top()),
                          QPointF(localRect_.center().x(), localRect_.bottom()));
        painter->drawEllipse(localRect_.center(), 3.0, 3.0);
        break;
      }
    } else {
      if (roiShape_ == RoiShape::Circle) {
        painter->drawEllipse(localRect_);
      } else {
        painter->drawRect(localRect_);
      }
    }

    painter->setPen(QColor("#e2e8f0"));
    painter->drawText(QRectF(localRect_.left(), localRect_.top() - 22.0, 180.0, 20.0), label_);
    painter->restore();
  }

  void setSelectionHandler(std::function<void(Domain, int)> handler) { selectionHandler_ = std::move(handler); }
  void setMoveFinishedHandler(std::function<void(Domain, int, QPointF)> handler) {
    moveFinishedHandler_ = std::move(handler);
  }

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent *event) override {
    QGraphicsObject::mousePressEvent(event);
    if (selectionHandler_) {
      selectionHandler_(domain_, index_);
    }
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override {
    QGraphicsObject::mouseReleaseEvent(event);
    if (moveFinishedHandler_) {
      moveFinishedHandler_(domain_, index_, pos());
    }
  }

private:
  Domain domain_;
  int index_ {0};
  QRectF localRect_;
  QString label_;
  QColor baseColor_;
  MarkShape markShape_ {MarkShape::Rectangle};
  RoiShape roiShape_ {RoiShape::Rectangle};
  std::function<void(Domain, int)> selectionHandler_;
  std::function<void(Domain, int, QPointF)> moveFinishedHandler_;
};

} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui_(new Ui::MainWindow) {
  ui_->setupUi(this);
  cameraTimer_ = new QTimer(this);
  cameraTimer_->setInterval(90);
  connect(cameraTimer_, &QTimer::timeout, this, &MainWindow::updateCameraFrame);

  buildMenus();
  buildCentralUi();
  createDefaultProgram();
  refreshCameraState();
  refreshStatusSummary();
  appendLog(QStringLiteral("主界面已切换为 CAD 式预览工位布局。"));
  appendLog(QStringLiteral("左侧支持中键拖拽、滚轮缩放、左键框选生成 Mark/ROI。"));
}

MainWindow::~MainWindow() {
  stopCameraPreview();
  delete ui_;
}

void MainWindow::buildMenus() {
  auto *fileMenu = menuBar()->addMenu(QStringLiteral("文件"));
  auto *newProgramAction = fileMenu->addAction(QStringLiteral("新建程序"));
  auto *openProgramAction = fileMenu->addAction(QStringLiteral("打开程序"));
  auto *saveProgramAction = fileMenu->addAction(QStringLiteral("保存程序"));
  fileMenu->addSeparator();
  auto *exitAction = fileMenu->addAction(QStringLiteral("退出"));

  auto *cameraMenu = menuBar()->addMenu(QStringLiteral("相机"));
  auto *cameraConfigAction = cameraMenu->addAction(QStringLiteral("相机配置"));
  auto *startPreviewAction = cameraMenu->addAction(QStringLiteral("开始预览"));
  auto *stopPreviewAction = cameraMenu->addAction(QStringLiteral("停止预览"));

  auto *motionMenu = menuBar()->addMenu(QStringLiteral("运控"));
  auto *openMotionAction = motionMenu->addAction(QStringLiteral("打开虚拟运控面板"));

  connect(newProgramAction, &QAction::triggered, this, &MainWindow::createDefaultProgram);
  connect(openProgramAction, &QAction::triggered, this, &MainWindow::openProgram);
  connect(saveProgramAction, &QAction::triggered, this, &MainWindow::saveCurrentProgram);
  connect(exitAction, &QAction::triggered, this, &QWidget::close);
  connect(cameraConfigAction, &QAction::triggered, this, &MainWindow::openCameraConfig);
  connect(startPreviewAction, &QAction::triggered, this, &MainWindow::startCameraPreview);
  connect(stopPreviewAction, &QAction::triggered, this, &MainWindow::stopCameraPreview);
  connect(openMotionAction, &QAction::triggered, this, &MainWindow::openMotionPanel);

  auto *toolBar = addToolBar(QStringLiteral("主工具栏"));
  toolBar->setMovable(false);
  toolBar->addAction(newProgramAction);
  toolBar->addAction(openProgramAction);
  toolBar->addAction(saveProgramAction);
  toolBar->addSeparator();
  toolBar->addAction(cameraConfigAction);
  toolBar->addAction(startPreviewAction);
  toolBar->addAction(openMotionAction);
}

void MainWindow::buildCentralUi() {
  auto *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  auto *rootLayout = new QVBoxLayout(centralWidget);
  rootLayout->setContentsMargins(12, 12, 12, 12);
  rootLayout->setSpacing(12);

  auto *headerFrame = new QFrame(centralWidget);
  headerFrame->setStyleSheet(
      QStringLiteral("QFrame { background: #0f172a; border-radius: 14px; } QLabel { color: #e2e8f0; }"));
  auto *headerLayout = new QHBoxLayout(headerFrame);
  auto *titleLabel = new QLabel(QStringLiteral("工业 AOI 视觉上位机"), headerFrame);
  titleLabel->setStyleSheet(QStringLiteral("font-size: 26px; font-weight: 800; color: #f8fafc;"));
  statusSummaryValueLabel_ = new QLabel(QStringLiteral("系统初始化中"), headerFrame);
  statusSummaryValueLabel_->setStyleSheet(
      QStringLiteral("padding: 8px 12px; background: rgba(148,163,184,0.16); border-radius: 10px;"));
  headerLayout->addWidget(titleLabel);
  headerLayout->addStretch();
  headerLayout->addWidget(statusSummaryValueLabel_);
  rootLayout->addWidget(headerFrame);

  auto *splitter = new QSplitter(Qt::Horizontal, centralWidget);
  splitter->setChildrenCollapsible(false);
  rootLayout->addWidget(splitter, 1);

  auto *leftWidget = new QWidget(splitter);
  auto *leftLayout = new QVBoxLayout(leftWidget);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  leftLayout->setSpacing(10);
  buildLeftWorkbench(leftLayout);

  auto *rightWidget = new QWidget(splitter);
  auto *rightLayout = new QVBoxLayout(rightWidget);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  rightLayout->setSpacing(10);
  buildRightPanel(rightLayout);

  splitter->addWidget(leftWidget);
  splitter->addWidget(rightWidget);
  splitter->setStretchFactor(0, 4);
  splitter->setStretchFactor(1, 3);

  statusBar()->showMessage(QStringLiteral("就绪"));
}

void MainWindow::buildLeftWorkbench(QBoxLayout *parentLayout) {
  auto *toolbarFrame = new QFrame(this);
  toolbarFrame->setStyleSheet(
      QStringLiteral("QFrame { background: #f8fafc; border: 1px solid #d0d5dd; border-radius: 12px; }"));
  auto *toolbarLayout = new QHBoxLayout(toolbarFrame);

  auto *selectModeButton = new QPushButton(QStringLiteral("选择"), toolbarFrame);
  auto *drawRoiButton = new QPushButton(QStringLiteral("框选 ROI"), toolbarFrame);
  auto *drawMarkButton = new QPushButton(QStringLiteral("框选 Mark"), toolbarFrame);
  auto *fitViewButton = new QPushButton(QStringLiteral("适配视图"), toolbarFrame);
  auto *startPreviewButton = new QPushButton(QStringLiteral("开始实时采图"), toolbarFrame);
  auto *stopPreviewButton = new QPushButton(QStringLiteral("停止采图"), toolbarFrame);
  toggleFovButton_ = new QPushButton(QStringLiteral("隐藏 FOV 640x360"), toolbarFrame);
  cameraModeValueLabel_ = new QLabel(QStringLiteral("--"), toolbarFrame);
  cameraStatusToolbarValueLabel_ = new QLabel(QStringLiteral("未启动"), toolbarFrame);
  fovInfoValueLabel_ = new QLabel(QStringLiteral("640 x 360"), toolbarFrame);
  cursorPositionValueLabel_ = new QLabel(QStringLiteral("X=0.0 Y=0.0"), toolbarFrame);
  zoomValueLabel_ = new QLabel(QStringLiteral("缩放 100%"), toolbarFrame);

  toolbarLayout->addWidget(selectModeButton);
  toolbarLayout->addWidget(drawRoiButton);
  toolbarLayout->addWidget(drawMarkButton);
  toolbarLayout->addWidget(fitViewButton);
  toolbarLayout->addWidget(startPreviewButton);
  toolbarLayout->addWidget(stopPreviewButton);
  toolbarLayout->addWidget(toggleFovButton_);
  toolbarLayout->addStretch();
  toolbarLayout->addWidget(cursorPositionValueLabel_);
  toolbarLayout->addSpacing(8);
  toolbarLayout->addWidget(zoomValueLabel_);
  toolbarLayout->addSpacing(8);
  toolbarLayout->addWidget(new QLabel(QStringLiteral("相机模式"), toolbarFrame));
  toolbarLayout->addWidget(cameraModeValueLabel_);
  toolbarLayout->addSpacing(8);
  toolbarLayout->addWidget(new QLabel(QStringLiteral("FOV"), toolbarFrame));
  toolbarLayout->addWidget(fovInfoValueLabel_);
  toolbarLayout->addSpacing(8);
  toolbarLayout->addWidget(new QLabel(QStringLiteral("状态"), toolbarFrame));
  toolbarLayout->addWidget(cameraStatusToolbarValueLabel_);

  parentLayout->addWidget(toolbarFrame);

  auto *graphicsGroupBox = new QGroupBox(QStringLiteral("CAD 拼接工位图"), this);
  auto *graphicsLayout = new QGridLayout(graphicsGroupBox);
  graphicsLayout->setContentsMargins(8, 8, 8, 8);
  graphicsLayout->setSpacing(0);

  auto *cornerLabel = new QLabel(QStringLiteral("XY"), graphicsGroupBox);
  cornerLabel->setAlignment(Qt::AlignCenter);
  cornerLabel->setMinimumSize(40, 28);
  cornerLabel->setStyleSheet(QStringLiteral("background: #111827; color: #cbd5e1; border-right: 1px solid #334155;"));

  topRulerWidget_ = new CadRulerWidget(Qt::Horizontal, graphicsGroupBox);
  leftRulerWidget_ = new CadRulerWidget(Qt::Vertical, graphicsGroupBox);
  workbenchGraphicsView_ = new CadGraphicsView(graphicsGroupBox);
  workbenchGraphicsView_->setRenderHint(QPainter::Antialiasing, true);
  workbenchGraphicsView_->setRenderHint(QPainter::SmoothPixmapTransform, true);
  workbenchGraphicsView_->setStyleSheet(
      QStringLiteral("QGraphicsView { background: #030712; border: 1px solid #1f2937; }"));

  workbenchScene_ = new QGraphicsScene(workbenchGraphicsView_);
  workbenchGraphicsView_->setScene(workbenchScene_);
  topRulerWidget_->setView(workbenchGraphicsView_);
  leftRulerWidget_->setView(workbenchGraphicsView_);

  graphicsLayout->addWidget(cornerLabel, 0, 0);
  graphicsLayout->addWidget(topRulerWidget_, 0, 1);
  graphicsLayout->addWidget(leftRulerWidget_, 1, 0);
  graphicsLayout->addWidget(workbenchGraphicsView_, 1, 1);
  graphicsLayout->setColumnStretch(1, 1);
  graphicsLayout->setRowStretch(1, 1);
  parentLayout->addWidget(graphicsGroupBox, 1);

  connect(selectModeButton, &QPushButton::clicked, this, [this] { setCanvasMode(CanvasMode::Select); });
  connect(drawRoiButton, &QPushButton::clicked, this, [this] {
    setCurrentPage(0);
    setCanvasMode(CanvasMode::DrawRoi);
  });
  connect(drawMarkButton, &QPushButton::clicked, this, [this] {
    setCurrentPage(1);
    setCanvasMode(CanvasMode::DrawMark);
  });
  connect(fitViewButton, &QPushButton::clicked, this, &MainWindow::resetWorkbenchView);
  connect(startPreviewButton, &QPushButton::clicked, this, &MainWindow::startCameraPreview);
  connect(stopPreviewButton, &QPushButton::clicked, this, &MainWindow::stopCameraPreview);
  connect(toggleFovButton_, &QPushButton::clicked, this, &MainWindow::toggleFovOverlay);
  connect(workbenchGraphicsView_, &CadGraphicsView::cursorScenePositionChanged, this,
          &MainWindow::updateCursorCoordinate);
  connect(workbenchGraphicsView_, &CadGraphicsView::viewTransformChanged, this, [this] {
    zoomValueLabel_->setText(QStringLiteral("缩放 %1%").arg(workbenchGraphicsView_->zoomFactor() * 100.0, 0, 'f', 0));
    topRulerWidget_->update();
    leftRulerWidget_->update();
  });
  connect(workbenchGraphicsView_, &CadGraphicsView::regionDrawn, this, &MainWindow::handleDrawnRegion);
}

void MainWindow::buildRightPanel(QBoxLayout *parentLayout) {
  auto *summaryGroupBox = new QGroupBox(QStringLiteral("程序概览"), this);
  auto *summaryLayout = new QFormLayout(summaryGroupBox);
  programNameValueLabel_ = new QLabel(QStringLiteral("--"), summaryGroupBox);
  programPathValueLabel_ = new QLabel(QStringLiteral("--"), summaryGroupBox);
  programPathValueLabel_->setWordWrap(true);
  programAiModelValueLabel_ = new QLabel(QStringLiteral("--"), summaryGroupBox);
  markCountValueLabel_ = new QLabel(QStringLiteral("0"), summaryGroupBox);
  roiCountValueLabel_ = new QLabel(QStringLiteral("0"), summaryGroupBox);
  motionStatusValueLabel_ = new QLabel(QStringLiteral("运行就绪"), summaryGroupBox);
  cameraDeviceValueLabel_ = new QLabel(QStringLiteral("0"), summaryGroupBox);
  cameraStatusDetailValueLabel_ = new QLabel(QStringLiteral("未启动"), summaryGroupBox);
  summaryLayout->addRow(QStringLiteral("程序名称"), programNameValueLabel_);
  summaryLayout->addRow(QStringLiteral("程序路径"), programPathValueLabel_);
  summaryLayout->addRow(QStringLiteral("AI 模型"), programAiModelValueLabel_);
  summaryLayout->addRow(QStringLiteral("Mark 数量"), markCountValueLabel_);
  summaryLayout->addRow(QStringLiteral("ROI 数量"), roiCountValueLabel_);
  summaryLayout->addRow(QStringLiteral("运控状态"), motionStatusValueLabel_);
  summaryLayout->addRow(QStringLiteral("设备索引"), cameraDeviceValueLabel_);
  summaryLayout->addRow(QStringLiteral("相机状态"), cameraStatusDetailValueLabel_);
  parentLayout->addWidget(summaryGroupBox);

  auto *stackFrame = new QFrame(this);
  stackFrame->setStyleSheet(QStringLiteral("QFrame { background: #ffffff; border: 1px solid #d0d5dd; border-radius: 14px; }"));
  auto *stackLayout = new QHBoxLayout(stackFrame);
  stackLayout->setContentsMargins(10, 10, 10, 10);
  stackLayout->setSpacing(10);

  toolSelectorListWidget_ = new QListWidget(stackFrame);
  toolSelectorListWidget_->addItem(QStringLiteral("ROI 样本点"));
  toolSelectorListWidget_->addItem(QStringLiteral("Mark 点"));
  toolSelectorListWidget_->addItem(QStringLiteral("模版编辑"));
  toolSelectorListWidget_->setFixedWidth(126);
  toolSelectorListWidget_->setCurrentRow(0);

  toolStackedWidget_ = new QStackedWidget(stackFrame);
  stackLayout->addWidget(toolSelectorListWidget_);
  stackLayout->addWidget(toolStackedWidget_, 1);
  parentLayout->addWidget(stackFrame, 1);

  buildRoiPage();
  buildMarkPage();
  buildTemplatePage();

  auto *overviewGroupBox = new QGroupBox(QStringLiteral("运行日志"), this);
  auto *overviewLayout = new QVBoxLayout(overviewGroupBox);
  operationLogTextEdit_ = new QTextEdit(overviewGroupBox);
  operationLogTextEdit_->setReadOnly(true);
  overviewLayout->addWidget(operationLogTextEdit_);
  parentLayout->addWidget(overviewGroupBox, 1);

  connect(toolSelectorListWidget_, &QListWidget::currentRowChanged, this, &MainWindow::setCurrentPage);
}

void MainWindow::buildOverviewPage() {}

void MainWindow::buildRoiPage() {
  auto *page = new QWidget(toolStackedWidget_);
  auto *layout = new QVBoxLayout(page);

  auto *toolGroupBox = new QGroupBox(QStringLiteral("ROI 绘制与工艺设置"), page);
  auto *toolLayout = new QFormLayout(toolGroupBox);
  roiNameLineEdit_ = new QLineEdit(toolGroupBox);
  roiNameLineEdit_->setPlaceholderText(QStringLiteral("例如 Inspect-1"));
  roiShapeComboBox_ = new QComboBox(toolGroupBox);
  roiShapeComboBox_->addItems({QStringLiteral("矩形"), QStringLiteral("圆形")});
  roiThresholdSpinBox_ = new QDoubleSpinBox(toolGroupBox);
  roiThresholdSpinBox_->setRange(0.0, 1.0);
  roiThresholdSpinBox_->setDecimals(3);
  roiThresholdSpinBox_->setSingleStep(0.01);
  roiThresholdSpinBox_->setValue(0.78);
  roiWidthSpinBox_ = new QDoubleSpinBox(toolGroupBox);
  roiHeightSpinBox_ = new QDoubleSpinBox(toolGroupBox);
  roiRotationSpinBox_ = new QDoubleSpinBox(toolGroupBox);
  for (auto *spinBox : {roiWidthSpinBox_, roiHeightSpinBox_}) {
    spinBox->setRange(4.0, 2000.0);
    spinBox->setDecimals(1);
    spinBox->setValue(100.0);
  }
  roiHeightSpinBox_->setValue(60.0);
  roiRotationSpinBox_->setRange(-180.0, 180.0);
  roiRotationSpinBox_->setDecimals(1);
  toolLayout->addRow(QStringLiteral("ROI 名称"), roiNameLineEdit_);
  toolLayout->addRow(QStringLiteral("ROI 形状"), roiShapeComboBox_);
  toolLayout->addRow(QStringLiteral("检测阈值"), roiThresholdSpinBox_);
  toolLayout->addRow(QStringLiteral("默认宽度"), roiWidthSpinBox_);
  toolLayout->addRow(QStringLiteral("默认高度"), roiHeightSpinBox_);
  toolLayout->addRow(QStringLiteral("旋转角度"), roiRotationSpinBox_);
  layout->addWidget(toolGroupBox);

  auto *buttonLayout = new QHBoxLayout;
  auto *drawButton = new QPushButton(QStringLiteral("进入框选 ROI"), page);
  auto *applyButton = new QPushButton(QStringLiteral("更新选中 ROI"), page);
  auto *deleteButton = new QPushButton(QStringLiteral("删除选中 ROI"), page);
  auto *centerButton = new QPushButton(QStringLiteral("定位到选中"), page);
  buttonLayout->addWidget(drawButton);
  buttonLayout->addWidget(applyButton);
  buttonLayout->addWidget(deleteButton);
  buttonLayout->addWidget(centerButton);
  layout->addLayout(buttonLayout);

  roiTableWidget_ = new QTableWidget(0, 7, page);
  roiTableWidget_->setHorizontalHeaderLabels(
      {QStringLiteral("名称"), QStringLiteral("形状"), QStringLiteral("阈值"), QStringLiteral("X"),
       QStringLiteral("Y"), QStringLiteral("宽高"), QStringLiteral("状态")});
  roiTableWidget_->horizontalHeader()->setStretchLastSection(true);
  roiTableWidget_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  roiTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
  roiTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
  roiTableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  layout->addWidget(roiTableWidget_, 1);

  toolStackedWidget_->addWidget(page);

  connect(drawButton, &QPushButton::clicked, this, [this] { setCanvasMode(CanvasMode::DrawRoi); });
  connect(applyButton, &QPushButton::clicked, this, &MainWindow::applyRoiEditorToSelection);
  connect(deleteButton, &QPushButton::clicked, this, &MainWindow::removeSelectedRoi);
  connect(centerButton, &QPushButton::clicked, this, &MainWindow::centerOnCurrentSelection);
  connect(roiTableWidget_, &QTableWidget::itemSelectionChanged, this, [this] {
    const int row = roiTableWidget_->currentRow();
    if (row >= 0) {
      selectRoiIndex(row);
    }
  });
}

void MainWindow::buildMarkPage() {
  auto *page = new QWidget(toolStackedWidget_);
  auto *layout = new QVBoxLayout(page);

  auto *toolGroupBox = new QGroupBox(QStringLiteral("Mark 工艺设置"), page);
  auto *toolLayout = new QFormLayout(toolGroupBox);
  markNameLineEdit_ = new QLineEdit(toolGroupBox);
  markNameLineEdit_->setPlaceholderText(QStringLiteral("例如 Mark-Left"));
  markShapeComboBox_ = new QComboBox(toolGroupBox);
  markShapeComboBox_->addItems(
      {QStringLiteral("矩形"), QStringLiteral("圆形"), QStringLiteral("菱形"), QStringLiteral("十字")});
  markAlgorithmComboBox_ = new QComboBox(toolGroupBox);
  markAlgorithmComboBox_->addItems({QStringLiteral("画笔抽色"), QStringLiteral("黑白几何")});
  markColorLineEdit_ = new QLineEdit(QStringLiteral("#ff4d4f"), toolGroupBox);
  markMinScoreSpinBox_ = new QDoubleSpinBox(toolGroupBox);
  markMinScoreSpinBox_->setRange(0.0, 1.0);
  markMinScoreSpinBox_->setDecimals(3);
  markMinScoreSpinBox_->setSingleStep(0.01);
  markMinScoreSpinBox_->setValue(0.80);
  markWidthSpinBox_ = new QDoubleSpinBox(toolGroupBox);
  markHeightSpinBox_ = new QDoubleSpinBox(toolGroupBox);
  markRotationSpinBox_ = new QDoubleSpinBox(toolGroupBox);
  for (auto *spinBox : {markWidthSpinBox_, markHeightSpinBox_}) {
    spinBox->setRange(4.0, 1000.0);
    spinBox->setDecimals(1);
    spinBox->setValue(48.0);
  }
  markRotationSpinBox_->setRange(-180.0, 180.0);
  markRotationSpinBox_->setDecimals(1);
  markPreviewScoreSpinBox_ = new QDoubleSpinBox(toolGroupBox);
  markPreviewScoreSpinBox_->setRange(0.0, 1.0);
  markPreviewScoreSpinBox_->setDecimals(3);
  markPreviewScoreSpinBox_->setReadOnly(true);
  markPreviewScoreSpinBox_->setButtonSymbols(QAbstractSpinBox::NoButtons);
  markLiveScoreSpinBox_ = new QDoubleSpinBox(toolGroupBox);
  markLiveScoreSpinBox_->setRange(0.0, 1.0);
  markLiveScoreSpinBox_->setDecimals(3);
  markLiveScoreSpinBox_->setReadOnly(true);
  markLiveScoreSpinBox_->setButtonSymbols(QAbstractSpinBox::NoButtons);
  toolLayout->addRow(QStringLiteral("Mark 名称"), markNameLineEdit_);
  toolLayout->addRow(QStringLiteral("模板形状"), markShapeComboBox_);
  toolLayout->addRow(QStringLiteral("识别算法"), markAlgorithmComboBox_);
  toolLayout->addRow(QStringLiteral("抽色前景色"), markColorLineEdit_);
  toolLayout->addRow(QStringLiteral("最低匹配度"), markMinScoreSpinBox_);
  toolLayout->addRow(QStringLiteral("默认宽度"), markWidthSpinBox_);
  toolLayout->addRow(QStringLiteral("默认高度"), markHeightSpinBox_);
  toolLayout->addRow(QStringLiteral("旋转角度"), markRotationSpinBox_);
  toolLayout->addRow(QStringLiteral("预览匹配度"), markPreviewScoreSpinBox_);
  toolLayout->addRow(QStringLiteral("实拍匹配度"), markLiveScoreSpinBox_);
  layout->addWidget(toolGroupBox);

  markPreviewProgressBar_ = new QProgressBar(page);
  markPreviewProgressBar_->setRange(0, 100);
  markPreviewProgressBar_->setValue(86);
  layout->addWidget(markPreviewProgressBar_);

  auto *buttonLayout = new QHBoxLayout;
  auto *drawButton = new QPushButton(QStringLiteral("进入框选 Mark"), page);
  auto *previewButton = new QPushButton(QStringLiteral("预览匹配"), page);
  auto *applyButton = new QPushButton(QStringLiteral("更新选中 Mark"), page);
  auto *deleteButton = new QPushButton(QStringLiteral("删除选中 Mark"), page);
  buttonLayout->addWidget(drawButton);
  buttonLayout->addWidget(previewButton);
  buttonLayout->addWidget(applyButton);
  buttonLayout->addWidget(deleteButton);
  layout->addLayout(buttonLayout);

  markTableWidget_ = new QTableWidget(0, 8, page);
  markTableWidget_->setHorizontalHeaderLabels(
      {QStringLiteral("名称"), QStringLiteral("算法"), QStringLiteral("形状"), QStringLiteral("预览"),
       QStringLiteral("最低"), QStringLiteral("实拍"), QStringLiteral("中心"), QStringLiteral("状态")});
  markTableWidget_->horizontalHeader()->setStretchLastSection(true);
  markTableWidget_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  markTableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
  markTableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
  markTableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  layout->addWidget(markTableWidget_, 1);

  toolStackedWidget_->addWidget(page);

  connect(drawButton, &QPushButton::clicked, this, [this] { setCanvasMode(CanvasMode::DrawMark); });
  connect(previewButton, &QPushButton::clicked, this, &MainWindow::previewMarkMatching);
  connect(applyButton, &QPushButton::clicked, this, &MainWindow::applyMarkEditorToSelection);
  connect(deleteButton, &QPushButton::clicked, this, &MainWindow::removeSelectedMark);
  connect(markTableWidget_, &QTableWidget::itemSelectionChanged, this, [this] {
    const int row = markTableWidget_->currentRow();
    if (row >= 0) {
      selectMarkIndex(row);
    }
  });
}

void MainWindow::buildTemplatePage() {
  auto *page = new QWidget(toolStackedWidget_);
  auto *layout = new QVBoxLayout(page);

  auto *templateGroupBox = new QGroupBox(QStringLiteral("模版编辑与读码调试"), page);
  auto *templateLayout = new QFormLayout(templateGroupBox);
  templatePreviewValueLabel_ = new QLabel(QStringLiteral("当前未选择任何 Mark / ROI"), templateGroupBox);
  templatePreviewValueLabel_->setWordWrap(true);
  codeRegionLineEdit_ = new QLineEdit(templateGroupBox);
  codeRegionLineEdit_->setPlaceholderText(QStringLiteral("例如 qr_region_01"));
  codeResultValueLabel_ = new QLabel(QStringLiteral("--"), templateGroupBox);
  markPreviewScoreValueLabel_ = new QLabel(QStringLiteral("0.000"), templateGroupBox);
  markLiveScoreValueLabel_ = new QLabel(QStringLiteral("0.000"), templateGroupBox);
  templateLayout->addRow(QStringLiteral("模板说明"), templatePreviewValueLabel_);
  templateLayout->addRow(QStringLiteral("读码区域名"), codeRegionLineEdit_);
  templateLayout->addRow(QStringLiteral("读码结果"), codeResultValueLabel_);
  templateLayout->addRow(QStringLiteral("当前预览分"), markPreviewScoreValueLabel_);
  templateLayout->addRow(QStringLiteral("当前实拍分"), markLiveScoreValueLabel_);
  layout->addWidget(templateGroupBox);

  auto *guideGroupBox = new QGroupBox(QStringLiteral("当前工艺策略"), page);
  auto *guideLayout = new QVBoxLayout(guideGroupBox);
  auto *guideLabel = new QLabel(
      QStringLiteral("1. ROI 样本点页负责矩形/圆形 ROI 的框选、阈值和尺寸维护。\n"
                     "2. Mark 点页负责抽色模板与黑白几何模板切换，并配置最低匹配度。\n"
                     "3. 画笔抽色模式适合字符、Logo、异形特征；黑白几何模式适合圆孔、矩形边框等稳定结构。\n"
                     "4. 左侧画布支持像 CAD 一样拖拽、缩放、框选，并可直接拖动物件调整位置。"),
      guideGroupBox);
  guideLabel->setWordWrap(true);
  guideLayout->addWidget(guideLabel);
  layout->addWidget(guideGroupBox);

  auto *buttonLayout = new QHBoxLayout;
  auto *testCodeButton = new QPushButton(QStringLiteral("测试读码"), page);
  auto *saveButton = new QPushButton(QStringLiteral("保存当前程序"), page);
  buttonLayout->addWidget(testCodeButton);
  buttonLayout->addWidget(saveButton);
  buttonLayout->addStretch();
  layout->addLayout(buttonLayout);
  layout->addStretch();

  toolStackedWidget_->addWidget(page);

  connect(testCodeButton, &QPushButton::clicked, this, &MainWindow::testCodeReading);
  connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveCurrentProgram);
  connect(codeRegionLineEdit_, &QLineEdit::editingFinished, this, &MainWindow::syncProgramFromEditors);
}

void MainWindow::appendLog(const QString &message) {
  if (operationLogTextEdit_ == nullptr) {
    return;
  }

  const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
  operationLogTextEdit_->append(QStringLiteral("[%1] %2").arg(timestamp, message));
  statusBar()->showMessage(message, 5000);
}

void MainWindow::refreshStatusSummary() {
  statusSummaryValueLabel_->setText(
      QStringLiteral("程序：%1 | 相机：%2 | 运控：%3 | 模式：%4")
          .arg(programManager_.currentProgram().has_value()
                   ? QString::fromStdString(programManager_.currentProgram()->name)
                   : QStringLiteral("未加载"))
          .arg(cameraStatusToolbarValueLabel_ != nullptr ? cameraStatusToolbarValueLabel_->text()
                                                         : QStringLiteral("未启动"))
          .arg(motionStateText())
          .arg(canvasMode_ == CanvasMode::DrawMark ? QStringLiteral("框选 Mark")
                                                   : (canvasMode_ == CanvasMode::DrawRoi ? QStringLiteral("框选 ROI")
                                                                                        : QStringLiteral("选择"))));
}

void MainWindow::refreshProgramWidgets() {
  const auto currentProgram = programManager_.currentProgram();
  if (!currentProgram.has_value()) {
    programNameValueLabel_->setText(QStringLiteral("--"));
    programPathValueLabel_->setText(QStringLiteral("--"));
    programAiModelValueLabel_->setText(QStringLiteral("--"));
    markCountValueLabel_->setText(QStringLiteral("0"));
    roiCountValueLabel_->setText(QStringLiteral("0"));
    markTableWidget_->setRowCount(0);
    roiTableWidget_->setRowCount(0);
    codeRegionLineEdit_->clear();
    refreshWorkbenchScene();
    refreshStatusSummary();
    return;
  }

  programNameValueLabel_->setText(QString::fromStdString(currentProgram->name));
  programPathValueLabel_->setText(currentProgram->filePath.empty() ? QStringLiteral("内存中的默认程序")
                                                                   : QString::fromStdString(currentProgram->filePath));
  programAiModelValueLabel_->setText(QString::fromStdString(currentProgram->aiModelPath));
  markCountValueLabel_->setText(QString::number(currentProgram->marks.size()));
  roiCountValueLabel_->setText(QString::number(currentProgram->rois.size()));
  motionStatusValueLabel_->setText(motionStateText());

  {
    const QSignalBlocker blocker(markTableWidget_);
    markTableWidget_->setRowCount(static_cast<int>(currentProgram->marks.size()));
    for (int row = 0; row < static_cast<int>(currentProgram->marks.size()); ++row) {
      const auto &mark = currentProgram->marks[static_cast<std::size_t>(row)];
      markTableWidget_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(mark.name)));
      markTableWidget_->setItem(row, 1, new QTableWidgetItem(markAlgorithmDisplayText(mark.algorithm)));
      markTableWidget_->setItem(row, 2, new QTableWidgetItem(markShapeDisplayText(mark.shape)));
      markTableWidget_->setItem(row, 3, new QTableWidgetItem(QString::number(mark.previewScore, 'f', 3)));
      markTableWidget_->setItem(row, 4, new QTableWidgetItem(QString::number(mark.minimumScore, 'f', 3)));
      markTableWidget_->setItem(row, 5, new QTableWidgetItem(QString::number(mark.score, 'f', 3)));
      markTableWidget_->setItem(
          row, 6,
          new QTableWidgetItem(QStringLiteral("(%1, %2)").arg(mark.x, 0, 'f', 1).arg(mark.y, 0, 'f', 1)));
      markTableWidget_->setItem(row, 7, new QTableWidgetItem(mark.score >= mark.minimumScore ? QStringLiteral("通过")
                                                                                              : QStringLiteral("待调整")));
    }
  }

  {
    const QSignalBlocker blocker(roiTableWidget_);
    roiTableWidget_->setRowCount(static_cast<int>(currentProgram->rois.size()));
    for (int row = 0; row < static_cast<int>(currentProgram->rois.size()); ++row) {
      const auto &roi = currentProgram->rois[static_cast<std::size_t>(row)];
      roiTableWidget_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(roi.name)));
      roiTableWidget_->setItem(row, 1, new QTableWidgetItem(roiShapeDisplayText(roi.shape)));
      roiTableWidget_->setItem(row, 2, new QTableWidgetItem(QString::number(roi.threshold, 'f', 3)));
      roiTableWidget_->setItem(row, 3, new QTableWidgetItem(QString::number(roi.x, 'f', 1)));
      roiTableWidget_->setItem(row, 4, new QTableWidgetItem(QString::number(roi.y, 'f', 1)));
      roiTableWidget_->setItem(
          row, 5,
          new QTableWidgetItem(QStringLiteral("%1 x %2").arg(roi.width, 0, 'f', 1).arg(roi.height, 0, 'f', 1)));
      roiTableWidget_->setItem(row, 6, new QTableWidgetItem(roi.enabled ? QStringLiteral("启用") : QStringLiteral("停用")));
    }
  }

  codeRegionLineEdit_->setText(QString::fromStdString(currentProgram->codeRegionName));
  refreshWorkbenchScene();
  refreshTableSelections();
  updateMarkEditorFromSelection();
  updateRoiEditorFromSelection();
  refreshStatusSummary();
}

void MainWindow::refreshWorkbenchScene(const bool keepView) {
  QPointF sceneCenter;
  if (keepView && workbenchViewInitialized_) {
    sceneCenter = workbenchGraphicsView_->mapToScene(workbenchGraphicsView_->viewport()->rect().center());
  }

  workbenchScene_->clear();

  const QImage image = buildWorkbenchImage(programManager_.currentProgram(), cameraModeText(), lastFrameSize_);
  workbenchPixmapItem_ = workbenchScene_->addPixmap(QPixmap::fromImage(image));
  workbenchPixmapItem_->setPos(0.0, 0.0);
  workbenchScene_->setSceneRect(-120.0, -80.0, image.width() + 240.0, image.height() + 160.0);

  if (const auto currentProgram = programManager_.currentProgram(); currentProgram.has_value()) {
    for (int index = 0; index < static_cast<int>(currentProgram->rois.size()); ++index) {
      const auto &roi = currentProgram->rois[static_cast<std::size_t>(index)];
      auto *item = new ProgramShapeItem(ProgramShapeItem::Domain::Roi, index, QRectF(0.0, 0.0, roi.width, roi.height),
                                        QString::fromStdString(roi.name), QColor("#22c55e"),
                                        MarkShape::Rectangle, roi.shape);
      item->setPos(roi.x, roi.y);
      item->setRotation(roi.rotation);
      item->setSelectionHandler([this](const ProgramShapeItem::Domain, const int itemIndex) { selectRoiIndex(itemIndex); });
      item->setMoveFinishedHandler([this](const ProgramShapeItem::Domain, const int itemIndex, const QPointF &position) {
        selectedRoiIndex_ = itemIndex;
        updateSelectedRoiFromScene(position);
      });
      workbenchScene_->addItem(item);
      if (index == selectedRoiIndex_) {
        item->setSelected(true);
      }
    }

    for (int index = 0; index < static_cast<int>(currentProgram->marks.size()); ++index) {
      const auto &mark = currentProgram->marks[static_cast<std::size_t>(index)];
      auto *item =
          new ProgramShapeItem(ProgramShapeItem::Domain::Mark, index,
                               QRectF(-mark.width / 2.0, -mark.height / 2.0, mark.width, mark.height),
                               QString::fromStdString(mark.name), colorFromMark(mark), mark.shape);
      item->setPos(mark.x, mark.y);
      item->setRotation(mark.rotation);
      item->setSelectionHandler([this](const ProgramShapeItem::Domain, const int itemIndex) { selectMarkIndex(itemIndex); });
      item->setMoveFinishedHandler([this](const ProgramShapeItem::Domain, const int itemIndex, const QPointF &position) {
        selectedMarkIndex_ = itemIndex;
        updateSelectedMarkFromScene(position);
      });
      workbenchScene_->addItem(item);
      if (index == selectedMarkIndex_) {
        item->setSelected(true);
      }
    }
  }

  fovRectItem_ = workbenchScene_->addRect(
      QRectF(560.0, 720.0, std::clamp(static_cast<qreal>(lastFrameSize_.width()), 180.0, 820.0),
             std::clamp(static_cast<qreal>(lastFrameSize_.height()), 120.0, 480.0)),
      QPen(QColor("#facc15"), 3, Qt::DashLine), QBrush(QColor(250, 204, 21, 14)));
  fovRectItem_->setVisible(showFovOverlay_);
  auto *fovText = workbenchScene_->addText(QStringLiteral("实时 FOV"));
  fovText->setDefaultTextColor(QColor("#fef08a"));
  fovText->setPos(fovRectItem_->rect().topLeft() + QPointF(8.0, -24.0));

  if (!workbenchViewInitialized_ || !keepView) {
    workbenchGraphicsView_->fitSceneContent();
    workbenchViewInitialized_ = true;
  } else {
    workbenchGraphicsView_->centerOn(sceneCenter);
  }

  topRulerWidget_->update();
  leftRulerWidget_->update();
}

void MainWindow::refreshCameraState() {
  cameraModeValueLabel_->setText(cameraModeText());
  const QString cameraStatusText = usbCamera_.isOpened() ? QStringLiteral("实时采图中") : QStringLiteral("未启动");
  cameraStatusToolbarValueLabel_->setText(cameraStatusText);
  cameraStatusDetailValueLabel_->setText(cameraStatusText);
  cameraDeviceValueLabel_->setText(QString::number(cameraDeviceIndex_));
  cameraStatusToolbarValueLabel_->setStyleSheet(
      usbCamera_.isOpened() ? QStringLiteral("color: #067647; font-weight: 700;")
                            : QStringLiteral("color: #667085; font-weight: 700;"));
  cameraStatusDetailValueLabel_->setStyleSheet(cameraStatusToolbarValueLabel_->styleSheet());
  fovInfoValueLabel_->setText(QStringLiteral("%1 x %2").arg(lastFrameSize_.width()).arg(lastFrameSize_.height()));
  toggleFovButton_->setText(showFovOverlay_ ? QStringLiteral("隐藏 FOV %1x%2")
                                                  .arg(lastFrameSize_.width())
                                                  .arg(lastFrameSize_.height())
                                            : QStringLiteral("显示 FOV %1x%2")
                                                  .arg(lastFrameSize_.width())
                                                  .arg(lastFrameSize_.height()));
  refreshStatusSummary();
}

void MainWindow::refreshTableSelections() {
  {
    const QSignalBlocker blocker(markTableWidget_);
    if (selectedMarkIndex_ >= 0 && selectedMarkIndex_ < markTableWidget_->rowCount()) {
      markTableWidget_->selectRow(selectedMarkIndex_);
    } else {
      markTableWidget_->clearSelection();
    }
  }

  {
    const QSignalBlocker blocker(roiTableWidget_);
    if (selectedRoiIndex_ >= 0 && selectedRoiIndex_ < roiTableWidget_->rowCount()) {
      roiTableWidget_->selectRow(selectedRoiIndex_);
    } else {
      roiTableWidget_->clearSelection();
    }
  }
}

void MainWindow::syncProgramFromEditors() {
  updateProgram([this](ProgramModel &program) { program.codeRegionName = codeRegionLineEdit_->text().toStdString(); });
}

void MainWindow::updateProgram(const std::function<void(ProgramModel &)> &updater) {
  const auto currentProgram = programManager_.currentProgram();
  if (!currentProgram.has_value()) {
    return;
  }

  ProgramModel updatedProgram = *currentProgram;
  updater(updatedProgram);
  programManager_.createProgram(updatedProgram);
  refreshProgramWidgets();
}

void MainWindow::createDefaultProgram() {
  const auto result = programManager_.createDefaultProgram();
  selectedMarkIndex_ = -1;
  selectedRoiIndex_ = -1;
  if (result) {
    appendLog(QStringLiteral("已新建默认程序。"));
  } else {
    appendLog(QStringLiteral("新建默认程序失败：%1").arg(QString::fromStdString(result.message)));
  }
  refreshProgramWidgets();
}

void MainWindow::openProgram() {
  ProgramEditDialog dialog(this);
  dialog.setProjectRootPath(projectRootPath());
  if (const auto currentProgram = programManager_.currentProgram();
      currentProgram.has_value() && !currentProgram->filePath.empty()) {
    dialog.setSelectedFilePath(QString::fromStdString(currentProgram->filePath));
  }

  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  const auto result = programManager_.loadProgram(dialog.selectedFilePath().toStdString());
  if (result) {
    selectedMarkIndex_ = -1;
    selectedRoiIndex_ = -1;
    appendLog(QStringLiteral("已打开程序：%1").arg(dialog.selectedFilePath()));
  } else {
    appendLog(QStringLiteral("程序打开失败：%1").arg(QString::fromStdString(result.message)));
  }
  refreshProgramWidgets();
}

void MainWindow::saveCurrentProgram() {
  syncProgramFromEditors();

  QString filePath = projectFilePath(QStringLiteral("data/active_demo_program.json"));
  if (const auto currentProgram = programManager_.currentProgram();
      currentProgram.has_value() && !currentProgram->filePath.empty()) {
    filePath = QString::fromStdString(currentProgram->filePath);
  }

  QDir().mkpath(QFileInfo(filePath).absolutePath());
  const auto result = programManager_.saveProgram(filePath.toStdString());
  if (result) {
    updateProgram([filePath](ProgramModel &program) { program.filePath = filePath.toStdString(); });
    appendLog(QStringLiteral("程序已保存：%1").arg(filePath));
  } else {
    appendLog(QStringLiteral("程序保存失败：%1").arg(QString::fromStdString(result.message)));
  }
}

void MainWindow::openCameraConfig() {
  CameraCalibDialog dialog(this);
  dialog.setDeviceIndex(cameraDeviceIndex_);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  cameraDeviceIndex_ = dialog.deviceIndex();
  cameraExposureMs_ = dialog.exposureTimeMs();
  cameraGain_ = dialog.gainValue();
  cameraResolutionPreset_ = dialog.resolutionPreset();
  appendLog(QStringLiteral("相机配置已更新：索引=%1，曝光=%2 ms，增益=%3 dB，分辨率=%4。")
                .arg(cameraDeviceIndex_)
                .arg(cameraExposureMs_, 0, 'f', 1)
                .arg(cameraGain_, 0, 'f', 1)
                .arg(cameraResolutionPreset_));
  refreshCameraState();
}

void MainWindow::openMotionPanel() {
  if (motionControlDialog_ == nullptr) {
    motionControlDialog_ = new MotionControlDialog(&virtualMotionController_, this);
    connect(motionControlDialog_, &MotionControlDialog::motionStateChanged, this, [this] {
      refreshProgramWidgets();
    });
    connect(motionControlDialog_, &MotionControlDialog::motionLogGenerated, this,
            [this](const QString &message) { appendLog(message); });
  }

  motionControlDialog_->show();
  motionControlDialog_->raise();
  motionControlDialog_->activateWindow();
}

void MainWindow::startCameraPreview() {
  if (usbCamera_.isOpened()) {
    return;
  }

  if (!usbCamera_.open(cameraDeviceIndex_)) {
    appendLog(QStringLiteral("相机启动失败，索引=%1。").arg(cameraDeviceIndex_));
    refreshCameraState();
    return;
  }

  cameraTimer_->start();
  appendLog(QStringLiteral("实时采图已启动，设备索引=%1。").arg(cameraDeviceIndex_));
  refreshCameraState();
  updateCameraFrame();
}

void MainWindow::stopCameraPreview() {
  if (!usbCamera_.isOpened()) {
    refreshCameraState();
    return;
  }

  cameraTimer_->stop();
  usbCamera_.close();
  appendLog(QStringLiteral("实时采图已停止。"));
  refreshCameraState();
  refreshWorkbenchScene();
}

void MainWindow::updateCameraFrame() {
  const CameraFrame frame = usbCamera_.grabFrame();
  if (frame.width <= 0 || frame.height <= 0) {
    return;
  }

  lastFrameSize_ = QSize(frame.width, frame.height);
  refreshCameraState();

  if (const auto currentProgram = programManager_.currentProgram();
      currentProgram.has_value() && selectedMarkIndex_ >= 0 &&
      selectedMarkIndex_ < static_cast<int>(currentProgram->marks.size())) {
    const auto &mark = currentProgram->marks[static_cast<std::size_t>(selectedMarkIndex_)];
    const double liveScore = calculateLiveScore(mark, lastFrameSize_);
    markLiveScoreSpinBox_->setValue(liveScore);
    markLiveScoreValueLabel_->setText(QString::number(liveScore, 'f', 3));
    markLiveScoreValueLabel_->setStyleSheet(
        liveScore >= mark.minimumScore ? QStringLiteral("color: #16a34a; font-weight: 700;")
                                       : QStringLiteral("color: #dc2626; font-weight: 700;"));
  }

  refreshWorkbenchScene();
}

void MainWindow::toggleFovOverlay() {
  showFovOverlay_ = !showFovOverlay_;
  refreshCameraState();
  refreshWorkbenchScene();
}

void MainWindow::resetWorkbenchView() {
  workbenchGraphicsView_->fitSceneContent();
  appendLog(QStringLiteral("画布已重置到整板适配视图。"));
}

void MainWindow::setCanvasMode(const CanvasMode mode) {
  canvasMode_ = mode;
  workbenchGraphicsView_->setDrawingEnabled(mode != CanvasMode::Select);
  appendLog(mode == CanvasMode::Select ? QStringLiteral("已切换到选择模式。")
                                       : (mode == CanvasMode::DrawRoi ? QStringLiteral("已切换到 ROI 框选模式。")
                                                                      : QStringLiteral("已切换到 Mark 框选模式。")));
  refreshStatusSummary();
}

void MainWindow::setCurrentPage(const int index) {
  if (toolStackedWidget_ != nullptr && index >= 0 && index < toolStackedWidget_->count()) {
    toolStackedWidget_->setCurrentIndex(index);
  }

  if (toolSelectorListWidget_ != nullptr && toolSelectorListWidget_->currentRow() != index) {
    toolSelectorListWidget_->setCurrentRow(index);
  }
}

void MainWindow::handleDrawnRegion(const QRectF &sceneRect) {
  if (canvasMode_ == CanvasMode::DrawMark) {
    addMarkFromSceneRect(sceneRect);
    return;
  }

  if (canvasMode_ == CanvasMode::DrawRoi) {
    addRoiFromSceneRect(sceneRect);
  }
}

void MainWindow::addMarkFromSceneRect(const QRectF &sceneRect) {
  updateProgram([this, sceneRect](ProgramModel &program) {
    MarkPoint mark;
    mark.name = markNameLineEdit_->text().isEmpty() ? QStringLiteral("Mark-%1").arg(program.marks.size() + 1).toStdString()
                                                    : markNameLineEdit_->text().toStdString();
    mark.shape = markShapeFromDisplayText(currentMarkShapeText());
    mark.algorithm = markAlgorithmFromDisplayText(currentMarkAlgorithmText());
    mark.sampledColor = markColorLineEdit_->text().toStdString();
    mark.minimumScore = markMinScoreSpinBox_->value();
    mark.width = sceneRect.width();
    mark.height = sceneRect.height();
    mark.rotation = markRotationSpinBox_->value();
    mark.x = sceneRect.center().x();
    mark.y = sceneRect.center().y();
    mark.previewScore = calculatePreviewScore(mark);
    mark.score = calculateLiveScore(mark, lastFrameSize_);
    program.marks.push_back(mark);
    selectedMarkIndex_ = static_cast<int>(program.marks.size()) - 1;
    selectedRoiIndex_ = -1;
  });

  setCurrentPage(1);
  appendLog(QStringLiteral("已通过框选区域生成一个 Mark 模板。"));
}

void MainWindow::addRoiFromSceneRect(const QRectF &sceneRect) {
  updateProgram([this, sceneRect](ProgramModel &program) {
    RoiRegion roi;
    roi.name = roiNameLineEdit_->text().isEmpty() ? QStringLiteral("ROI-%1").arg(program.rois.size() + 1).toStdString()
                                                  : roiNameLineEdit_->text().toStdString();
    roi.shape = roiShapeFromDisplayText(currentRoiShapeText());
    roi.threshold = roiThresholdSpinBox_->value();
    roi.rotation = roiRotationSpinBox_->value();
    roi.x = sceneRect.left();
    roi.y = sceneRect.top();
    roi.width = sceneRect.width();
    roi.height = sceneRect.height();
    program.rois.push_back(roi);
    selectedRoiIndex_ = static_cast<int>(program.rois.size()) - 1;
    selectedMarkIndex_ = -1;
  });

  setCurrentPage(0);
  appendLog(QStringLiteral("已通过框选区域生成一个 ROI 样本点。"));
}

void MainWindow::selectMarkIndex(const int index) {
  selectedMarkIndex_ = index;
  selectedRoiIndex_ = -1;
  refreshTableSelections();
  updateMarkEditorFromSelection();
  refreshWorkbenchScene();
}

void MainWindow::selectRoiIndex(const int index) {
  selectedRoiIndex_ = index;
  selectedMarkIndex_ = -1;
  refreshTableSelections();
  updateRoiEditorFromSelection();
  refreshWorkbenchScene();
}

void MainWindow::applyMarkEditorToSelection() {
  if (selectedMarkIndex_ < 0) {
    return;
  }

  updateProgram([this](ProgramModel &program) {
    if (selectedMarkIndex_ >= static_cast<int>(program.marks.size())) {
      return;
    }

    auto &mark = program.marks[static_cast<std::size_t>(selectedMarkIndex_)];
    mark.name = markNameLineEdit_->text().toStdString();
    mark.shape = markShapeFromDisplayText(currentMarkShapeText());
    mark.algorithm = markAlgorithmFromDisplayText(currentMarkAlgorithmText());
    mark.sampledColor = markColorLineEdit_->text().toStdString();
    mark.minimumScore = markMinScoreSpinBox_->value();
    mark.width = markWidthSpinBox_->value();
    mark.height = markHeightSpinBox_->value();
    mark.rotation = markRotationSpinBox_->value();
    mark.previewScore = calculatePreviewScore(mark);
    mark.score = calculateLiveScore(mark, lastFrameSize_);
  });

  appendLog(QStringLiteral("选中的 Mark 参数已更新。"));
}

void MainWindow::applyRoiEditorToSelection() {
  if (selectedRoiIndex_ < 0) {
    return;
  }

  updateProgram([this](ProgramModel &program) {
    if (selectedRoiIndex_ >= static_cast<int>(program.rois.size())) {
      return;
    }

    auto &roi = program.rois[static_cast<std::size_t>(selectedRoiIndex_)];
    roi.name = roiNameLineEdit_->text().toStdString();
    roi.shape = roiShapeFromDisplayText(currentRoiShapeText());
    roi.threshold = roiThresholdSpinBox_->value();
    roi.width = roiWidthSpinBox_->value();
    roi.height = roiHeightSpinBox_->value();
    roi.rotation = roiRotationSpinBox_->value();
  });

  appendLog(QStringLiteral("选中的 ROI 参数已更新。"));
}

void MainWindow::removeSelectedMark() {
  if (selectedMarkIndex_ < 0) {
    return;
  }

  const int currentIndex = selectedMarkIndex_;
  selectedMarkIndex_ = -1;
  updateProgram([currentIndex](ProgramModel &program) {
    if (currentIndex < static_cast<int>(program.marks.size())) {
      program.marks.erase(program.marks.begin() + currentIndex);
    }
  });
  appendLog(QStringLiteral("已删除选中的 Mark 点。"));
}

void MainWindow::removeSelectedRoi() {
  if (selectedRoiIndex_ < 0) {
    return;
  }

  const int currentIndex = selectedRoiIndex_;
  selectedRoiIndex_ = -1;
  updateProgram([currentIndex](ProgramModel &program) {
    if (currentIndex < static_cast<int>(program.rois.size())) {
      program.rois.erase(program.rois.begin() + currentIndex);
    }
  });
  appendLog(QStringLiteral("已删除选中的 ROI。"));
}

void MainWindow::previewMarkMatching() {
  if (selectedMarkIndex_ < 0) {
    return;
  }

  updateProgram([this](ProgramModel &program) {
    if (selectedMarkIndex_ >= static_cast<int>(program.marks.size())) {
      return;
    }

    auto &mark = program.marks[static_cast<std::size_t>(selectedMarkIndex_)];
    mark.name = markNameLineEdit_->text().toStdString();
    mark.shape = markShapeFromDisplayText(currentMarkShapeText());
    mark.algorithm = markAlgorithmFromDisplayText(currentMarkAlgorithmText());
    mark.sampledColor = markColorLineEdit_->text().toStdString();
    mark.minimumScore = markMinScoreSpinBox_->value();
    mark.width = markWidthSpinBox_->value();
    mark.height = markHeightSpinBox_->value();
    mark.rotation = markRotationSpinBox_->value();
    mark.previewScore = calculatePreviewScore(mark);
    mark.score = calculateLiveScore(mark, lastFrameSize_);
  });

  if (const auto currentProgram = programManager_.currentProgram();
      currentProgram.has_value() && selectedMarkIndex_ >= 0 &&
      selectedMarkIndex_ < static_cast<int>(currentProgram->marks.size())) {
    const auto &mark = currentProgram->marks[static_cast<std::size_t>(selectedMarkIndex_)];
    markPreviewScoreSpinBox_->setValue(mark.previewScore);
    markLiveScoreSpinBox_->setValue(mark.score);
    markPreviewProgressBar_->setValue(static_cast<int>(std::round(mark.previewScore * 100.0)));
    markPreviewScoreValueLabel_->setText(QString::number(mark.previewScore, 'f', 3));
    markLiveScoreValueLabel_->setText(QString::number(mark.score, 'f', 3));
    const bool pass = mark.score >= mark.minimumScore;
    markLiveScoreValueLabel_->setStyleSheet(pass ? QStringLiteral("color: #16a34a; font-weight: 700;")
                                                 : QStringLiteral("color: #dc2626; font-weight: 700;"));
    templatePreviewValueLabel_->setText(
        QStringLiteral("当前 Mark：%1\n算法：%2\n预览匹配度：%3\n实拍匹配度：%4\n阈值判断：%5")
            .arg(QString::fromStdString(mark.name))
            .arg(markAlgorithmDisplayText(mark.algorithm))
            .arg(mark.previewScore, 0, 'f', 3)
            .arg(mark.score, 0, 'f', 3)
            .arg(pass ? QStringLiteral("通过") : QStringLiteral("低于阈值，需要调整")));
    appendLog(QStringLiteral("Mark 匹配度预览完成：%1，实拍匹配=%2，最低阈值=%3。")
                  .arg(QString::fromStdString(mark.name))
                  .arg(mark.score, 0, 'f', 3)
                  .arg(mark.minimumScore, 0, 'f', 3));
  }
}

void MainWindow::testCodeReading() {
  syncProgramFromEditors();
  CodeReader reader;
  const auto result = reader.readQrCode(projectFilePath(QStringLiteral("tests/data/demo.png")).toStdString());
  codeResultValueLabel_->setText(result ? QString::fromStdString(result.value) : QStringLiteral("读码失败"));
  appendLog(result ? QStringLiteral("读码测试完成，结果=%1").arg(QString::fromStdString(result.value))
                   : QStringLiteral("读码测试失败：%1").arg(QString::fromStdString(result.message)));
}

void MainWindow::centerOnCurrentSelection() {
  if (const auto currentProgram = programManager_.currentProgram(); currentProgram.has_value()) {
    if (selectedMarkIndex_ >= 0 && selectedMarkIndex_ < static_cast<int>(currentProgram->marks.size())) {
      const auto &mark = currentProgram->marks[static_cast<std::size_t>(selectedMarkIndex_)];
      workbenchGraphicsView_->centerOn(mark.x, mark.y);
      return;
    }

    if (selectedRoiIndex_ >= 0 && selectedRoiIndex_ < static_cast<int>(currentProgram->rois.size())) {
      const auto &roi = currentProgram->rois[static_cast<std::size_t>(selectedRoiIndex_)];
      workbenchGraphicsView_->centerOn(roi.x + roi.width / 2.0, roi.y + roi.height / 2.0);
    }
  }
}

void MainWindow::updateCursorCoordinate(const QPointF &scenePos) {
  cursorPositionValueLabel_->setText(QStringLiteral("X=%1 Y=%2").arg(scenePos.x(), 0, 'f', 1).arg(scenePos.y(), 0, 'f', 1));
  topRulerWidget_->setCursorScenePosition(scenePos);
  leftRulerWidget_->setCursorScenePosition(scenePos);
}

void MainWindow::updateMarkEditorFromSelection() {
  const auto currentProgram = programManager_.currentProgram();
  if (!currentProgram.has_value() || selectedMarkIndex_ < 0 ||
      selectedMarkIndex_ >= static_cast<int>(currentProgram->marks.size())) {
    return;
  }

  const auto &mark = currentProgram->marks[static_cast<std::size_t>(selectedMarkIndex_)];
  markNameLineEdit_->setText(QString::fromStdString(mark.name));
  markColorLineEdit_->setText(QString::fromStdString(mark.sampledColor));
  markShapeComboBox_->setCurrentText(markShapeDisplayText(mark.shape));
  markAlgorithmComboBox_->setCurrentText(markAlgorithmDisplayText(mark.algorithm));
  markMinScoreSpinBox_->setValue(mark.minimumScore);
  markWidthSpinBox_->setValue(mark.width);
  markHeightSpinBox_->setValue(mark.height);
  markRotationSpinBox_->setValue(mark.rotation);
  markPreviewScoreSpinBox_->setValue(mark.previewScore);
  markLiveScoreSpinBox_->setValue(mark.score);
  markPreviewProgressBar_->setValue(static_cast<int>(std::round(mark.previewScore * 100.0)));
  markPreviewScoreValueLabel_->setText(QString::number(mark.previewScore, 'f', 3));
  markLiveScoreValueLabel_->setText(QString::number(mark.score, 'f', 3));
}

void MainWindow::updateRoiEditorFromSelection() {
  const auto currentProgram = programManager_.currentProgram();
  if (!currentProgram.has_value() || selectedRoiIndex_ < 0 ||
      selectedRoiIndex_ >= static_cast<int>(currentProgram->rois.size())) {
    return;
  }

  const auto &roi = currentProgram->rois[static_cast<std::size_t>(selectedRoiIndex_)];
  roiNameLineEdit_->setText(QString::fromStdString(roi.name));
  roiShapeComboBox_->setCurrentText(roiShapeDisplayText(roi.shape));
  roiThresholdSpinBox_->setValue(roi.threshold);
  roiWidthSpinBox_->setValue(roi.width);
  roiHeightSpinBox_->setValue(roi.height);
  roiRotationSpinBox_->setValue(roi.rotation);
}

void MainWindow::updateSelectedMarkFromScene(const QPointF &centerScenePos) {
  updateProgram([this, centerScenePos](ProgramModel &program) {
    if (selectedMarkIndex_ >= 0 && selectedMarkIndex_ < static_cast<int>(program.marks.size())) {
      auto &mark = program.marks[static_cast<std::size_t>(selectedMarkIndex_)];
      mark.x = centerScenePos.x();
      mark.y = centerScenePos.y();
    }
  });
  appendLog(QStringLiteral("Mark 位置已更新到 X=%1, Y=%2。").arg(centerScenePos.x(), 0, 'f', 1).arg(centerScenePos.y(), 0, 'f', 1));
}

void MainWindow::updateSelectedRoiFromScene(const QPointF &topLeftScenePos) {
  updateProgram([this, topLeftScenePos](ProgramModel &program) {
    if (selectedRoiIndex_ >= 0 && selectedRoiIndex_ < static_cast<int>(program.rois.size())) {
      auto &roi = program.rois[static_cast<std::size_t>(selectedRoiIndex_)];
      roi.x = topLeftScenePos.x();
      roi.y = topLeftScenePos.y();
    }
  });
  appendLog(QStringLiteral("ROI 位置已更新到 X=%1, Y=%2。").arg(topLeftScenePos.x(), 0, 'f', 1).arg(topLeftScenePos.y(), 0, 'f', 1));
}

QString MainWindow::projectRootPath() const {
  const QString applicationDir = QCoreApplication::applicationDirPath();
  const QStringList candidates = {
      QDir::currentPath(),
      applicationDir,
      QDir(applicationDir).absoluteFilePath(QStringLiteral("..")),
      QDir(applicationDir).absoluteFilePath(QStringLiteral("../..")),
      QDir(applicationDir).absoluteFilePath(QStringLiteral("../../..")),
  };

  for (const QString &candidate : candidates) {
    if (QFileInfo(QDir(candidate).filePath(QStringLiteral("config/default_program.json"))).exists()) {
      return QDir(candidate).absolutePath();
    }
  }

  return QDir::currentPath();
}

QString MainWindow::projectFilePath(const QString &relativePath) const {
  return QDir(projectRootPath()).filePath(relativePath);
}

QString MainWindow::cameraModeText() const {
#ifdef AOI_HAS_OPENCV
  return QStringLiteral("真实摄像头 + CAD 叠加");
#else
  return QStringLiteral("模拟采图 + CAD 叠加");
#endif
}

QString MainWindow::motionStateText() const {
  return virtualMotionController_.isStopped() ? QStringLiteral("急停锁定") : QStringLiteral("运行就绪");
}

QString MainWindow::currentMarkShapeText() const {
  return markShapeComboBox_ != nullptr ? markShapeComboBox_->currentText() : QStringLiteral("矩形");
}

QString MainWindow::currentMarkAlgorithmText() const {
  return markAlgorithmComboBox_ != nullptr ? markAlgorithmComboBox_->currentText() : QStringLiteral("画笔抽色");
}

QString MainWindow::currentRoiShapeText() const {
  return roiShapeComboBox_ != nullptr ? roiShapeComboBox_->currentText() : QStringLiteral("矩形");
}

#endif

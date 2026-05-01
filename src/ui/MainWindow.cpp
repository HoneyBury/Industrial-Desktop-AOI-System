#include "ui/MainWindow.h"

#ifdef AOI_HAS_QT_WIDGETS

#include "boardscan/BoardScanExecutor.h"
#include "boardscan/BoardScanPlanner.h"
#include "boardscan/BoardStitcher.h"
#include "boardscan/BoardViewTransform.h"
#include "config/AppSettings.h"
#include "ui/LaserOffsetCalibDialog.h"
#include "ui/CadGraphicsView.h"
#include "ui/CadRulerWidget.h"
#include "ui/CameraCalibDialog.h"
#include "ui/DataCollectDialog.h"
#include "ui/LogWindow.h"
#include "ui/MarkEditDialog.h"
#include "ui/MarkOffsetDialog.h"
#include "ui/MotionControlDialog.h"
#include "ui/NewProgramDialog.h"
#include "ui/OriginCalibDialog.h"
#include "ui/ProgramEditDialog.h"
#include "ui/RunModeWidget.h"
#include "ui/SettingsDialog.h"
#include "ui/ProductionHistoryDialog.h"
#include "spc/LaserSpcBridge.h"
#include "spc/LaserSpcWindow.h"
#include "SpcWriteManager.h"
#include "infrastructure/AppConfigService.h"
#include "ui_MainWindow.h"

#include "vision/CodeReader.h"
#include "vision/CoordinateTransformer.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include <QAction>
#include <QCheckBox>
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
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLinearGradient>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
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

BoardSceneRect workbenchBoardAreaRect() {
  return BoardSceneRect {180.0, 120.0, 1320.0, 880.0};
}

QRectF toQRectF(const BoardSceneRect &rect) { return QRectF(rect.x, rect.y, rect.width, rect.height); }

BoardSceneRect toBoardSceneRect(const QRectF &rect) { return BoardSceneRect {rect.x(), rect.y(), rect.width(), rect.height()}; }

BoardScenePoint toBoardScenePoint(const QPointF &point) { return BoardScenePoint {point.x(), point.y()}; }

std::optional<BoardSceneLayout> currentBoardSceneLayout(const std::optional<ProgramModel> &program) {
  if (!program.has_value()) {
    return std::nullopt;
  }

  const auto layout = BoardViewTransform::computeLayout(program->boardDefinition, workbenchBoardAreaRect());
  if (!layout.valid) {
    return std::nullopt;
  }

  return layout;
}

MechanicalPose defaultBoardReadyOriginPose(const BoardDefinition &boardDefinition) {
  return MechanicalPose {-boardDefinition.boardLengthMm, -boardDefinition.boardWidthMm, 0.0, 0.0};
}

MechanicalPose boardScanOriginPose(const ProgramModel &program, const MechanicalPose &fallbackPose) {
  if (program.runtimeSummary.hasOriginCalibration) {
    return program.runtimeSummary.originCorrectedPose;
  }
  if (program.originCalibration.calibrated) {
    return program.originCalibration.machineReferencePose;
  }
  return fallbackPose;
}

MechanicalPose logicalBoardReadyOriginPose(const ProgramModel &program) {
  return boardScanOriginPose(program, defaultBoardReadyOriginPose(program.boardDefinition));
}

QImage placeholderScanTile(const QSize &fallbackSize, const FovCapturePose &pose) {
  const QSize tileSize = fallbackSize.isValid() ? fallbackSize : QSize(640, 360);
  QImage tile(tileSize, QImage::Format_ARGB32_Premultiplied);
  tile.fill(QColor("#0f172a"));

  QPainter painter(&tile);
  painter.setRenderHint(QPainter::Antialiasing, true);
  QLinearGradient background(0.0, 0.0, tile.width(), tile.height());
  background.setColorAt(0.0, QColor("#0f172a"));
  background.setColorAt(1.0, QColor("#1d4ed8"));
  painter.fillRect(tile.rect(), background);
  painter.fillRect(tile.rect(), QColor(15, 23, 42, 170));
  painter.setPen(QPen(QColor("#1e3a5f"), 1));
  for (int x = 0; x < tile.width(); x += 40) {
    painter.drawLine(x, 0, x, tile.height());
  }
  for (int y = 0; y < tile.height(); y += 40) {
    painter.drawLine(0, y, tile.width(), y);
  }
  painter.setPen(QPen(QColor("#38bdf8"), 3));
  painter.drawRect(tile.rect().adjusted(8, 8, -8, -8));
  painter.setPen(QColor("#e2e8f0"));
  painter.drawText(QRect(24, 24, tile.width() - 48, tile.height() - 48),
                   Qt::AlignLeft | Qt::TextWordWrap,
                   QStringLiteral("虚拟 FOV\n行=%1 列=%2\n产品坐标=(%3, %4) mm\n机械坐标=(%5, %6) mm")
                       .arg(pose.row)
                       .arg(pose.column)
                       .arg(pose.productCenterMm.x, 0, 'f', 2)
                       .arg(pose.productCenterMm.y, 0, 'f', 2)
                       .arg(pose.machinePose.x, 0, 'f', 2)
                       .arg(pose.machinePose.y, 0, 'f', 2));
  return tile;
}

QImage annotateScanTile(const QImage &source, const FovCapturePose &pose) {
  QImage tile = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  QPainter painter(&tile);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.fillRect(QRect(16, 16, 320, 96), QColor(2, 6, 23, 180));
  painter.setPen(QPen(QColor("#38bdf8"), 3));
  painter.drawRect(tile.rect().adjusted(10, 10, -10, -10));
  painter.setPen(QColor("#e2e8f0"));
  painter.drawText(QRect(32, 32, 280, 72),
                   Qt::AlignLeft | Qt::TextWordWrap,
                   QStringLiteral("FOV r%1 c%2\nX=%3  Y=%4")
                       .arg(pose.row)
                       .arg(pose.column)
                       .arg(pose.machinePose.x, 0, 'f', 2)
                       .arg(pose.machinePose.y, 0, 'f', 2));
  return tile;
}

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

QImage cameraFrameToQImage(const CameraFrame &frame) {
  if (frame.width <= 0 || frame.height <= 0 || frame.data.empty()) {
    return {};
  }

  switch (frame.pixelFormat) {
  case CameraPixelFormat::Rgb24: {
    QImage image(frame.data.data(), frame.width, frame.height, frame.width * 3, QImage::Format_RGB888);
    return image.copy();
  }
  case CameraPixelFormat::Bgr24: {
    QImage image(frame.data.data(), frame.width, frame.height, frame.width * 3, QImage::Format_BGR888);
    return image.copy();
  }
  case CameraPixelFormat::Gray8: {
    QImage image(frame.data.data(), frame.width, frame.height, frame.width, QImage::Format_Grayscale8);
    return image.copy();
  }
  }

  return {};
}

QString persistFrameToTempFile(const QImage &image, const QString &prefix) {
  if (image.isNull()) {
    return {};
  }

  const QString path =
      QDir::temp().filePath(QStringLiteral("%1_%2.png").arg(prefix).arg(QDateTime::currentMSecsSinceEpoch()));
  return image.save(path) ? path : QString();
}

QRectF workbenchFovRect(const QSize &frameSize) {
  return QRectF(560.0, 720.0, std::clamp(static_cast<qreal>(frameSize.width()), 180.0, 820.0),
                std::clamp(static_cast<qreal>(frameSize.height()), 120.0, 480.0));
}

QSize parseResolutionPreset(const QString &preset) {
  const QStringList parts = preset.split('x', Qt::SkipEmptyParts);
  if (parts.size() != 2) {
    return {1280, 720};
  }

  bool widthOk = false;
  bool heightOk = false;
  const int width = parts[0].trimmed().toInt(&widthOk);
  const int height = parts[1].trimmed().toInt(&heightOk);
  if (!widthOk || !heightOk || width <= 0 || height <= 0) {
    return {1280, 720};
  }

  return {width, height};
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

  const QRectF boardAreaRect = toQRectF(workbenchBoardAreaRect());
  QRectF boardRect = boardAreaRect.adjusted(80.0, 60.0, -80.0, -60.0);
  double railHeight = 44.0;
  double boardLengthMm = 0.0;
  double boardWidthMm = 0.0;
  double railWidthMm = 0.0;
  QString scanOrderText = QStringLiteral("从左到右");
  if (const auto layout = currentBoardSceneLayout(program); layout.has_value()) {
    boardLengthMm = program->boardDefinition.boardLengthMm;
    boardWidthMm = program->boardDefinition.boardWidthMm;
    railWidthMm = program->boardDefinition.railWidthMm;
    scanOrderText = program->scanRecipe.scanOrder == ScanOrder::TopToBottom
                        ? QStringLiteral("从上到下")
                        : QStringLiteral("从左到右");
    boardRect = toQRectF(layout->boardRect);
    railHeight = layout->railHeightPixels;
  }

  painter.setBrush(QColor(14, 116, 144, 28));
  painter.setPen(QPen(QColor("#0ea5e9"), 2));
  painter.drawRoundedRect(boardAreaRect, 18.0, 18.0);
  painter.drawText(QRectF(210.0, 136.0, 380.0, 28.0), QStringLiteral("整板拼接区 / 扫描工作台"));

  painter.setBrush(QColor(71, 85, 105, 80));
  painter.setPen(Qt::NoPen);
  painter.drawRoundedRect(QRectF(boardRect.left(), boardRect.top() - railHeight - 18.0, boardRect.width(), railHeight), 10.0, 10.0);
  painter.drawRoundedRect(QRectF(boardRect.left(), boardRect.bottom() + 18.0, boardRect.width(), railHeight), 10.0, 10.0);

  painter.setBrush(QColor(15, 23, 42, 160));
  painter.setPen(QPen(QColor("#38bdf8"), 3));
  painter.drawRoundedRect(boardRect, 14.0, 14.0);
  const QString boardImagePath =
      program.has_value() ? QString::fromStdString(program->runtimeSummary.wholeBoardImagePath) : QString();
  if (!boardImagePath.isEmpty()) {
    const QImage boardImage(boardImagePath);
    if (!boardImage.isNull()) {
      painter.save();
      painter.setClipRect(boardRect.adjusted(4.0, 4.0, -4.0, -4.0));
      painter.drawImage(boardRect, boardImage);
      painter.restore();
    }
  }

  if (program.has_value() && program->runtimeSummary.scanTileRows > 0 && program->runtimeSummary.scanTileColumns > 0) {
    painter.setPen(QPen(QColor(250, 204, 21, 120), 1, Qt::DashLine));
    const double tileWidth = boardRect.width() / static_cast<double>(program->runtimeSummary.scanTileColumns);
    const double tileHeight = boardRect.height() / static_cast<double>(program->runtimeSummary.scanTileRows);
    for (int column = 1; column < program->runtimeSummary.scanTileColumns; ++column) {
      const double x = boardRect.left() + tileWidth * static_cast<double>(column);
      painter.drawLine(QPointF(x, boardRect.top()), QPointF(x, boardRect.bottom()));
    }
    for (int row = 1; row < program->runtimeSummary.scanTileRows; ++row) {
      const double y = boardRect.top() + tileHeight * static_cast<double>(row);
      painter.drawLine(QPointF(boardRect.left(), y), QPointF(boardRect.right(), y));
    }
  }

  painter.setPen(QColor("#e2e8f0"));
  painter.drawText(boardRect.adjusted(20.0, 16.0, -20.0, -20.0),
                   QStringLiteral("板轮廓\n长=%1 mm 宽=%2 mm\n轨道=%3 mm\n扫描顺序=%4\n扫描网格=%5 x %6")
                       .arg(boardLengthMm, 0, 'f', 1)
                       .arg(boardWidthMm, 0, 'f', 1)
                       .arg(railWidthMm, 0, 'f', 1)
                       .arg(scanOrderText)
                       .arg(program.has_value() ? program->runtimeSummary.scanTileRows : 0)
                       .arg(program.has_value() ? program->runtimeSummary.scanTileColumns : 0));

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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui_(new Ui::MainWindow),
      virtualMotionSystem_(virtualMotionController_, virtualTransportController_) {
  ui_->setupUi(this);
  cameraTimer_ = new QTimer(this);
  cameraTimer_->setInterval(90);
  connect(cameraTimer_, &QTimer::timeout, this, &MainWindow::updateCameraFrame);

  simulationTimer_ = new QTimer(this);
  simulationTimer_->setInterval(16);
  connect(simulationTimer_, &QTimer::timeout, this, &MainWindow::tickVirtualDevices);

  workflowTimer_ = new QTimer(this);
  workflowTimer_->setInterval(600);
  connect(workflowTimer_, &QTimer::timeout, this, &MainWindow::advanceWorkflowStep);

  simulationTimer_->start();

  appSettings_ = AppSettingsManager::load(projectFilePath(QStringLiteral("config/app_settings.json")).toStdString());
  const QString runtimeDatabasePath = projectFilePath(QStringLiteral("data/aoi_runtime.db"));
  QDir().mkpath(QFileInfo(runtimeDatabasePath).absolutePath());
  const auto databaseOpenResult = databaseManager_.open(runtimeDatabasePath.toStdString());

  logWindow_ = new LogWindow();
  if (appSettings_.persistLogs) {
    logWindow_->setPersistEnabled(true, QString::fromStdString(appSettings_.logFilePath));
  }

  buildMenus();
  buildCentralUi();

  // Initialize SPC write manager and bridge for laser-data submission.
  {
    LaserSpc::Infrastructure::AppConfigService spcConfig;
    auto spcSettings = spcConfig.settings();
    spcSettings.useMySql = true;
    spcSettings.allowMockFallback = true;
    spcWriteManager_ = new HostSpc::SpcWriteManager(spcSettings, this);
    spcBridge_ = new LaserSpcBridge(this);
    spcBridge_->setWriteManager(spcWriteManager_);
  }

  createDefaultProgram();
  refreshCameraState();
  refreshStatusSummary();

  if (appSettings_.showLogWindow) {
    logWindow_->show();
  }

  appendLog(QStringLiteral("主界面已切换为 CAD 式预览工位布局。"));
  appendLog(QStringLiteral("左侧支持中键拖拽、滚轮缩放、左键框选生成 Mark/ROI。"));
  appendLog(QStringLiteral("可通过右上角按钮切换至运行模式。"));
  appendLog(databaseOpenResult
                ? QStringLiteral("运行追溯数据库已就绪：%1").arg(runtimeDatabasePath)
                : QStringLiteral("运行追溯数据库打开失败：%1").arg(QString::fromStdString(databaseOpenResult.message)));
}

MainWindow::~MainWindow() {
  stopCameraPreview();
  stopWorkflowRun();
  delete spcWindow_;
  databaseManager_.close();
  delete ui_;
}

void MainWindow::buildMenus() {
  auto *programMenu = menuBar()->addMenu(QStringLiteral("程序"));
  auto *newProgramAction = programMenu->addAction(QStringLiteral("新建程序"));
  auto *openProgramAction = programMenu->addAction(QStringLiteral("打开程序"));
  auto *saveProgramAction = programMenu->addAction(QStringLiteral("保存程序"));
  programMenu->addSeparator();
  auto *settingsAction = programMenu->addAction(QStringLiteral("系统设置..."));
  programMenu->addSeparator();
  auto *exitAction = programMenu->addAction(QStringLiteral("退出"));

  auto *cameraMenu = menuBar()->addMenu(QStringLiteral("相机"));
  auto *cameraConfigAction = cameraMenu->addAction(QStringLiteral("相机配置"));
  auto *startPreviewAction = cameraMenu->addAction(QStringLiteral("开始预览"));
  auto *stopPreviewAction = cameraMenu->addAction(QStringLiteral("停止预览"));

  auto *motionMenu = menuBar()->addMenu(QStringLiteral("运控"));
  auto *openMotionAction = motionMenu->addAction(QStringLiteral("打开虚拟运控面板"));
  auto *loadBoardAction = motionMenu->addAction(QStringLiteral("进板"));
  auto *unloadBoardAction = motionMenu->addAction(QStringLiteral("出板"));

  auto *calibrationMenu = menuBar()->addMenu(QStringLiteral("校正"));
  auto *markOffsetAction = calibrationMenu->addAction(QStringLiteral("Mark 点校正"));
  auto *originCalibAction = calibrationMenu->addAction(QStringLiteral("机械原点校正"));
  auto *laserOffsetAction = calibrationMenu->addAction(QStringLiteral("激光偏移校正"));

  auto *viewMenu = menuBar()->addMenu(QStringLiteral("视图"));
  auto *openLogAction = viewMenu->addAction(QStringLiteral("运行日志"));

  auto *toolsMenu = menuBar()->addMenu(QStringLiteral("工具"));
  auto *dataCollectAction = toolsMenu->addAction(QStringLiteral("数据集采集"));
  auto *markEditAction = toolsMenu->addAction(QStringLiteral("Mark 点编辑器"));
  auto *wholeBoardScanAction = toolsMenu->addAction(QStringLiteral("执行整板扫描"));

  connect(newProgramAction, &QAction::triggered, this, &MainWindow::createDefaultProgram);
  connect(openProgramAction, &QAction::triggered, this, &MainWindow::openProgram);
  connect(saveProgramAction, &QAction::triggered, this, &MainWindow::saveCurrentProgram);
  connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettings);
  connect(exitAction, &QAction::triggered, this, &QWidget::close);
  connect(cameraConfigAction, &QAction::triggered, this, &MainWindow::openCameraConfig);
  connect(startPreviewAction, &QAction::triggered, this, &MainWindow::startCameraPreview);
  connect(stopPreviewAction, &QAction::triggered, this, &MainWindow::stopCameraPreview);
  connect(openMotionAction, &QAction::triggered, this, &MainWindow::openMotionPanel);
  connect(loadBoardAction, &QAction::triggered, this, &MainWindow::loadBoardToTrack);
  connect(unloadBoardAction, &QAction::triggered, this, &MainWindow::unloadBoardFromTrack);
  connect(markOffsetAction, &QAction::triggered, this, &MainWindow::openMarkOffsetCalibration);
  connect(originCalibAction, &QAction::triggered, this, &MainWindow::openOriginCalibration);
  connect(laserOffsetAction, &QAction::triggered, this, &MainWindow::openLaserOffsetCalibration);
  connect(openLogAction, &QAction::triggered, this, &MainWindow::openLogWindow);
  connect(dataCollectAction, &QAction::triggered, this, &MainWindow::openDataCollect);
  connect(markEditAction, &QAction::triggered, this, &MainWindow::openMarkEditDialog);
  connect(wholeBoardScanAction, &QAction::triggered, this, &MainWindow::runWholeBoardScan);

  auto *toolBar = addToolBar(QStringLiteral("主工具栏"));
  toolBar->setMovable(false);
  toolBar->addAction(newProgramAction);
  toolBar->addAction(openProgramAction);
  toolBar->addAction(saveProgramAction);
  toolBar->addSeparator();
  toolBar->addAction(cameraConfigAction);
  toolBar->addAction(startPreviewAction);
  toolBar->addAction(openMotionAction);
  toolBar->addSeparator();
  toolBar->addAction(settingsAction);
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

  auto makeStatusChip = [headerFrame](const QString &label, QLabel *&valueLabel, const QString &initialValue,
                                      const int minWidth = 72) {
    auto *chip = new QFrame(headerFrame);
    chip->setStyleSheet(QStringLiteral(
        "QFrame { background: #1e293b; border: 1px solid #334155; border-radius: 8px; }"
        "QLabel { color: #cbd5e1; }"));
    auto *chipLayout = new QHBoxLayout(chip);
    chipLayout->setContentsMargins(10, 5, 10, 5);
    chipLayout->setSpacing(6);
    auto *labelWidget = new QLabel(label, chip);
    labelWidget->setStyleSheet(QStringLiteral("color: #64748b; font-size: 11px;"));
    valueLabel = new QLabel(initialValue, chip);
    valueLabel->setStyleSheet(QStringLiteral("color: #e2e8f0; font-size: 11px; font-weight: 700;"));
    valueLabel->setMinimumWidth(minWidth);
    valueLabel->setAlignment(Qt::AlignCenter);
    chipLayout->addWidget(labelWidget);
    chipLayout->addWidget(valueLabel);
    return chip;
  };

  auto *posChip = makeStatusChip(QStringLiteral("坐标"), cursorPositionValueLabel_, QStringLiteral("X=0.0 Y=0.0"), 112);
  auto *zoomChip = makeStatusChip(QStringLiteral("缩放"), zoomValueLabel_, QStringLiteral("100%"), 64);
  auto *camModeChip = makeStatusChip(QStringLiteral("相机"), cameraModeValueLabel_, QStringLiteral("--"), 112);
  auto *fovChip = makeStatusChip(QStringLiteral("FOV"), fovInfoValueLabel_, QStringLiteral("640 x 360"), 96);
  auto *statusChip = makeStatusChip(QStringLiteral("状态"), cameraStatusToolbarValueLabel_, QStringLiteral("未启动"), 88);

  statusSummaryValueLabel_ = new QLabel(QStringLiteral("系统初始化中"), headerFrame);
  statusSummaryValueLabel_->setStyleSheet(
      QStringLiteral("padding: 8px 12px; background: rgba(148,163,184,0.16); border-radius: 10px;"));
  headerLayout->addWidget(posChip);
  headerLayout->addWidget(zoomChip);
  headerLayout->addWidget(camModeChip);
  headerLayout->addWidget(fovChip);
  headerLayout->addWidget(statusChip);
  headerLayout->addStretch();

  // Mode switch button
  switchToRunButton_ = new QPushButton(QStringLiteral("运行模式"), headerFrame);
  switchToRunButton_->setStyleSheet(QStringLiteral(
      "QPushButton { background: #16a34a; color: #f8fafc; border: 1px solid #22c55e; border-radius: 8px; "
      "  padding: 8px 18px; min-height: 30px; font-weight: 700; font-size: 12px; }"
      "QPushButton:hover { background: #22c55e; }"));
  switchToEditorButton_ = new QPushButton(QStringLiteral("返回编辑"), headerFrame);
  switchToEditorButton_->setStyleSheet(QStringLiteral(
      "QPushButton { background: #1e3a5f; color: #e2e8f0; border: 1px solid #334155; border-radius: 8px; "
      "  padding: 8px 18px; min-height: 30px; font-weight: 700; font-size: 12px; }"
      "QPushButton:hover { background: #2563eb; }"));
  switchToEditorButton_->setVisible(false);

  // SPC dashboard button
  auto *spcButton = new QPushButton(QStringLiteral("SPC 看板"), headerFrame);
  spcButton->setStyleSheet(QStringLiteral(
      "QPushButton { background: #7c3aed; color: #f8fafc; border: 1px solid #8b5cf6; border-radius: 8px; "
      "  padding: 8px 18px; min-height: 30px; font-weight: 700; font-size: 12px; }"
      "QPushButton:hover { background: #8b5cf6; }"));
  connect(spcButton, &QPushButton::clicked, this, &MainWindow::openSpcDashboard);

  auto *historyButton = new QPushButton(QStringLiteral("生产历史"), headerFrame);
  historyButton->setStyleSheet(QStringLiteral(
      "QPushButton { background: #0f766e; color: #f8fafc; border: 1px solid #14b8a6; border-radius: 8px; "
      "  padding: 8px 18px; min-height: 30px; font-weight: 700; font-size: 12px; }"
      "QPushButton:hover { background: #14b8a6; }"));
  connect(historyButton, &QPushButton::clicked, this, &MainWindow::openProductionHistory);

  headerLayout->addWidget(switchToRunButton_);
  headerLayout->addWidget(switchToEditorButton_);
  headerLayout->addWidget(spcButton);
  headerLayout->addWidget(historyButton);
  headerLayout->addWidget(statusSummaryValueLabel_);
  rootLayout->addWidget(headerFrame);

  // Main stacked widget: editor (0) / run (1)
  mainStackedWidget_ = new QStackedWidget(centralWidget);

  // Page 0: Editor interface
  editorPage_ = new QWidget(mainStackedWidget_);
  auto *editorLayout = new QHBoxLayout(editorPage_);
  editorLayout->setContentsMargins(0, 0, 0, 0);
  editorLayout->setSpacing(0);

  auto *splitter = new QSplitter(Qt::Horizontal, editorPage_);
  splitter->setChildrenCollapsible(false);

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
  splitter->setStretchFactor(0, 3);
  splitter->setStretchFactor(1, 2);
  splitter->setSizes({900, 500});
  splitter->setHandleWidth(4);

  rightWidget->setMinimumWidth(380);
  editorLayout->addWidget(splitter);
  mainStackedWidget_->addWidget(editorPage_);

  // Page 1: Run interface
  buildRunInterface();
  mainStackedWidget_->addWidget(runModeWidget_);

  rootLayout->addWidget(mainStackedWidget_, 1);

  // Mode switch connections
  connect(switchToRunButton_, &QPushButton::clicked, this, &MainWindow::switchToRunMode);
  connect(switchToEditorButton_, &QPushButton::clicked, this, &MainWindow::switchToEditorMode);

  mainStackedWidget_->setCurrentIndex(0);

  statusBar()->showMessage(QStringLiteral("就绪"));
}

void MainWindow::buildLeftWorkbench(QBoxLayout *parentLayout) {
  auto *toolbarFrame = new QFrame(this);
  toolbarFrame->setStyleSheet(QStringLiteral(
      "QFrame { background: #0f172a; border: 1px solid #334155; border-radius: 12px; }"
      "QLabel { color: #cbd5e1; }"
      "QPushButton { background: #1e293b; color: #e2e8f0; border: 1px solid #334155; border-radius: 8px; "
      "  padding: 5px 12px; min-height: 26px; }"
      "QPushButton:hover { background: #2563eb; }"
      "QPushButton:pressed { background: #1d4ed8; }"));
  auto *toolbarOuterLayout = new QVBoxLayout(toolbarFrame);
  toolbarOuterLayout->setContentsMargins(8, 8, 8, 8);
  toolbarOuterLayout->setSpacing(6);

  auto *buttonRowTop = new QHBoxLayout;
  buttonRowTop->setSpacing(6);
  auto *buttonRowBottom = new QHBoxLayout;
  buttonRowBottom->setSpacing(6);

  auto *selectModeButton = new QPushButton(QStringLiteral("选择"), toolbarFrame);
  auto *drawRoiButton = new QPushButton(QStringLiteral("框选 ROI"), toolbarFrame);
  auto *drawMarkButton = new QPushButton(QStringLiteral("框选 Mark"), toolbarFrame);
  auto *fitViewButton = new QPushButton(QStringLiteral("适配视图"), toolbarFrame);
  auto *startPreviewButton = new QPushButton(QStringLiteral("开始实时采图"), toolbarFrame);
  auto *stopPreviewButton = new QPushButton(QStringLiteral("停止采图"), toolbarFrame);
  auto *wholeBoardScanButton = new QPushButton(QStringLiteral("整板扫描"), toolbarFrame);
  auto *markCalibButton = new QPushButton(QStringLiteral("Mark 校正"), toolbarFrame);
  auto *originCalibButton = new QPushButton(QStringLiteral("原点校正"), toolbarFrame);
  toggleCodeCameraViewButton_ = new QPushButton(QStringLiteral("读码相机视图"), toolbarFrame);
  toggleFovButton_ = new QPushButton(QStringLiteral("隐藏 FOV 640x360"), toolbarFrame);
  auto *gestureHelpButton = new QPushButton(QStringLiteral("? 操作提示"), toolbarFrame);
  gestureHelpButton->setToolTip(QStringLiteral("触控板/鼠标操作说明"));
  gestureHelpButton->setStyleSheet(QStringLiteral(
      "QPushButton { background: #1e293b; color: #facc15; border: 1px solid #facc15; border-radius: 8px; "
      "  padding: 5px 12px; min-height: 26px; font-weight: 700; }"
      "QPushButton:hover { background: #facc15; color: #0f172a; }"));

  auto *settingsButton = new QPushButton(QStringLiteral("⚙ 系统设置"), toolbarFrame);
  settingsButton->setStyleSheet(QStringLiteral(
      "QPushButton { background: #1e3a5f; color: #e2e8f0; border: 1px solid #334155; border-radius: 8px; "
      "  padding: 5px 14px; min-height: 26px; }"
      "QPushButton:hover { background: #2563eb; }"));

  buttonRowTop->addWidget(selectModeButton);
  buttonRowTop->addWidget(drawRoiButton);
  buttonRowTop->addWidget(drawMarkButton);
  buttonRowTop->addWidget(fitViewButton);
  buttonRowTop->addWidget(startPreviewButton);
  buttonRowTop->addWidget(stopPreviewButton);
  buttonRowTop->addWidget(wholeBoardScanButton);
  buttonRowTop->addStretch();
  buttonRowTop->addWidget(settingsButton);

  buttonRowBottom->addWidget(markCalibButton);
  buttonRowBottom->addWidget(originCalibButton);
  buttonRowBottom->addWidget(toggleCodeCameraViewButton_);
  buttonRowBottom->addWidget(toggleFovButton_);
  buttonRowBottom->addStretch();
  buttonRowBottom->addWidget(gestureHelpButton);

  toolbarOuterLayout->addLayout(buttonRowTop);
  toolbarOuterLayout->addLayout(buttonRowBottom);
  parentLayout->addWidget(toolbarFrame);

  auto *graphicsGroupBox = new QGroupBox(QStringLiteral("CAD 拼接工位图"), this);
  graphicsGroupBox->setStyleSheet(QStringLiteral(
      "QGroupBox { color: #cbd5e1; font-weight: 600; border: 1px solid #334155; border-radius: 10px; "
      "  margin-top: 12px; padding-top: 16px; }"
      "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; color: #cbd5e1; }"));
  auto *graphicsLayout = new QGridLayout(graphicsGroupBox);
  graphicsLayout->setContentsMargins(8, 8, 8, 8);
  graphicsLayout->setSpacing(0);

  auto *cornerLabel = new QLabel(QStringLiteral("XY"), graphicsGroupBox);
  cornerLabel->setAlignment(Qt::AlignCenter);
  cornerLabel->setMinimumSize(32, 32);
  cornerLabel->setStyleSheet(QStringLiteral("background: #0f172a; color: #cbd5e1; border-right: 1px solid #334155; border-bottom: 1px solid #334155;"));

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
  connect(wholeBoardScanButton, &QPushButton::clicked, this, &MainWindow::runWholeBoardScan);
  connect(markCalibButton, &QPushButton::clicked, this, &MainWindow::openMarkOffsetCalibration);
  connect(originCalibButton, &QPushButton::clicked, this, &MainWindow::openOriginCalibration);
  connect(toggleCodeCameraViewButton_, &QPushButton::clicked, this, &MainWindow::toggleCodeCameraView);
  connect(toggleFovButton_, &QPushButton::clicked, this, &MainWindow::toggleFovOverlay);
  connect(settingsButton, &QPushButton::clicked, this, &MainWindow::openSettings);
  connect(gestureHelpButton, &QPushButton::clicked, this, [this] {
    QMessageBox::information(this, QStringLiteral("CAD 画布操作说明"),
      QStringLiteral("触控板操作：\n"
                     "  • 双指滑动 → 平移/拖拽画布\n"
                     "  • 双指捏合 → 缩放画布\n"
                     "  • 单指点击拖拽 → 框选 ROI / Mark 区域\n"
                     "\n"
                     "鼠标操作：\n"
                     "  • 鼠标中键拖拽 → 平移画布\n"
                     "  • 滚轮滚动 → 缩放画布\n"
                     "  • 左键拖拽 → 框选 ROI / Mark 区域\n"
                     "\n"
                     "模式切换：\n"
                     "  • 点击「选择」→ 浏览/拖动已有物件\n"
                     "  • 点击「框选 ROI」→ 在画布中拖拽生成 ROI\n"
                     "  • 点击「框选 Mark」→ 在画布中拖拽生成 Mark\n"
                     "  • 点击「适配视图」→ 恢复最佳视图"));
  });
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
  auto *stackFrame = new QFrame(this);
  stackFrame->setStyleSheet(QStringLiteral(
      "QFrame#stackFrame { background: #0f172a; border: 1px solid #334155; border-radius: 14px; }"
      "QGroupBox { color: #e2e8f0; font-weight: 600; border: 1px solid #334155; border-radius: 10px; "
      "  margin-top: 12px; padding-top: 16px; }"
      "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; color: #cbd5e1; }"
      "QLabel { color: #e2e8f0; min-height: 20px; }"
      "QComboBox { background: #1e293b; color: #e2e8f0; border: 1px solid #334155; border-radius: 6px; "
      "  padding: 5px 10px; min-height: 28px; }"
      "QComboBox::drop-down { border: none; width: 20px; }"
      "QComboBox QAbstractItemView { background: #1e293b; color: #e2e8f0; "
      "  selection-background-color: #2563eb; border: 1px solid #334155; }"
      "QLineEdit { background: #1e293b; color: #e2e8f0; border: 1px solid #334155; border-radius: 6px; "
      "  padding: 5px 10px; min-height: 28px; }"
      "QDoubleSpinBox { background: #1e293b; color: #e2e8f0; border: 1px solid #334155; border-radius: 6px; "
      "  padding: 5px 10px; min-height: 28px; }"
      "QPushButton { background: #1e3a5f; color: #e2e8f0; border: 1px solid #334155; border-radius: 8px; "
      "  padding: 6px 14px; min-height: 28px; }"
      "QPushButton:hover { background: #2563eb; }"
      "QPushButton:pressed { background: #1d4ed8; }"
      "QTableWidget { background: #0f172a; color: #e2e8f0; border: 1px solid #334155; "
      "  gridline-color: #1e293b; font-size: 13px; }"
      "QTableWidget::item { padding: 6px 10px; }"
      "QTableWidget::item:selected { background: #1e3a5f; color: #f8fafc; }"
      "QHeaderView::section { background: #1e293b; color: #cbd5e1; border: 1px solid #334155; "
      "  padding: 6px 10px; font-weight: 600; font-size: 12px; }"
      "QListWidget { background: #0f172a; color: #e2e8f0; border: none; font-size: 13px; }"
      "QListWidget::item { padding: 10px 14px; }"
      "QListWidget::item:selected { background: #1e3a5f; color: #f8fafc; }"
      "QListWidget::item:hover { background: #162033; }"
      "QProgressBar { background: #1e293b; border: 1px solid #334155; border-radius: 4px; "
      "  text-align: center; color: #e2e8f0; min-height: 18px; }"
      "QProgressBar::chunk { background: #2563eb; border-radius: 3px; }"
      "QFormLayout { spacing: 12px; }"));
  stackFrame->setObjectName(QStringLiteral("stackFrame"));
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

  connect(toolSelectorListWidget_, &QListWidget::currentRowChanged, this, &MainWindow::setCurrentPage);
}

void MainWindow::buildRoiPage() {
  auto *scrollArea = new QScrollArea(toolStackedWidget_);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; } QScrollBar:vertical { background: #0f172a; width: 8px; } QScrollBar::handle:vertical { background: #334155; border-radius: 4px; min-height: 24px; } QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"));

  auto *page = new QWidget(scrollArea);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(10);

  auto *toolGroupBox = new QGroupBox(QStringLiteral("ROI 绘制与工艺设置"), page);
  auto *toolLayout = new QFormLayout(toolGroupBox);
  toolLayout->setSpacing(10);
  toolLayout->setContentsMargins(12, 16, 12, 12);
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
  roiTableWidget_->setMinimumHeight(140);
  layout->addWidget(roiTableWidget_, 1);

  scrollArea->setWidget(page);
  toolStackedWidget_->addWidget(scrollArea);

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
  auto *scrollArea = new QScrollArea(toolStackedWidget_);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; } QScrollBar:vertical { background: #0f172a; width: 8px; } QScrollBar::handle:vertical { background: #334155; border-radius: 4px; min-height: 24px; } QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"));

  auto *page = new QWidget(scrollArea);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(10);

  auto *toolGroupBox = new QGroupBox(QStringLiteral("Mark 工艺设置"), page);
  auto *toolLayout = new QFormLayout(toolGroupBox);
  toolLayout->setSpacing(10);
  toolLayout->setContentsMargins(12, 16, 12, 12);
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
  markTableWidget_->setMinimumHeight(140);
  layout->addWidget(markTableWidget_, 1);

  scrollArea->setWidget(page);
  toolStackedWidget_->addWidget(scrollArea);

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
  auto *scrollArea = new QScrollArea(toolStackedWidget_);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; } QScrollBar:vertical { background: #0f172a; width: 8px; } QScrollBar::handle:vertical { background: #334155; border-radius: 4px; min-height: 24px; } QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"));

  auto *page = new QWidget(scrollArea);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(8, 8, 8, 8);
  layout->setSpacing(10);

  auto *templateGroupBox = new QGroupBox(QStringLiteral("模版编辑与读码调试"), page);
  auto *templateLayout = new QFormLayout(templateGroupBox);
  templateLayout->setSpacing(10);
  templateLayout->setContentsMargins(12, 16, 12, 12);
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

  auto *templateCacheGroupBox = new QGroupBox(QStringLiteral("模板缓存预览"), page);
  auto *templateCacheLayout = new QVBoxLayout(templateCacheGroupBox);
  templateCachePreviewLabel_ = new QLabel(QStringLiteral("当前还没有缓存模板图"), templateCacheGroupBox);
  templateCachePreviewLabel_->setAlignment(Qt::AlignCenter);
  templateCachePreviewLabel_->setMinimumHeight(220);
  templateCachePreviewLabel_->setWordWrap(true);
  templateCachePreviewLabel_->setStyleSheet(QStringLiteral(
      "QLabel { background: #020617; border: 1px solid #334155; border-radius: 12px; color: #94a3b8; }"));
  templateCacheLayout->addWidget(templateCachePreviewLabel_);
  layout->addWidget(templateCacheGroupBox);

  auto *calibrationGroupBox = new QGroupBox(QStringLiteral("校正结果"), page);
  auto *calibrationLayout = new QVBoxLayout(calibrationGroupBox);
  calibrationSummaryValueLabel_ = new QLabel(QStringLiteral("当前还没有校正结果。"), calibrationGroupBox);
  calibrationSummaryValueLabel_->setWordWrap(true);
  calibrationLayout->addWidget(calibrationSummaryValueLabel_);
  layout->addWidget(calibrationGroupBox);

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
  auto *captureTemplateButton = new QPushButton(QStringLiteral("抓取当前模板图"), page);
  auto *saveButton = new QPushButton(QStringLiteral("保存当前程序"), page);
  buttonLayout->addWidget(testCodeButton);
  buttonLayout->addWidget(captureTemplateButton);
  buttonLayout->addWidget(saveButton);
  buttonLayout->addStretch();
  layout->addLayout(buttonLayout);
  layout->addStretch();

  scrollArea->setWidget(page);
  toolStackedWidget_->addWidget(scrollArea);

  connect(testCodeButton, &QPushButton::clicked, this, &MainWindow::testCodeReading);
  connect(captureTemplateButton, &QPushButton::clicked, this, &MainWindow::captureCurrentTemplateImage);
  connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveCurrentProgram);
  connect(codeRegionLineEdit_, &QLineEdit::editingFinished, this, &MainWindow::syncProgramFromEditors);
}

void MainWindow::appendLog(const QString &message) {
  if (logWindow_ != nullptr) {
    logWindow_->appendLog(message);
  }

  statusBar()->showMessage(message, 5000);
}

void MainWindow::refreshStatusSummary() {
  statusSummaryValueLabel_->setText(
      QStringLiteral("程序：%1 | 运控：%2 | 模式：%3")
          .arg(programManager_.currentProgram().has_value()
                   ? QString::fromStdString(programManager_.currentProgram()->name)
                   : QStringLiteral("未加载"))
          .arg(motionStateText())
          .arg(canvasMode_ == CanvasMode::DrawMark ? QStringLiteral("框选 Mark")
                                                   : (canvasMode_ == CanvasMode::DrawRoi ? QStringLiteral("框选 ROI")
                                                                                        : QStringLiteral("选择"))));
}

void MainWindow::refreshTemplatePreviewSummary() {
  if (templatePreviewValueLabel_ == nullptr) {
    return;
  }

  const auto currentProgram = programManager_.currentProgram();
  if (!currentProgram.has_value()) {
    templatePreviewValueLabel_->setText(QStringLiteral("当前未选择任何 Mark / ROI"));
    if (templateCachePreviewLabel_ != nullptr) {
      templateCachePreviewLabel_->setPixmap(QPixmap());
      templateCachePreviewLabel_->setText(QStringLiteral("当前还没有缓存模板图"));
    }
    if (calibrationSummaryValueLabel_ != nullptr) {
      calibrationSummaryValueLabel_->setText(QStringLiteral("当前还没有校正结果。"));
    }
    return;
  }

  QStringList lines;
  QStringList calibrationLines;
  if (selectedMarkIndex_ >= 0 && selectedMarkIndex_ < static_cast<int>(currentProgram->marks.size())) {
    lines << QStringLiteral("当前 Mark：%1")
                 .arg(QString::fromStdString(currentProgram->marks[static_cast<std::size_t>(selectedMarkIndex_)].name));
  } else if (selectedRoiIndex_ >= 0 && selectedRoiIndex_ < static_cast<int>(currentProgram->rois.size())) {
    lines << QStringLiteral("当前 ROI：%1")
                 .arg(QString::fromStdString(currentProgram->rois[static_cast<std::size_t>(selectedRoiIndex_)].name));
  } else {
    lines << QStringLiteral("当前未选择任何 Mark / ROI");
  }

  lines << QStringLiteral("模板缓存：%1")
               .arg(currentProgram->runtimeSummary.templateCachePath.empty()
                        ? QStringLiteral("暂无")
                        : QString::fromStdString(currentProgram->runtimeSummary.templateCachePath));
  lines << QStringLiteral("匹配结果：%1")
               .arg(currentProgram->runtimeSummary.latestTemplateMatchSummary.empty()
                        ? QStringLiteral("暂无")
                        : QString::fromStdString(currentProgram->runtimeSummary.latestTemplateMatchSummary));
  lines << QStringLiteral("整板扫描：%1")
               .arg(currentProgram->runtimeSummary.lastBoardScanSummary.empty()
                        ? QStringLiteral("暂无")
                        : QString::fromStdString(currentProgram->runtimeSummary.lastBoardScanSummary));

  if (currentProgram->runtimeSummary.hasMarkCalibration) {
    calibrationLines << QStringLiteral("Mark 校正：dX=%1 mm, dY=%2 mm, dR=%3°")
                            .arg(currentProgram->runtimeSummary.markCalibrationOffsetXmm, 0, 'f', 4)
                            .arg(currentProgram->runtimeSummary.markCalibrationOffsetYmm, 0, 'f', 4)
                            .arg(currentProgram->runtimeSummary.markCalibrationRotationDegrees, 0, 'f', 3);
  }

  if (currentProgram->runtimeSummary.hasOriginCalibration) {
    calibrationLines << QStringLiteral("原点补偿：X=%1, Y=%2, Z=%3, R=%4")
                            .arg(currentProgram->runtimeSummary.originCorrectedPose.x, 0, 'f', 3)
                            .arg(currentProgram->runtimeSummary.originCorrectedPose.y, 0, 'f', 3)
                            .arg(currentProgram->runtimeSummary.originCorrectedPose.z, 0, 'f', 3)
                            .arg(currentProgram->runtimeSummary.originCorrectedPose.r, 0, 'f', 3);
  }

  templatePreviewValueLabel_->setText(lines.join(QStringLiteral("\n")));

  if (templateCachePreviewLabel_ != nullptr) {
    if (!currentProgram->runtimeSummary.templateCachePath.empty()) {
      const QString cachePath = QString::fromStdString(currentProgram->runtimeSummary.templateCachePath);
      const QPixmap pixmap(cachePath);
      if (!pixmap.isNull()) {
        templateCachePreviewLabel_->setText(QString());
        templateCachePreviewLabel_->setPixmap(
            pixmap.scaled(templateCachePreviewLabel_->size() - QSize(12, 12), Qt::KeepAspectRatio, Qt::SmoothTransformation));
      } else {
        templateCachePreviewLabel_->setPixmap(QPixmap());
        templateCachePreviewLabel_->setText(QStringLiteral("模板缓存图存在，但当前无法预览。\n%1").arg(cachePath));
      }
    } else {
      templateCachePreviewLabel_->setPixmap(QPixmap());
      templateCachePreviewLabel_->setText(QStringLiteral("当前还没有缓存模板图"));
    }
  }

  if (calibrationSummaryValueLabel_ != nullptr) {
    calibrationSummaryValueLabel_->setText(
        calibrationLines.isEmpty() ? QStringLiteral("当前还没有校正结果。")
                                   : calibrationLines.join(QStringLiteral("\n")));
  }
}

void MainWindow::refreshProgramWidgets() {
  const auto currentProgram = programManager_.currentProgram();
  auto setLabelText = [](QLabel *label, const QString &text) {
    if (label != nullptr) label->setText(text);
  };

  if (!currentProgram.has_value()) {
    virtualCamera_.setBoardDefinition(BoardDefinition {});
    virtualCamera_.setScanRecipe(ScanRecipe {});
    virtualCamera_.clearBoardReadyOriginPose();
    setLabelText(programNameValueLabel_, QStringLiteral("--"));
    setLabelText(programPathValueLabel_, QStringLiteral("--"));
    setLabelText(programAiModelValueLabel_, QStringLiteral("--"));
    setLabelText(markCountValueLabel_, QStringLiteral("0"));
    setLabelText(roiCountValueLabel_, QStringLiteral("0"));
    if (markTableWidget_ != nullptr) markTableWidget_->setRowCount(0);
    if (roiTableWidget_ != nullptr) roiTableWidget_->setRowCount(0);
    if (codeRegionLineEdit_ != nullptr) codeRegionLineEdit_->clear();
    refreshTemplatePreviewSummary();
    refreshWorkbenchScene();
    refreshStatusSummary();
    return;
  }

  if (motionControlDialog_ != nullptr) {
    motionControlDialog_->setProgramBoardDefinition(currentProgram->boardDefinition);
  }
  virtualMotionSystem_.setBoardDefinition(currentProgram->boardDefinition);
  virtualCamera_.setBoardDefinition(currentProgram->boardDefinition);
  virtualCamera_.setScanRecipe(currentProgram->scanRecipe);
  virtualCamera_.setBoardReadyOriginPose(logicalBoardReadyOriginPose(*currentProgram));

  setLabelText(programNameValueLabel_, QString::fromStdString(currentProgram->name));
  setLabelText(programPathValueLabel_, currentProgram->filePath.empty() ? QStringLiteral("内存中的默认程序")
                                                                       : QString::fromStdString(currentProgram->filePath));
  setLabelText(programAiModelValueLabel_, QString::fromStdString(currentProgram->aiModelPath));
  setLabelText(markCountValueLabel_, QString::number(currentProgram->marks.size()));
  setLabelText(roiCountValueLabel_, QString::number(currentProgram->rois.size()));
  setLabelText(motionStatusValueLabel_, motionStateText());

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
          new QTableWidgetItem(QStringLiteral("(%1, %2) mm").arg(mark.x, 0, 'f', 1).arg(mark.y, 0, 'f', 1)));
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
      roiTableWidget_->setItem(row, 3, new QTableWidgetItem(QStringLiteral("%1 mm").arg(roi.x, 0, 'f', 1)));
      roiTableWidget_->setItem(row, 4, new QTableWidgetItem(QStringLiteral("%1 mm").arg(roi.y, 0, 'f', 1)));
      roiTableWidget_->setItem(
          row, 5,
          new QTableWidgetItem(QStringLiteral("%1 x %2 mm").arg(roi.width, 0, 'f', 1).arg(roi.height, 0, 'f', 1)));
      roiTableWidget_->setItem(row, 6, new QTableWidgetItem(roi.enabled ? QStringLiteral("启用") : QStringLiteral("停用")));
    }
  }

  if (codeRegionLineEdit_ != nullptr) codeRegionLineEdit_->setText(QString::fromStdString(currentProgram->codeRegionName));
  refreshTemplatePreviewSummary();
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

  if (const auto currentProgram = programManager_.currentProgram();
      currentProgram.has_value() && currentBoardSceneLayout(currentProgram).has_value()) {
    const auto layout = *currentBoardSceneLayout(currentProgram);
    for (int index = 0; index < static_cast<int>(currentProgram->rois.size()); ++index) {
      const auto &roi = currentProgram->rois[static_cast<std::size_t>(index)];
      const auto sceneRect =
          BoardViewTransform::boardToSceneTopLeftRect(layout, MillimeterPoint {roi.x, roi.y}, roi.width, roi.height);
      auto *item = new ProgramShapeItem(ProgramShapeItem::Domain::Roi, index,
                                        QRectF(0.0, 0.0, sceneRect.width, sceneRect.height),
                                        QString::fromStdString(roi.name), QColor("#22c55e"),
                                        MarkShape::Rectangle, roi.shape);
      item->setPos(sceneRect.x, sceneRect.y);
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
      const auto sceneRect = BoardViewTransform::boardToSceneCenteredRect(
          layout, MillimeterPoint {mark.x, mark.y}, mark.width, mark.height);
      auto *item =
          new ProgramShapeItem(ProgramShapeItem::Domain::Mark, index,
                               QRectF(-sceneRect.width / 2.0, -sceneRect.height / 2.0, sceneRect.width, sceneRect.height),
                               QString::fromStdString(mark.name), colorFromMark(mark), mark.shape);
      item->setPos(sceneRect.x + sceneRect.width / 2.0, sceneRect.y + sceneRect.height / 2.0);
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

  QRectF fovRect = workbenchFovRect(lastFrameSize_);
  if (const auto currentProgram = programManager_.currentProgram();
      currentProgram.has_value() && currentBoardSceneLayout(currentProgram).has_value()) {
    const auto layout = *currentBoardSceneLayout(currentProgram);
    const auto fovSceneRect = BoardViewTransform::boardToSceneTopLeftRect(
        layout, MillimeterPoint {0.0, 0.0}, currentProgram->scanRecipe.fovWidthMm, currentProgram->scanRecipe.fovHeightMm);
    if (fovSceneRect.width > 0.0 && fovSceneRect.height > 0.0) {
      fovRect = toQRectF(fovSceneRect);
    }
  }

  auto *fovBackdrop = workbenchScene_->addRect(
      fovRect.adjusted(6.0, 6.0, -6.0, -6.0),
      QPen(QColor("#1e293b"), 1.5),
      QBrush(showCodeCameraView_ ? QColor("#020617") : QColor(250, 204, 21, 14)));
  fovBackdrop->setZValue(2.0);
  fovBackdrop->setVisible(showFovOverlay_);

  if (showCodeCameraView_) {
    if (!lastCameraFrameImage_.isNull()) {
      const QSize contentSize = fovRect.adjusted(8.0, 8.0, -8.0, -8.0).size().toSize();
      const QImage scaledFrame =
          lastCameraFrameImage_.scaled(contentSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
      auto *fovPixmapItem = workbenchScene_->addPixmap(QPixmap::fromImage(scaledFrame));
      const QRectF contentRect = fovBackdrop->rect();
      fovPixmapItem->setPos(contentRect.center().x() - scaledFrame.width() / 2.0,
                            contentRect.center().y() - scaledFrame.height() / 2.0);
      fovPixmapItem->setZValue(3.0);
      fovPixmapItem->setVisible(showFovOverlay_);
    } else {
      auto *emptyText = workbenchScene_->addText(QStringLiteral("读码相机视图\n等待实时采图"));
      emptyText->setDefaultTextColor(QColor("#cbd5e1"));
      emptyText->setPos(fovBackdrop->rect().center().x() - 62.0, fovBackdrop->rect().center().y() - 26.0);
      emptyText->setZValue(3.0);
      emptyText->setVisible(showFovOverlay_);
    }
  }

  fovRectItem_ = workbenchScene_->addRect(
      fovRect,
      QPen(QColor("#facc15"), 3, Qt::DashLine), QBrush(Qt::NoBrush));
  fovRectItem_->setVisible(showFovOverlay_);
  fovRectItem_->setZValue(4.0);
  auto *fovText = workbenchScene_->addText(showCodeCameraView_ ? QStringLiteral("实时 FOV / 读码相机视图")
                                                               : QStringLiteral("实时 FOV"));
  fovText->setDefaultTextColor(QColor("#fef08a"));
  fovText->setPos(fovRectItem_->rect().topLeft() + QPointF(8.0, -24.0));
  fovText->setZValue(5.0);
  fovText->setVisible(showFovOverlay_);

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
  const QString cameraStatusText = virtualCamera_.isOpened() ? QStringLiteral("虚拟采图中") : QStringLiteral("未启动");
  if (cameraStatusToolbarValueLabel_ != nullptr) cameraStatusToolbarValueLabel_->setText(cameraStatusText);
  if (cameraStatusDetailValueLabel_ != nullptr) cameraStatusDetailValueLabel_->setText(cameraStatusText);
  if (cameraDeviceValueLabel_ != nullptr) cameraDeviceValueLabel_->setText(QStringLiteral("DEMO"));
  if (cameraStatusToolbarValueLabel_ != nullptr) {
    cameraStatusToolbarValueLabel_->setStyleSheet(
        virtualCamera_.isOpened() ? QStringLiteral("color: #067647; font-weight: 700;")
                                  : QStringLiteral("color: #667085; font-weight: 700;"));
  }
  if (cameraStatusDetailValueLabel_ != nullptr) {
    cameraStatusDetailValueLabel_->setStyleSheet(virtualCamera_.isOpened()
                                                     ? QStringLiteral("color: #067647; font-weight: 700;")
                                                     : QStringLiteral("color: #667085; font-weight: 700;"));
  }
  if (fovInfoValueLabel_ != nullptr) fovInfoValueLabel_->setText(QStringLiteral("%1 x %2").arg(lastFrameSize_.width()).arg(lastFrameSize_.height()));
  toggleFovButton_->setText(showFovOverlay_ ? QStringLiteral("隐藏 FOV %1x%2")
                                                  .arg(lastFrameSize_.width())
                                                  .arg(lastFrameSize_.height())
                                            : QStringLiteral("显示 FOV %1x%2")
                                                  .arg(lastFrameSize_.width())
                                                  .arg(lastFrameSize_.height()));
  if (toggleCodeCameraViewButton_ != nullptr) {
    toggleCodeCameraViewButton_->setText(showCodeCameraView_ ? QStringLiteral("隐藏读码相机视图")
                                                             : QStringLiteral("读码相机视图"));
  }
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
  if (codeRegionLineEdit_ == nullptr) return;
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
  if (newProgramDialog_ == nullptr) {
    newProgramDialog_ = new NewProgramDialog(this);
  }

  BoardDefinition initialBoardDefinition;
  ScanRecipe initialScanRecipe;
  QString initialProgramName = QStringLiteral("board_program");
  if (const auto currentProgram = programManager_.currentProgram(); currentProgram.has_value()) {
    initialBoardDefinition = currentProgram->boardDefinition;
    initialScanRecipe = currentProgram->scanRecipe;
    initialProgramName = QString::fromStdString(currentProgram->name);
  }

  newProgramDialog_->setInitialProgramName(initialProgramName);
  newProgramDialog_->setInitialBoardDefinition(initialBoardDefinition);
  newProgramDialog_->setInitialScanRecipe(initialScanRecipe);
  if (newProgramDialog_->exec() != QDialog::Accepted) {
    appendLog(QStringLiteral("已取消新建程序。"));
    return;
  }

  const auto result = programManager_.createDefaultProgram();
  selectedMarkIndex_ = -1;
  selectedRoiIndex_ = -1;
  if (result) {
    updateProgram([this](ProgramModel &program) {
      program.name = newProgramDialog_->programName().isEmpty()
                         ? "board_program"
                         : newProgramDialog_->programName().toStdString();
      program.boardDefinition = newProgramDialog_->boardDefinition();
      program.scanRecipe = newProgramDialog_->scanRecipe();
    });
    appendLog(QStringLiteral("已新建默认程序。"));
  } else {
    appendLog(QStringLiteral("新建默认程序失败：%1").arg(QString::fromStdString(result.message)));
  }
  refreshProgramWidgets();
  resetWorkbenchView();
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
  resetWorkbenchView();
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
  dialog.setExposureTimeMs(cameraExposureMs_);
  dialog.setGainValue(cameraGain_);
  dialog.setResolutionPreset(cameraResolutionPreset_);
  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  cameraDeviceIndex_ = dialog.deviceIndex();
  cameraExposureMs_ = dialog.exposureTimeMs();
  cameraGain_ = dialog.gainValue();
  cameraResolutionPreset_ = dialog.resolutionPreset();
  appendLog(QStringLiteral("虚拟相机配置已更新：索引=%1，曝光=%2 ms，增益=%3 dB，分辨率=%4。")
                .arg(cameraDeviceIndex_)
                .arg(cameraExposureMs_, 0, 'f', 1)
                .arg(cameraGain_, 0, 'f', 1)
                .arg(cameraResolutionPreset_));
  refreshCameraState();
}

void MainWindow::openMotionPanel() {
  if (motionControlDialog_ == nullptr) {
    motionControlDialog_ = new MotionControlDialog(&virtualMotionSystem_, this);
    connect(motionControlDialog_, &MotionControlDialog::motionStateChanged, this, [this] {
      refreshProgramWidgets();
    });
    connect(motionControlDialog_, &MotionControlDialog::motionLogGenerated, this,
            [this](const QString &message) { appendLog(message); });
  }

  if (const auto currentProgram = programManager_.currentProgram(); currentProgram.has_value()) {
    motionControlDialog_->setProgramBoardDefinition(currentProgram->boardDefinition);
  }

  motionControlDialog_->show();
  motionControlDialog_->raise();
  motionControlDialog_->activateWindow();
}

void MainWindow::openSpcDashboard() {
  if (spcWindow_ == nullptr) {
    spcWindow_ = new LaserSpcWindow();
  }

  spcWindow_->show();
  spcWindow_->raise();
  spcWindow_->activateWindow();
}

void MainWindow::openProductionHistory() {
  ProductionHistoryDialog dialog(&databaseManager_, this);
  dialog.exec();
}

void MainWindow::openMarkOffsetCalibration() {
  if (!virtualCamera_.isOpened()) {
    startCameraPreview();
  }

  MarkOffsetDialog dialog(this);
  dialog.setFrameProvider([this] { return currentCalibrationFrame(); },
                          [this] { return virtualCamera_.isOpened(); });
  if (const auto currentProgram = programManager_.currentProgram(); currentProgram.has_value()) {
    dialog.setAlignmentContext(currentProgram->marks, currentProgram->pixelScaleCalibration);
  }
  connect(&dialog, &MarkOffsetDialog::compensationApplied, this,
          [this](const double deltaXmm, const double deltaYmm, const double rotationDegrees) {
            updateProgram([deltaXmm, deltaYmm, rotationDegrees](ProgramModel &program) {
              program.runtimeSummary.hasMarkCalibration = true;
              program.runtimeSummary.markCalibrationOffsetXmm = deltaXmm;
              program.runtimeSummary.markCalibrationOffsetYmm = deltaYmm;
              program.runtimeSummary.markCalibrationRotationDegrees = rotationDegrees;
            });
            CalibrationRecord record;
            record.calibrationType = "mark_alignment";
            record.notes = "dX=" + std::to_string(deltaXmm) + ", dY=" + std::to_string(deltaYmm) +
                           ", dR=" + std::to_string(rotationDegrees);
            logCalibrationRecord(record);
            MechanicalPose pose = currentMechanicalPose();
            pose.x -= deltaXmm;
            pose.y -= deltaYmm;
            pose.r -= rotationDegrees;
            applyMechanicalPose(pose, QStringLiteral("Mark 校正补偿"));
            refreshTemplatePreviewSummary();
          });
  dialog.exec();
}

void MainWindow::openOriginCalibration() {
  if (!virtualCamera_.isOpened()) {
    startCameraPreview();
  }

  OriginCalibDialog dialog(this);
  dialog.setFrameProvider([this] { return currentCalibrationFrame(); },
                          [this] { return virtualCamera_.isOpened(); });
  if (const auto currentProgram = programManager_.currentProgram(); currentProgram.has_value()) {
    dialog.setCalibrationContext(currentProgram->calibrationData, currentMechanicalPose());
  } else {
    dialog.setCalibrationContext(CameraCalibrationData {}, currentMechanicalPose());
  }
  connect(&dialog, &OriginCalibDialog::correctedPoseApplied, this,
          [this](const double x, const double y, const double z, const double r) {
            const auto currentProgram = programManager_.currentProgram();
            const BoardDefinition boardDefinition =
                currentProgram.has_value() ? currentProgram->boardDefinition : BoardDefinition {};
            const MechanicalPose correctedCornerPose {x, y, z, r};
            const MechanicalPose logicalOriginPose {
                correctedCornerPose.x - boardDefinition.boardLengthMm,
                correctedCornerPose.y - boardDefinition.boardWidthMm,
                correctedCornerPose.z,
                correctedCornerPose.r,
            };

            updateProgram([logicalOriginPose](ProgramModel &program) {
              program.originCalibration.calibrated = true;
              program.originCalibration.machineReferencePose = logicalOriginPose;
              program.runtimeSummary.hasOriginCalibration = true;
              program.runtimeSummary.originCorrectedPose = logicalOriginPose;
            });
            CalibrationRecord record;
            record.calibrationType = "origin";
            record.originX = logicalOriginPose.x;
            record.originY = logicalOriginPose.y;
            record.originR = logicalOriginPose.r;
            record.notes = "Stopper-corner calibration converted to logical board origin";
            logCalibrationRecord(record);
            applyMechanicalPose(correctedCornerPose, QStringLiteral("原点校正对位"));
            appendLog(QStringLiteral("已将挡板右下角参考位换算为逻辑原点：X=%1, Y=%2")
                          .arg(logicalOriginPose.x, 0, 'f', 3)
                          .arg(logicalOriginPose.y, 0, 'f', 3));
            refreshTemplatePreviewSummary();
          });
  dialog.exec();
}

void MainWindow::openLaserOffsetCalibration() {
  if (!virtualCamera_.isOpened()) {
    startCameraPreview();
  }

  LaserOffsetCalibDialog dialog(this);
  dialog.setFrameProvider([this] { return currentCalibrationFrame(); },
                          [this] { return virtualCamera_.isOpened(); });

  const auto currentProgram = programManager_.currentProgram();
  PixelPoint opticalCenter {0.0, 0.0};
  if (currentProgram.has_value()) {
    if (selectedMarkIndex_ >= 0 && selectedMarkIndex_ < static_cast<int>(currentProgram->marks.size())) {
      const auto &selectedMark = currentProgram->marks[static_cast<std::size_t>(selectedMarkIndex_)];
      opticalCenter = PixelPoint {selectedMark.x, selectedMark.y};
    }
    dialog.setCalibrationContext(currentProgram->pixelScaleCalibration, opticalCenter);
  }

  connect(&dialog, &LaserOffsetCalibDialog::calibrationApplied, this,
          [this](const LaserOffsetCalibration &calibration) {
            updateProgram([calibration](ProgramModel &program) {
              program.laserOffsetCalibration = calibration;
              program.runtimeSummary.hasLaserOffsetCalibration = true;
            });
            CalibrationRecord record;
            record.calibrationType = "laser_offset";
            record.laserOffsetDx = calibration.cameraToLaserDxMm;
            record.laserOffsetDy = calibration.cameraToLaserDyMm;
            record.notes = "Laser offset calibration from dialog";
            logCalibrationRecord(record);
            appendLog(QStringLiteral("激光偏移校正完成：dX=%1 mm, dY=%2 mm")
                          .arg(calibration.cameraToLaserDxMm, 0, 'f', 4)
                          .arg(calibration.cameraToLaserDyMm, 0, 'f', 4));
            refreshTemplatePreviewSummary();
          });

  dialog.exec();
}

void MainWindow::loadBoardToTrack() {
  if (virtualTransportController_.loadBoard()) {
    appendLog(QStringLiteral("进板完成：%1").arg(QString::fromStdString(virtualTransportController_.lastSignalMessage())));
  } else {
    appendLog(QStringLiteral("进板失败。"));
  }
  refreshStatusSummary();
}

void MainWindow::unloadBoardFromTrack() {
  if (virtualTransportController_.unloadBoard()) {
    appendLog(QStringLiteral("出板完成：%1").arg(QString::fromStdString(virtualTransportController_.lastSignalMessage())));
  } else {
    appendLog(QStringLiteral("出板失败。"));
  }
  refreshStatusSummary();
}

BoardScanCaptureWorkflowResult MainWindow::executeWholeBoardScan(const bool refreshWorkbenchAfterScan) {
  const auto currentProgram = programManager_.currentProgram();
  if (!currentProgram.has_value()) {
    return BoardScanCaptureWorkflowResult::failure("Current program is missing.");
  }

  if (!virtualTransportController_.isBoardReady()) {
    return BoardScanCaptureWorkflowResult::failure("Board is not ready. Please load a board first.");
  }

  if (!virtualCamera_.isOpened()) {
    startCameraPreview();
  }

  const MechanicalPose originPose = logicalBoardReadyOriginPose(*currentProgram);
  BoardScanPlanner planner;
  const auto planResult = planner.plan(currentProgram->boardDefinition, currentProgram->scanRecipe, originPose);
  if (!planResult) {
    return BoardScanCaptureWorkflowResult::failure(planResult.message);
  }

  const QString scanRoot = projectFilePath(QStringLiteral("data/board_scans"));
  QDir().mkpath(scanRoot);
  const QString sanitizedProgramName =
      QString::fromStdString(currentProgram->name).trimmed().isEmpty()
          ? QStringLiteral("board_program")
          : QString::fromStdString(currentProgram->name).trimmed().replace(' ', '_');
  const QString scanSessionDir =
      QDir(scanRoot).filePath(QStringLiteral("%1_%2")
                                  .arg(sanitizedProgramName)
                                  .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))));
  QDir().mkpath(scanSessionDir);

  BoardScanExecutor executor;
  const auto executeResult = executor.execute(planResult.value, virtualMotionController_,
                                              [this, scanSessionDir](const FovCapturePose &pose) -> std::string {
                                                QImage tile = lastCameraFrameImage_;
                                                if (tile.isNull()) {
                                                  tile = placeholderScanTile(lastFrameSize_, pose);
                                                } else {
                                                  tile = annotateScanTile(tile, pose);
                                                }

                                                const QString tilePath =
                                                    QDir(scanSessionDir)
                                                        .filePath(QStringLiteral("tile_r%1_c%2_%3.png")
                                                                      .arg(pose.row)
                                                                      .arg(pose.column)
                                                                      .arg(pose.index));
                                                if (!tile.save(tilePath)) {
                                                  return {};
                                                }
                                                return tilePath.toStdString();
                                              },
                                              [this](const MechanicalPose &pose) {
                                                return virtualMotionSystem_.moveCameraPose(pose);
                                              });
  if (!executeResult) {
    return BoardScanCaptureWorkflowResult::failure(executeResult.message);
  }

  QImage firstTile(QString::fromStdString(executeResult.value.front().imagePath));
  if (firstTile.isNull()) {
    firstTile = placeholderScanTile(lastFrameSize_, executeResult.value.front().pose);
  }

  const QString mosaicPath = QDir(scanSessionDir).filePath(QStringLiteral("whole_board_mosaic.png"));

  BoardStitcher stitcher;
  const auto layoutResult = stitcher.buildLayout(planResult.value, firstTile.width(), firstTile.height());
  if (!layoutResult) {
    return BoardScanCaptureWorkflowResult::failure(layoutResult.message);
  }

#ifdef AOI_HAS_OPENCV
  const auto stitchResult = stitcher.stitch(layoutResult.value, executeResult.value, mosaicPath.toStdString());
  if (stitchResult.capturedTileCount > 0) {
    const QString summary =
        QStringLiteral("整板扫描完成（OpenCV 拼接）：%1 张 FOV，%2 行 x %3 列，输出=%4")
            .arg(stitchResult.capturedTileCount)
            .arg(planResult.value.tileRows)
            .arg(planResult.value.tileColumns)
            .arg(mosaicPath);
    return BoardScanCaptureWorkflowResult::success(
        BoardScanCaptureResult {stitchResult.mosaicImagePath, stitchResult.tileRows,
                                stitchResult.tileColumns, stitchResult.capturedTileCount,
                                summary.toStdString(), executeResult.value},
        summary.toStdString());
  }
  // OpenCV stitch failed; fall through to QPainter path.
  appendLog(QStringLiteral("OpenCV 拼接失败，回退到 QPainter 拼接：%1")
                .arg(QString::fromStdString(stitchResult.summary)));
#endif

  // QPainter fallback path (also used when OpenCV is unavailable).
  {
    QImage mosaic(layoutResult.value.mosaicPixelWidth, layoutResult.value.mosaicPixelHeight,
                  QImage::Format_ARGB32_Premultiplied);
    mosaic.fill(QColor("#020617"));
    QPainter painter(&mosaic);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    for (const auto &placement : layoutResult.value.placements) {
      const QString tilePath =
          QString::fromStdString(executeResult.value[static_cast<std::size_t>(placement.pose.index)].imagePath);
      QImage tile(tilePath);
      if (tile.isNull()) {
        tile = placeholderScanTile(QSize(placement.pixelWidth, placement.pixelHeight), placement.pose);
      }
      painter.drawImage(QRect(placement.pixelX, placement.pixelY, placement.pixelWidth, placement.pixelHeight), tile);
      painter.setPen(QPen(QColor("#facc15"), 1, Qt::DashLine));
      painter.drawRect(QRect(placement.pixelX, placement.pixelY, placement.pixelWidth, placement.pixelHeight));
      painter.setPen(QColor("#e2e8f0"));
      painter.drawText(QRect(placement.pixelX + 10, placement.pixelY + 10, 160, 24),
                       QStringLiteral("r%1 c%2").arg(placement.pose.row).arg(placement.pose.column));
    }
    painter.end();

    if (!mosaic.save(mosaicPath)) {
      return BoardScanCaptureWorkflowResult::failure("Failed to save stitched whole-board mosaic.");
    }
  }

  const QString summary =
      QStringLiteral("整板扫描完成（QPainter）：%1 张 FOV，%2 行 x %3 列，输出=%4")
          .arg(executeResult.value.size())
          .arg(planResult.value.tileRows)
          .arg(planResult.value.tileColumns)
          .arg(mosaicPath);

  updateProgram([&](ProgramModel &program) {
    program.runtimeSummary.wholeBoardImagePath = mosaicPath.toStdString();
    program.runtimeSummary.scanTileRows = planResult.value.tileRows;
    program.runtimeSummary.scanTileColumns = planResult.value.tileColumns;
    program.runtimeSummary.lastBoardScanSummary = summary.toStdString();
  });

  if (refreshWorkbenchAfterScan) {
    refreshWorkbenchScene(false);
    resetWorkbenchView();
    refreshStatusSummary();
  }

  BoardScanCaptureResult result;
  result.mosaicImagePath = mosaicPath.toStdString();
  result.tileRows = planResult.value.tileRows;
  result.tileColumns = planResult.value.tileColumns;
  result.capturedTileCount = static_cast<int>(executeResult.value.size());
  result.summary = summary.toStdString();
  result.capturedTiles = executeResult.value;
  return BoardScanCaptureWorkflowResult::success(std::move(result), summary.toStdString());
}

void MainWindow::runWholeBoardScan() {
  syncProgramFromEditors();

  const auto result = executeWholeBoardScan(true);
  if (!result) {
    appendLog(QStringLiteral("整板扫描失败：%1").arg(QString::fromStdString(result.message)));
    return;
  }

  appendLog(QString::fromStdString(result.value.summary));
}

void MainWindow::logCalibrationRecord(const CalibrationRecord &record) {
  if (!databaseManager_.isOpen()) {
    return;
  }

  const auto insertResult = databaseManager_.insertCalibrationRecord(record);
  if (!insertResult) {
    appendLog(QStringLiteral("标定记录写入失败：%1").arg(QString::fromStdString(insertResult.message)));
  }
}

void MainWindow::openLogWindow() {
  if (logWindow_ == nullptr) {
    return;
  }

  if (!logWindow_->isVisible()) {
    logWindow_->show();
  } else {
    logWindow_->raise();
    logWindow_->activateWindow();
  }
}

void MainWindow::openDataCollect() {
  if (dataCollectDialog_ == nullptr) {
    dataCollectDialog_ = new DataCollectDialog(this);
    dataCollectDialog_->setCaptureDirectory(projectRootPath() + QStringLiteral("/data/template_cache"));

    connect(dataCollectDialog_, &DataCollectDialog::captureRequested, this, [this]() {
      // Capture current camera frame and add to dataset.
      if (!lastCameraFrameImage_.isNull()) {
        const QString dir = projectRootPath() + QStringLiteral("/data/template_cache");
        QDir().mkpath(dir);
        const QString path = dir + QStringLiteral("/capture_") +
                             QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")) +
                             QStringLiteral(".png");
        lastCameraFrameImage_.save(path, "PNG");
        dataCollectDialog_->addCapturedImage(path, QStringLiteral("captured"));
        appendLog(QStringLiteral("Dataset image saved: ") + path);
      } else {
        appendLog(QStringLiteral("No camera frame available for dataset capture."));
      }
    });

    connect(dataCollectDialog_, &DataCollectDialog::exportRequested, this,
            [this](const QString &exportPath) {
              appendLog(QStringLiteral("Dataset export requested to: ") + exportPath);
            });
  }

  if (!dataCollectDialog_->isVisible()) {
    dataCollectDialog_->show();
  } else {
    dataCollectDialog_->raise();
    dataCollectDialog_->activateWindow();
  }
}

void MainWindow::openMarkEditDialog() {
  if (markEditDialog_ == nullptr) {
    markEditDialog_ = new MarkEditDialog(this);
    connect(markEditDialog_, &QDialog::accepted, this, [this]() {
      const MarkPoint mark = markEditDialog_->markPoint();
      updateProgram([&mark](ProgramModel &program) {
        program.marks.push_back(mark);

        // Update compatibility fields.
        if (!mark.name.empty()) {
          program.markReferences.push_back(MarkReferenceRecord {
              mark.name,
              PixelPoint {mark.x, mark.y},
              MillimeterPoint {mark.x * program.pixelScaleCalibration.pixelToMillimeterX,
                               mark.y * program.pixelScaleCalibration.pixelToMillimeterY},
              mark.enabled,
          });
        }
      });

      appendLog(QStringLiteral("Mark 点已添加: ") + QString::fromStdString(mark.name));
      refreshProgramWidgets();
      refreshWorkbenchScene();
    });
  }

  if (!markEditDialog_->isVisible()) {
    markEditDialog_->show();
  } else {
    markEditDialog_->raise();
    markEditDialog_->activateWindow();
  }
}

void MainWindow::openSettings() {
  if (settingsDialog_ == nullptr) {
    settingsDialog_ = new SettingsDialog(this);
    connect(settingsDialog_, &QDialog::accepted, this, [this] {
      appSettings_ = settingsDialog_->settings();
      AppSettingsManager::save(appSettings_, projectFilePath(QStringLiteral("config/app_settings.json")).toStdString());

      if (logWindow_ != nullptr) {
        logWindow_->setPersistEnabled(appSettings_.persistLogs,
                                      QString::fromStdString(appSettings_.logFilePath));
      }

      appendLog(QStringLiteral("系统设置已更新并保存。"));
    });
  }

  settingsDialog_->setSettings(appSettings_);
  settingsDialog_->exec();
}

void MainWindow::startCameraPreview() {
  if (virtualCamera_.isOpened()) {
    return;
  }

  const QSize frameSize = parseResolutionPreset(cameraResolutionPreset_);
  virtualCamera_.setPreferredFrameSize(frameSize.width(), frameSize.height());
  virtualCamera_.setBoardImagePath(projectFilePath(QStringLiteral("demoimage/board.png")).toStdString());

  if (const auto currentProgram = programManager_.currentProgram(); currentProgram.has_value()) {
    virtualCamera_.setBoardDefinition(currentProgram->boardDefinition);
    virtualCamera_.setScanRecipe(currentProgram->scanRecipe);
    virtualCamera_.setBoardReadyOriginPose(logicalBoardReadyOriginPose(*currentProgram));
  } else {
    virtualCamera_.setBoardDefinition(BoardDefinition {});
    virtualCamera_.setScanRecipe(ScanRecipe {});
    virtualCamera_.clearBoardReadyOriginPose();
  }

  virtualCamera_.setCurrentPose(currentMechanicalPose());
  virtualCamera_.setTransportState(
      virtualTransportController_.state(),
      virtualTransportController_.boardPosition(),
      virtualTransportController_.stopperTargetPosition());

  if (!virtualCamera_.open(cameraDeviceIndex_)) {
    appendLog(QStringLiteral("虚拟相机启动失败，整板图路径=%1。")
                  .arg(projectFilePath(QStringLiteral("demoimage/board.png"))));
    refreshCameraState();
    return;
  }

  cameraTimer_->start();
  appendLog(QStringLiteral("虚拟整板相机已启动。"));
  refreshCameraState();
  updateCameraFrame();
}

void MainWindow::stopCameraPreview() {
  if (!virtualCamera_.isOpened()) {
    refreshCameraState();
    return;
  }

  cameraTimer_->stop();
  virtualCamera_.close();
  lastCameraFrameImage_ = QImage();
  appendLog(QStringLiteral("虚拟整板相机已停止。"));
  refreshCameraState();
  refreshWorkbenchScene();
}

void MainWindow::updateCameraFrame() {
  virtualCamera_.setCurrentPose(currentMechanicalPose());
  virtualCamera_.setTransportState(
      virtualTransportController_.state(),
      virtualTransportController_.boardPosition(),
      virtualTransportController_.stopperTargetPosition());

  const CameraFrame frame = virtualCamera_.grabFrame();
  if (frame.width <= 0 || frame.height <= 0) {
    return;
  }

  lastCameraFrameImage_ = cameraFrameToQImage(frame);
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

void MainWindow::toggleCodeCameraView() {
  showCodeCameraView_ = !showCodeCameraView_;
  appendLog(showCodeCameraView_ ? QStringLiteral("已在 FOV 窗口内打开读码相机视图。")
                                : QStringLiteral("已关闭 FOV 窗口内的读码相机视图。"));
  refreshCameraState();
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
  const auto currentProgram = programManager_.currentProgram();
  const auto layout = currentBoardSceneLayout(currentProgram);
  if (!layout.has_value()) {
    appendLog(QStringLiteral("当前无法计算板坐标布局，Mark 创建失败。"));
    return;
  }

  const auto boardRect = BoardViewTransform::sceneToBoardRect(*layout, toBoardSceneRect(sceneRect));
  if (!boardRect.has_value()) {
    appendLog(QStringLiteral("框选区域不在板范围内，Mark 创建失败。"));
    return;
  }

  updateProgram([this, boardRect](ProgramModel &program) {
    MarkPoint mark;
    mark.name = markNameLineEdit_->text().isEmpty() ? QStringLiteral("Mark-%1").arg(program.marks.size() + 1).toStdString()
                                                    : markNameLineEdit_->text().toStdString();
    mark.shape = markShapeFromDisplayText(currentMarkShapeText());
    mark.algorithm = markAlgorithmFromDisplayText(currentMarkAlgorithmText());
    mark.sampledColor = markColorLineEdit_->text().toStdString();
    mark.minimumScore = markMinScoreSpinBox_->value();
    mark.width = boardRect->width;
    mark.height = boardRect->height;
    mark.rotation = markRotationSpinBox_->value();
    mark.x = boardRect->x + boardRect->width / 2.0;
    mark.y = boardRect->y + boardRect->height / 2.0;
    mark.previewScore = calculatePreviewScore(mark);
    mark.score = calculateLiveScore(mark, lastFrameSize_);
    program.marks.push_back(mark);
    selectedMarkIndex_ = static_cast<int>(program.marks.size()) - 1;
    selectedRoiIndex_ = -1;
  });

  setCurrentPage(1);
  appendLog(QStringLiteral("已在整板坐标系中生成 Mark：中心=(%1, %2) mm。")
                .arg(boardRect->x + boardRect->width / 2.0, 0, 'f', 2)
                .arg(boardRect->y + boardRect->height / 2.0, 0, 'f', 2));
}

void MainWindow::addRoiFromSceneRect(const QRectF &sceneRect) {
  const auto currentProgram = programManager_.currentProgram();
  const auto layout = currentBoardSceneLayout(currentProgram);
  if (!layout.has_value()) {
    appendLog(QStringLiteral("当前无法计算板坐标布局，ROI 创建失败。"));
    return;
  }

  const auto boardRect = BoardViewTransform::sceneToBoardRect(*layout, toBoardSceneRect(sceneRect));
  if (!boardRect.has_value()) {
    appendLog(QStringLiteral("框选区域不在板范围内，ROI 创建失败。"));
    return;
  }

  updateProgram([this, boardRect](ProgramModel &program) {
    RoiRegion roi;
    roi.name = roiNameLineEdit_->text().isEmpty() ? QStringLiteral("ROI-%1").arg(program.rois.size() + 1).toStdString()
                                                  : roiNameLineEdit_->text().toStdString();
    roi.shape = roiShapeFromDisplayText(currentRoiShapeText());
    roi.threshold = roiThresholdSpinBox_->value();
    roi.rotation = roiRotationSpinBox_->value();
    roi.x = boardRect->x;
    roi.y = boardRect->y;
    roi.width = boardRect->width;
    roi.height = boardRect->height;
    program.rois.push_back(roi);
    selectedRoiIndex_ = static_cast<int>(program.rois.size()) - 1;
    selectedMarkIndex_ = -1;
  });

  setCurrentPage(0);
  appendLog(QStringLiteral("已在整板坐标系中生成 ROI：左上=(%1, %2) mm。")
                .arg(boardRect->x, 0, 'f', 2)
                .arg(boardRect->y, 0, 'f', 2));
}

void MainWindow::selectMarkIndex(const int index) {
  selectedMarkIndex_ = index;
  selectedRoiIndex_ = -1;
  refreshTableSelections();
  updateMarkEditorFromSelection();
  refreshTemplatePreviewSummary();
  refreshWorkbenchScene();
}

void MainWindow::selectRoiIndex(const int index) {
  selectedRoiIndex_ = index;
  selectedMarkIndex_ = -1;
  refreshTableSelections();
  updateRoiEditorFromSelection();
  refreshTemplatePreviewSummary();
  refreshWorkbenchScene();
}

void MainWindow::applyMarkEditorToSelection() {
  if (selectedMarkIndex_ < 0) {
    return;
  }

  const auto currentProgram = programManager_.currentProgram();
  if (!currentProgram.has_value()) {
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
    const MillimeterPoint clampedCenter = BoardViewTransform::clampCenteredPoint(
        program.boardDefinition, MillimeterPoint {mark.x, mark.y}, mark.width, mark.height);
    mark.x = clampedCenter.x;
    mark.y = clampedCenter.y;
    mark.previewScore = calculatePreviewScore(mark);
    mark.score = calculateLiveScore(mark, lastFrameSize_);
  });

  appendLog(QStringLiteral("选中的 Mark 参数已更新。"));
}

void MainWindow::applyRoiEditorToSelection() {
  if (selectedRoiIndex_ < 0) {
    return;
  }

  const auto currentProgram = programManager_.currentProgram();
  if (!currentProgram.has_value()) {
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
    const MillimeterPoint clampedTopLeft =
        BoardViewTransform::clampTopLeftPoint(program.boardDefinition, MillimeterPoint {roi.x, roi.y}, roi.width, roi.height);
    roi.x = clampedTopLeft.x;
    roi.y = clampedTopLeft.y;
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
    auto mark = currentProgram->marks[static_cast<std::size_t>(selectedMarkIndex_)];
    QString detectSummary = QStringLiteral("当前未接入实时检测");

    if (!lastCameraFrameImage_.isNull()) {
      const QString tempImagePath = persistFrameToTempFile(lastCameraFrameImage_, QStringLiteral("mark_preview"));
      if (!tempImagePath.isEmpty()) {
        MarkDetector detector;
        const auto detectResult = mark.algorithm == MarkAlgorithm::BinaryGeometry
                                      ? detector.detectCircularMarks(tempImagePath.toStdString())
                                      : detector.detectTemplateMarks(tempImagePath.toStdString());
        if (detectResult && !detectResult.value.empty()) {
          const auto &detectedMark = detectResult.value.front();
          mark.previewScore = std::clamp(detectedMark.previewScore, 0.0, 1.0);
          mark.score = std::clamp(detectedMark.score, 0.0, 1.0);
          detectSummary = QStringLiteral("检测到 %1 个候选，采用第一个候选：中心=(%2, %3)，检测分=%4")
                              .arg(static_cast<int>(detectResult.value.size()))
                              .arg(detectedMark.x, 0, 'f', 1)
                              .arg(detectedMark.y, 0, 'f', 1)
                              .arg(detectedMark.score, 0, 'f', 3);
        } else if (!detectResult) {
          detectSummary = QStringLiteral("实时检测失败：%1").arg(QString::fromStdString(detectResult.message));
        } else {
          detectSummary = QStringLiteral("实时检测未返回任何候选");
        }
      }
    }

    updateProgram([this, &mark, detectSummary](ProgramModel &program) {
      if (selectedMarkIndex_ >= 0 && selectedMarkIndex_ < static_cast<int>(program.marks.size())) {
        program.marks[static_cast<std::size_t>(selectedMarkIndex_)].previewScore = mark.previewScore;
        program.marks[static_cast<std::size_t>(selectedMarkIndex_)].score = mark.score;
        program.runtimeSummary.latestTemplateMatchSummary = detectSummary.toStdString();
      }
    });

    markPreviewScoreSpinBox_->setValue(mark.previewScore);
    markLiveScoreSpinBox_->setValue(mark.score);
    markPreviewProgressBar_->setValue(static_cast<int>(std::round(mark.previewScore * 100.0)));
    markPreviewScoreValueLabel_->setText(QString::number(mark.previewScore, 'f', 3));
    markLiveScoreValueLabel_->setText(QString::number(mark.score, 'f', 3));
    const bool pass = mark.score >= mark.minimumScore;
    markLiveScoreValueLabel_->setStyleSheet(pass ? QStringLiteral("color: #16a34a; font-weight: 700;")
                                                 : QStringLiteral("color: #dc2626; font-weight: 700;"));
    refreshTemplatePreviewSummary();
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

void MainWindow::captureCurrentTemplateImage() {
  if (lastCameraFrameImage_.isNull()) {
    appendLog(QStringLiteral("当前没有可用的实时图像，无法抓取模板图。"));
    return;
  }

  QString targetName = QStringLiteral("template_capture");
  if (const auto currentProgram = programManager_.currentProgram(); currentProgram.has_value()) {
    if (selectedMarkIndex_ >= 0 && selectedMarkIndex_ < static_cast<int>(currentProgram->marks.size())) {
      targetName = QString::fromStdString(currentProgram->marks[static_cast<std::size_t>(selectedMarkIndex_)].name);
    } else if (selectedRoiIndex_ >= 0 && selectedRoiIndex_ < static_cast<int>(currentProgram->rois.size())) {
      targetName = QString::fromStdString(currentProgram->rois[static_cast<std::size_t>(selectedRoiIndex_)].name);
    }
  }

  QString templateRoot = appSettings_.templateFolderPath.empty()
                             ? projectFilePath(QStringLiteral("data/template_cache"))
                             : QString::fromStdString(appSettings_.templateFolderPath);
  QDir().mkpath(templateRoot);
  const QString outputPath = QDir(templateRoot).filePath(
      QStringLiteral("%1_%2.png").arg(targetName).arg(QDateTime::currentMSecsSinceEpoch()));

  if (!lastCameraFrameImage_.save(outputPath)) {
    appendLog(QStringLiteral("模板图抓取失败：%1").arg(outputPath));
    return;
  }

  lastTemplateCapturePath_ = outputPath;

  updateProgram([this, outputPath](ProgramModel &program) {
    program.runtimeSummary.templateCachePath = outputPath.toStdString();

    // Store the template path in the currently selected ROI's detector config
    // so it can be used for per-ROI template matching during inspection.
    if (selectedRoiIndex_ >= 0 &&
        selectedRoiIndex_ < static_cast<int>(program.roiDetectorConfigs.size())) {
      auto &config = program.roiDetectorConfigs[static_cast<std::size_t>(selectedRoiIndex_)];
      if (config.detectorType == RoiDetectorType::Template) {
        config.templateImagePath = outputPath.toStdString();
      }
    }
  });
  refreshTemplatePreviewSummary();
  appendLog(QStringLiteral("已抓取当前模板图：%1").arg(outputPath));
}

void MainWindow::centerOnCurrentSelection() {
  if (const auto currentProgram = programManager_.currentProgram();
      currentProgram.has_value() && currentBoardSceneLayout(currentProgram).has_value()) {
    const auto layout = *currentBoardSceneLayout(currentProgram);
    if (selectedMarkIndex_ >= 0 && selectedMarkIndex_ < static_cast<int>(currentProgram->marks.size())) {
      const auto &mark = currentProgram->marks[static_cast<std::size_t>(selectedMarkIndex_)];
      const auto scenePoint = BoardViewTransform::boardToScenePoint(layout, MillimeterPoint {mark.x, mark.y});
      workbenchGraphicsView_->centerOn(scenePoint.x, scenePoint.y);
      return;
    }

    if (selectedRoiIndex_ >= 0 && selectedRoiIndex_ < static_cast<int>(currentProgram->rois.size())) {
      const auto &roi = currentProgram->rois[static_cast<std::size_t>(selectedRoiIndex_)];
      const auto sceneRect =
          BoardViewTransform::boardToSceneTopLeftRect(layout, MillimeterPoint {roi.x, roi.y}, roi.width, roi.height);
      workbenchGraphicsView_->centerOn(sceneRect.x + sceneRect.width / 2.0, sceneRect.y + sceneRect.height / 2.0);
    }
  }
}

void MainWindow::updateCursorCoordinate(const QPointF &scenePos) {
  QString cursorText = QStringLiteral("Scene X=%1 Y=%2").arg(scenePos.x(), 0, 'f', 1).arg(scenePos.y(), 0, 'f', 1);
  if (const auto layout = currentBoardSceneLayout(programManager_.currentProgram()); layout.has_value()) {
    if (const auto boardPoint = BoardViewTransform::sceneToBoardPoint(*layout, toBoardScenePoint(scenePos), false);
        boardPoint.has_value()) {
      cursorText = QStringLiteral("板坐标 X=%1 mm Y=%2 mm").arg(boardPoint->x, 0, 'f', 2).arg(boardPoint->y, 0, 'f', 2);
    }
  }
  cursorPositionValueLabel_->setText(cursorText);
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
  const auto currentProgram = programManager_.currentProgram();
  const auto layout = currentBoardSceneLayout(currentProgram);
  if (!currentProgram.has_value() || !layout.has_value() || selectedMarkIndex_ < 0 ||
      selectedMarkIndex_ >= static_cast<int>(currentProgram->marks.size())) {
    return;
  }

  const auto &mark = currentProgram->marks[static_cast<std::size_t>(selectedMarkIndex_)];
  const auto boardPoint = BoardViewTransform::sceneToBoardPoint(*layout, toBoardScenePoint(centerScenePos));
  if (!boardPoint.has_value()) {
    return;
  }

  const MillimeterPoint clampedCenter =
      BoardViewTransform::clampCenteredPoint(currentProgram->boardDefinition, *boardPoint, mark.width, mark.height);
  updateProgram([this, clampedCenter](ProgramModel &program) {
    if (selectedMarkIndex_ >= 0 && selectedMarkIndex_ < static_cast<int>(program.marks.size())) {
      auto &currentMark = program.marks[static_cast<std::size_t>(selectedMarkIndex_)];
      currentMark.x = clampedCenter.x;
      currentMark.y = clampedCenter.y;
    }
  });
  appendLog(QStringLiteral("Mark 位置已更新到板坐标 X=%1 mm, Y=%2 mm。")
                .arg(clampedCenter.x, 0, 'f', 2)
                .arg(clampedCenter.y, 0, 'f', 2));
}

void MainWindow::updateSelectedRoiFromScene(const QPointF &topLeftScenePos) {
  const auto currentProgram = programManager_.currentProgram();
  const auto layout = currentBoardSceneLayout(currentProgram);
  if (!currentProgram.has_value() || !layout.has_value() || selectedRoiIndex_ < 0 ||
      selectedRoiIndex_ >= static_cast<int>(currentProgram->rois.size())) {
    return;
  }

  const auto &roi = currentProgram->rois[static_cast<std::size_t>(selectedRoiIndex_)];
  const auto boardPoint = BoardViewTransform::sceneToBoardPoint(*layout, toBoardScenePoint(topLeftScenePos));
  if (!boardPoint.has_value()) {
    return;
  }

  const MillimeterPoint clampedTopLeft =
      BoardViewTransform::clampTopLeftPoint(currentProgram->boardDefinition, *boardPoint, roi.width, roi.height);
  updateProgram([this, clampedTopLeft](ProgramModel &program) {
    if (selectedRoiIndex_ >= 0 && selectedRoiIndex_ < static_cast<int>(program.rois.size())) {
      auto &currentRoi = program.rois[static_cast<std::size_t>(selectedRoiIndex_)];
      currentRoi.x = clampedTopLeft.x;
      currentRoi.y = clampedTopLeft.y;
    }
  });
  appendLog(QStringLiteral("ROI 位置已更新到板坐标 X=%1 mm, Y=%2 mm。")
                .arg(clampedTopLeft.x, 0, 'f', 2)
                .arg(clampedTopLeft.y, 0, 'f', 2));
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

QImage MainWindow::currentCalibrationFrame() const {
  return lastCameraFrameImage_;
}

MechanicalPose MainWindow::currentMechanicalPose() const {
  return virtualMotionSystem_.currentCameraPose();
}

void MainWindow::applyMechanicalPose(const MechanicalPose &pose, const QString &reason) {
  const bool moveOk = virtualMotionSystem_.moveCameraPose(pose);
  refreshProgramWidgets();
  appendLog((moveOk ? QStringLiteral("%1已应用到虚拟运控：X=%2, Y=%3, Z=%4, R=%5")
                     : QStringLiteral("%1应用到虚拟运控失败：X=%2, Y=%3, Z=%4, R=%5"))
                .arg(reason)
                .arg(pose.x, 0, 'f', 3)
                .arg(pose.y, 0, 'f', 3)
                .arg(pose.z, 0, 'f', 3)
                .arg(pose.r, 0, 'f', 3));
}

void MainWindow::tickVirtualDevices() {
  constexpr double kSimulationDtSec = 0.016;
  virtualMotionSystem_.tick(kSimulationDtSec);
}

QString MainWindow::cameraModeText() const {
  return QStringLiteral("虚拟整板相机 + CAD 叠加");
}

QString MainWindow::boardTransportStateText() const {
  switch (virtualTransportController_.state()) {
  case BoardTransportState::Idle:
    return QStringLiteral("待进板");
  case BoardTransportState::Loading:
    return QStringLiteral("进板中");
  case BoardTransportState::BoardReady:
    return QStringLiteral("板到位");
  case BoardTransportState::Unloading:
    return QStringLiteral("出板中");
  }
  return QStringLiteral("未知");
}

QString MainWindow::motionStateText() const {
  const QString axisText = virtualMotionController_.isStopped() ? QStringLiteral("轴急停") : QStringLiteral("轴就绪");
  return QStringLiteral("%1 / %2").arg(axisText, boardTransportStateText());
}

QString MainWindow::runModeOverlayText() const {
  QString workflowStateText = QStringLiteral("待启动");
  if (appMode_ != AppMode::Run) {
    workflowStateText = QStringLiteral("编辑模式");
  } else if (workflowRunning_) {
    workflowStateText = QStringLiteral("运行中");
  } else if (workflowContext_.nextStepIndex > 0 || workflowContext_.boardIndex > 0 ||
             workflowContext_.okCount > 0 || workflowContext_.ngCount > 0) {
    workflowStateText = QStringLiteral("已暂停");
  }

  const int totalBoards = workflowContext_.totalBoards > 0 ? workflowContext_.totalBoards : 10;
  const int boardDisplayIndex =
      totalBoards > 0 ? qBound(1, workflowContext_.boardIndex + 1, totalBoards) : workflowContext_.boardIndex + 1;
  const MechanicalPose pose = currentMechanicalPose();
  const VirtualCameraPoseInfo cardPoseInfo = virtualCamera_.lastPoseInfo();

  QString boardSizeText = QStringLiteral("板尺寸未加载");
  if (const auto currentProgram = programManager_.currentProgram(); currentProgram.has_value()) {
    const auto &definition = currentProgram->boardDefinition;
    boardSizeText = QStringLiteral("%1 x %2 mm | 轨宽 %3 mm")
                        .arg(definition.boardLengthMm, 0, 'f', 1)
                        .arg(definition.boardWidthMm, 0, 'f', 1)
                        .arg(definition.railWidthMm, 0, 'f', 1);
  }

  const QString stepCounter =
      workflowTotalSteps_ > 0 ? QStringLiteral("%1/%2").arg(workflowStepIndex_).arg(workflowTotalSteps_)
                              : QStringLiteral("--/--");
  const QString stepSummary =
      workflowStepSummary_.trimmed().isEmpty() ? QStringLiteral("等待启动") : workflowStepSummary_.trimmed();

  return QStringLiteral("流程：%1 | 步骤 %2\n"
                        "当前步骤：%3\n"
                        "板状态：%4 | 板 %5/%6 | OK %7 NG %8\n"
                        "轴位：X=%9  Y=%10  Z=%11  R=%12\n"
                        "卡坐标：X=%13  Y=%14\n"
                        "板参数：%15")
      .arg(workflowStateText)
      .arg(stepCounter)
      .arg(stepSummary)
      .arg(boardTransportStateText())
      .arg(boardDisplayIndex)
      .arg(totalBoards)
      .arg(workflowContext_.okCount)
      .arg(workflowContext_.ngCount)
      .arg(pose.x, 0, 'f', 3)
      .arg(pose.y, 0, 'f', 3)
      .arg(pose.z, 0, 'f', 3)
      .arg(pose.r, 0, 'f', 3)
      .arg(cardPoseInfo.physicalCardXmm, 0, 'f', 3)
      .arg(cardPoseInfo.physicalCardYmm, 0, 'f', 3)
      .arg(boardSizeText);
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

void MainWindow::buildRunInterface() {
  runModeWidget_ = new RunModeWidget(mainStackedWidget_);
  runModeWidget_->setFrameProvider([this] { return currentCalibrationFrame(); });
  runModeWidget_->setStatusTextProvider([this] { return runModeOverlayText(); });
  runModeWidget_->setTotalBoards(10);

  connect(runModeWidget_, &RunModeWidget::startRequested, this, &MainWindow::startWorkflowRun);
  connect(runModeWidget_, &RunModeWidget::stopRequested, this, &MainWindow::stopWorkflowRun);
  connect(runModeWidget_, &RunModeWidget::pauseRequested, this, &MainWindow::pauseWorkflowRun);
  connect(runModeWidget_, &RunModeWidget::singleStepRequested, this, &MainWindow::runSingleWorkflowStep);
}

void MainWindow::switchToEditorMode() {
  appMode_ = AppMode::Editor;
  stopWorkflowRun();

  if (mainStackedWidget_ != nullptr) {
    mainStackedWidget_->setCurrentIndex(0);
  }

  if (switchToRunButton_ != nullptr) {
    switchToRunButton_->setVisible(true);
  }

  if (switchToEditorButton_ != nullptr) {
    switchToEditorButton_->setVisible(false);
  }

  appendLog(QStringLiteral("已切换到编辑模式。"));
  refreshStatusSummary();
}

void MainWindow::switchToRunMode() {
  appMode_ = AppMode::Run;

  // Ensure camera is running
  if (!virtualCamera_.isOpened()) {
    startCameraPreview();
  }

  // Initialize workflow context
  const auto currentProgram = programManager_.currentProgram();
  workflowContext_ = WorkflowContext {};
  if (currentProgram.has_value()) {
    // We need a mutable pointer for the workflow; use the program manager's non-const access.
    workflowContext_.program = programManager_.mutableProgram();
  }
  workflowContext_.motionController = &virtualMotionController_;
  workflowContext_.laserController = &virtualLaserController_;
  workflowContext_.transportController = &virtualTransportController_;
  workflowContext_.databaseManager = &databaseManager_;
  workflowContext_.reuseInjectedInputs = false;
  workflowContext_.boardReady = virtualTransportController_.isBoardReady();
  workflowContext_.totalBoards = 10;
  workflowContext_.boardIndex = runModeWidget_->currentBoardIndex();

  workflowStepIndex_ = 0;
  workflowTotalSteps_ = static_cast<int>(processEngine_.stepCount());
  workflowStepSummary_ = QStringLiteral("等待启动");
  workflowRunning_ = false;

  // Wire callbacks
  workflowContext_.onStepProgress = [this](const int stepIdx, const int totalSteps,
                                           const std::string &stepName, const StepExecutionStatus status) {
    QString statusText;
    switch (status) {
    case StepExecutionStatus::Running:
      statusText = QStringLiteral("执行中");
      break;
    case StepExecutionStatus::Succeeded:
      statusText = QStringLiteral("完成");
      break;
    case StepExecutionStatus::Failed:
      statusText = QStringLiteral("失败");
      break;
    case StepExecutionStatus::Skipped:
      statusText = QStringLiteral("跳过");
      break;
    default:
      statusText = QStringLiteral("等待");
      break;
    }

    workflowStepSummary_ = QStringLiteral("步骤 %1/%2: %3 [%4]")
                               .arg(stepIdx)
                               .arg(totalSteps)
                               .arg(QString::fromStdString(stepName), statusText);
    runModeWidget_->updateStepProgress(workflowStepSummary_, stepIdx, totalSteps);
  };

  workflowContext_.onLog = [this](const std::string &message) {
    runModeWidget_->appendProductionLog(QString::fromStdString(message));
    appendLog(QString::fromStdString(message));
  };

  workflowContext_.onBoardResult = [this](const int boardIdx, const bool ok, const std::string & /* summary */) {
    runModeWidget_->recordBoardResult(ok);
  };

  workflowContext_.captureFrame = [this]() -> std::string {
    if (lastCameraFrameImage_.isNull()) {
      return {};
    }

    const QString tempDir = appSettings_.templateFolderPath.empty()
                                ? projectFilePath(QStringLiteral("data/template_cache"))
                                : QString::fromStdString(appSettings_.templateFolderPath);
    QDir().mkpath(tempDir);
    const QString path =
        QDir(tempDir).filePath(QStringLiteral("capture_%1.png").arg(QDateTime::currentMSecsSinceEpoch()));
    if (lastCameraFrameImage_.save(path)) {
      return path.toStdString();
    }

    return {};
  };
  workflowContext_.captureWholeBoardScan = [this]() -> BoardScanCaptureWorkflowResult {
    return executeWholeBoardScan(false);
  };
  workflowContext_.moveCameraPose = [this](const MechanicalPose &pose, const std::string &) {
    return virtualMotionSystem_.moveCameraPose(pose);
  };

  if (mainStackedWidget_ != nullptr) {
    mainStackedWidget_->setCurrentIndex(1);
  }

  if (switchToRunButton_ != nullptr) {
    switchToRunButton_->setVisible(false);
  }

  if (switchToEditorButton_ != nullptr) {
    switchToEditorButton_->setVisible(true);
  }

  runModeWidget_->appendProductionLog(QStringLiteral("已进入运行模式，等待启动。"));
  runModeWidget_->fitPreviewContent();
  appendLog(QStringLiteral("已切换到运行模式。"));
  refreshStatusSummary();
}

namespace {

QString stepTypeDisplayName(const ProcessStepType type) {
  switch (type) {
  case ProcessStepType::LoadBoard:      return QStringLiteral("进板");
  case ProcessStepType::RoughPosition:  return QStringLiteral("粗定位");
  case ProcessStepType::ImageCapture:   return QStringLiteral("图像采集");
  case ProcessStepType::MarkAlign:      return QStringLiteral("Mark 定位");
  case ProcessStepType::DefectInspect:  return QStringLiteral("缺陷检测");
  case ProcessStepType::PreLaser:       return QStringLiteral("激光前检查");
  case ProcessStepType::LaserExecute:   return QStringLiteral("激光执行");
  case ProcessStepType::PostLaserVerify: return QStringLiteral("激光后验证");
  case ProcessStepType::OutputResult:   return QStringLiteral("输出结果");
  }
  return QStringLiteral("未知步骤");
}

} // namespace

void MainWindow::startWorkflowRun() {
  if (workflowRunning_) {
    return;
  }

  if (!virtualTransportController_.isBoardReady()) {
    if (runModeWidget_ != nullptr) {
      runModeWidget_->appendProductionLog(QStringLiteral("板尚未到位，无法启动流程。请先执行进板。"));
    }
    appendLog(QStringLiteral("板尚未到位，无法启动流程。"));
    return;
  }

  if (workflowContext_.program == programManager_.mutableProgram() && workflowContext_.totalBoards > 0 &&
      workflowContext_.boardIndex < workflowContext_.totalBoards &&
      (workflowContext_.nextStepIndex > 0 || workflowContext_.boardIndex > 0 ||
       workflowContext_.okCount > 0 || workflowContext_.ngCount > 0)) {
    workflowRunning_ = true;
    workflowTimer_->start();
    if (runModeWidget_ != nullptr) {
      runModeWidget_->appendProductionLog(QStringLiteral("工作流已恢复。"));
    }
    appendLog(QStringLiteral("工作流已恢复。"));
    return;
  }

  workflowContext_ = WorkflowContext {};
  workflowContext_.program = programManager_.mutableProgram();
  workflowContext_.motionController = &virtualMotionController_;
  workflowContext_.laserController = &virtualLaserController_;
  workflowContext_.transportController = &virtualTransportController_;
  workflowContext_.databaseManager = &databaseManager_;
  workflowContext_.reuseInjectedInputs = false;
  workflowContext_.boardReady = virtualTransportController_.isBoardReady();
  workflowContext_.totalBoards = 10;
  workflowContext_.boardIndex = 0;
  workflowContext_.okCount = 0;
  workflowContext_.ngCount = 0;
  workflowContext_.currentMachinePose = currentMechanicalPose();
  workflowStepSummary_ = QStringLiteral("等待启动");

  // Wire frame capture callback.
  workflowContext_.captureFrame = [this]() -> std::string {
    if (lastCameraFrameImage_.isNull()) {
      return {};
    }

    const QString tempDir = appSettings_.templateFolderPath.empty()
                                ? projectFilePath(QStringLiteral("data/template_cache"))
                                : QString::fromStdString(appSettings_.templateFolderPath);
    QDir().mkpath(tempDir);
    const QString path =
        QDir(tempDir).filePath(QStringLiteral("capture_%1.png").arg(QDateTime::currentMSecsSinceEpoch()));
    if (lastCameraFrameImage_.save(path)) {
      return path.toStdString();
    }

    return {};
  };
  workflowContext_.captureWholeBoardScan = [this]() -> BoardScanCaptureWorkflowResult {
    return executeWholeBoardScan(false);
  };
  workflowContext_.moveCameraPose = [this](const MechanicalPose &pose, const std::string &) {
    return virtualMotionSystem_.moveCameraPose(pose);
  };

  // Wire step progress callback for UI updates.
  workflowContext_.onStepProgress = [this](const int stepIdx, const int totalSteps,
                                           const std::string &stepName, const StepExecutionStatus status) {
    workflowStepIndex_ = stepIdx;
    workflowTotalSteps_ = totalSteps;
    if (runModeWidget_ != nullptr) {
      QString statusText;
      switch (status) {
      case StepExecutionStatus::Running:
        statusText = QStringLiteral("执行中");
        break;
      case StepExecutionStatus::Succeeded:
        statusText = QStringLiteral("完成");
        break;
      case StepExecutionStatus::Failed:
        statusText = QStringLiteral("失败");
        break;
      case StepExecutionStatus::Skipped:
        statusText = QStringLiteral("跳过");
        break;
      default:
        statusText = QStringLiteral("等待");
        break;
      }

      workflowStepSummary_ = QStringLiteral("步骤 %1/%2: %3 [%4]")
                                 .arg(stepIdx)
                                 .arg(totalSteps)
                                 .arg(QString::fromStdString(stepName), statusText);
      runModeWidget_->updateStepProgress(workflowStepSummary_, stepIdx, totalSteps);
    }
  };

  // Wire log callback.
  workflowContext_.onLog = [this](const std::string &message) {
    const QString msg = QString::fromStdString(message);
    if (runModeWidget_ != nullptr) {
      runModeWidget_->appendProductionLog(msg);
    }

    appendLog(msg);
  };

  // Wire board result callback.
  workflowContext_.onBoardResult = [this](const int, const bool ok, const std::string &) {
    if (runModeWidget_ != nullptr) {
      runModeWidget_->recordBoardResult(ok);
    }
  };

  workflowStepIndex_ = 0;
  workflowTotalSteps_ = static_cast<int>(processEngine_.stepCount());
  workflowStepSummary_ = QStringLiteral("准备启动流程");
  workflowRunning_ = true;
  workflowTimer_->start();

  if (runModeWidget_ != nullptr) {
    runModeWidget_->appendProductionLog(
        QStringLiteral("工作流已启动，共 %1 块板待处理，%2 个步骤/板。")
            .arg(workflowContext_.totalBoards)
            .arg(workflowTotalSteps_));
    runModeWidget_->appendProductionLog(
        QStringLiteral("─────────────────────────────────\n开始处理板 #1 / %1")
            .arg(workflowContext_.totalBoards));
  }

  appendLog(QStringLiteral("工作流已启动。"));
}

void MainWindow::stopWorkflowRun() {
  workflowRunning_ = false;
  workflowTimer_->stop();
  processEngine_.requestCancel();
  workflowStepSummary_ = QStringLiteral("工作流已停止");

  if (runModeWidget_ != nullptr) {
    runModeWidget_->appendProductionLog(QStringLiteral("工作流已停止。"));
  }

  appendLog(QStringLiteral("工作流已停止。"));
}

void MainWindow::pauseWorkflowRun() {
  workflowRunning_ = false;
  workflowTimer_->stop();
  workflowStepSummary_ = QStringLiteral("工作流已暂停");

  if (runModeWidget_ != nullptr) {
    runModeWidget_->appendProductionLog(QStringLiteral("工作流已暂停。"));
  }

  appendLog(QStringLiteral("工作流已暂停。"));
}

void MainWindow::runSingleWorkflowStep() {
  advanceWorkflowStepInternal(true);
}

void MainWindow::advanceWorkflowStep() {
  advanceWorkflowStepInternal(false);
}

void MainWindow::advanceWorkflowStepInternal(const bool allowWhenPaused) {
  if (!allowWhenPaused && !workflowRunning_) {
    return;
  }

  const WorkflowStepRunResult stepResult = processEngine_.runNextStep(workflowContext_);
  if (!stepResult.advanced) {
    appendLog(QStringLiteral("工作流无法继续推进。"));
    return;
  }

  if (runModeWidget_ != nullptr) {
    const auto &record = stepResult.record;
    const QString statusIcon = record.status == StepExecutionStatus::Succeeded   ? QStringLiteral("OK")
                             : record.status == StepExecutionStatus::Failed     ? QStringLiteral("FAIL")
                             : record.status == StepExecutionStatus::Skipped   ? QStringLiteral("SKIP")
                                                                               : QStringLiteral("...");
    runModeWidget_->appendProductionLog(
        QStringLiteral("  %1 [%2] %3")
            .arg(stepTypeDisplayName(record.stepType))
            .arg(statusIcon)
            .arg(QString::fromStdString(record.message)));
  }

  if (!stepResult.boardCompleted) {
    return;
  }

  const int totalSteps = workflowTotalSteps_ > 0 ? workflowTotalSteps_
                                                  : static_cast<int>(processEngine_.stepCount());
  if (runModeWidget_ != nullptr) {
    workflowStepSummary_ = QStringLiteral("板 #%1 完成").arg(workflowContext_.boardIndex + 1);
    runModeWidget_->updateStepProgress(
        workflowStepSummary_,
        totalSteps, totalSteps);
  }

  // Submit laser-point results to SPC for the completed board.
  if (spcBridge_ != nullptr) {
    spcBridge_->submitWorkflowResult(workflowContext_);
  }

  ++workflowContext_.boardIndex;

  if (workflowContext_.boardIndex >= workflowContext_.totalBoards) {
    workflowTimer_->stop();
    workflowRunning_ = false;

    if (runModeWidget_ != nullptr) {
      workflowStepSummary_ = QStringLiteral("全部完成");
      runModeWidget_->appendProductionLog(
          QString::fromUtf8("═══════════════════════════════════\n"
                            "全部 %1 块板处理完成。OK=%2, NG=%3\n"
                            "═══════════════════════════════════")
              .arg(workflowContext_.totalBoards)
              .arg(workflowContext_.okCount)
              .arg(workflowContext_.ngCount));
      runModeWidget_->updateStepProgress(workflowStepSummary_, totalSteps, totalSteps);
    }

    appendLog(QString::fromUtf8("工作流全部完成。"));
    return;
  }

  if (runModeWidget_ != nullptr) {
    runModeWidget_->appendProductionLog(
        QString::fromUtf8("─────────────────────────────────\n开始处理板 #%1 / %2")
            .arg(workflowContext_.boardIndex + 1)
            .arg(workflowContext_.totalBoards));
  }
}

#endif

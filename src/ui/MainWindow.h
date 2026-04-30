#pragma once

#include "camera/UsbCamera.h"
#include "motion/VirtualMotionController.h"
#include "program/ProgramManager.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QMainWindow>
#include <QPointer>
#include <QString>
#include <QSize>
#include <functional>

class CadGraphicsView;
class CadRulerWidget;
class MotionControlDialog;
class QComboBox;
class QDoubleSpinBox;
class QGraphicsPixmapItem;
class QGraphicsRectItem;
class QGraphicsScene;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTextEdit;
class QTimer;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

private:
  enum class CanvasMode {
    Select,
    DrawRoi,
    DrawMark,
  };

  void buildMenus();
  void buildCentralUi();
  void buildLeftWorkbench(class QBoxLayout *parentLayout);
  void buildRightPanel(class QBoxLayout *parentLayout);
  void buildOverviewPage();
  void buildRoiPage();
  void buildMarkPage();
  void buildTemplatePage();
  void appendLog(const QString &message);
  void refreshStatusSummary();
  void refreshProgramWidgets();
  void refreshWorkbenchScene(bool keepView = true);
  void refreshCameraState();
  void refreshTableSelections();
  void syncProgramFromEditors();
  void updateProgram(const std::function<void(ProgramModel &)> &updater);
  void createDefaultProgram();
  void openProgram();
  void saveCurrentProgram();
  void openCameraConfig();
  void openMotionPanel();
  void startCameraPreview();
  void stopCameraPreview();
  void updateCameraFrame();
  void toggleFovOverlay();
  void resetWorkbenchView();
  void setCanvasMode(CanvasMode mode);
  void setCurrentPage(int index);
  void handleDrawnRegion(const QRectF &sceneRect);
  void addMarkFromSceneRect(const QRectF &sceneRect);
  void addRoiFromSceneRect(const QRectF &sceneRect);
  void selectMarkIndex(int index);
  void selectRoiIndex(int index);
  void applyMarkEditorToSelection();
  void applyRoiEditorToSelection();
  void removeSelectedMark();
  void removeSelectedRoi();
  void previewMarkMatching();
  void testCodeReading();
  void centerOnCurrentSelection();
  void updateCursorCoordinate(const QPointF &scenePos);
  void updateMarkEditorFromSelection();
  void updateRoiEditorFromSelection();
  void updateSelectedMarkFromScene(const QPointF &centerScenePos);
  void updateSelectedRoiFromScene(const QPointF &topLeftScenePos);

  [[nodiscard]] QString projectRootPath() const;
  [[nodiscard]] QString projectFilePath(const QString &relativePath) const;
  [[nodiscard]] QString cameraModeText() const;
  [[nodiscard]] QString motionStateText() const;
  [[nodiscard]] QString currentMarkShapeText() const;
  [[nodiscard]] QString currentMarkAlgorithmText() const;
  [[nodiscard]] QString currentRoiShapeText() const;

  Ui::MainWindow *ui_ {nullptr};
  UsbCamera usbCamera_;
  VirtualMotionController virtualMotionController_;
  ProgramManager programManager_;
  MotionControlDialog *motionControlDialog_ {nullptr};
  QTimer *cameraTimer_ {nullptr};
  int cameraDeviceIndex_ {0};
  double cameraExposureMs_ {12.0};
  double cameraGain_ {0.0};
  QString cameraResolutionPreset_ {QStringLiteral("1280 x 720")};
  QSize lastFrameSize_ {640, 360};
  bool showFovOverlay_ {true};
  bool workbenchViewInitialized_ {false};
  CanvasMode canvasMode_ {CanvasMode::Select};
  int selectedMarkIndex_ {-1};
  int selectedRoiIndex_ {-1};

  CadGraphicsView *workbenchGraphicsView_ {nullptr};
  CadRulerWidget *topRulerWidget_ {nullptr};
  CadRulerWidget *leftRulerWidget_ {nullptr};
  QGraphicsScene *workbenchScene_ {nullptr};
  QGraphicsPixmapItem *workbenchPixmapItem_ {nullptr};
  QGraphicsRectItem *fovRectItem_ {nullptr};
  QPushButton *toggleFovButton_ {nullptr};
  QLabel *statusSummaryValueLabel_ {nullptr};
  QLabel *cameraModeValueLabel_ {nullptr};
  QLabel *cameraStatusToolbarValueLabel_ {nullptr};
  QLabel *cameraStatusDetailValueLabel_ {nullptr};
  QLabel *cameraDeviceValueLabel_ {nullptr};
  QLabel *fovInfoValueLabel_ {nullptr};
  QLabel *cursorPositionValueLabel_ {nullptr};
  QLabel *zoomValueLabel_ {nullptr};
  QLabel *programNameValueLabel_ {nullptr};
  QLabel *programPathValueLabel_ {nullptr};
  QLabel *programAiModelValueLabel_ {nullptr};
  QLabel *markCountValueLabel_ {nullptr};
  QLabel *roiCountValueLabel_ {nullptr};
  QLabel *motionStatusValueLabel_ {nullptr};
  QLabel *templatePreviewValueLabel_ {nullptr};
  QLabel *codeResultValueLabel_ {nullptr};
  QLabel *markPreviewScoreValueLabel_ {nullptr};
  QLabel *markLiveScoreValueLabel_ {nullptr};
  QListWidget *toolSelectorListWidget_ {nullptr};
  QStackedWidget *toolStackedWidget_ {nullptr};
  QTableWidget *markTableWidget_ {nullptr};
  QTableWidget *roiTableWidget_ {nullptr};
  QLineEdit *markNameLineEdit_ {nullptr};
  QLineEdit *markColorLineEdit_ {nullptr};
  QComboBox *markShapeComboBox_ {nullptr};
  QComboBox *markAlgorithmComboBox_ {nullptr};
  QDoubleSpinBox *markMinScoreSpinBox_ {nullptr};
  QDoubleSpinBox *markWidthSpinBox_ {nullptr};
  QDoubleSpinBox *markHeightSpinBox_ {nullptr};
  QDoubleSpinBox *markRotationSpinBox_ {nullptr};
  QDoubleSpinBox *markPreviewScoreSpinBox_ {nullptr};
  QDoubleSpinBox *markLiveScoreSpinBox_ {nullptr};
  QDoubleSpinBox *roiThresholdSpinBox_ {nullptr};
  QDoubleSpinBox *roiWidthSpinBox_ {nullptr};
  QDoubleSpinBox *roiHeightSpinBox_ {nullptr};
  QDoubleSpinBox *roiRotationSpinBox_ {nullptr};
  QLineEdit *roiNameLineEdit_ {nullptr};
  QComboBox *roiShapeComboBox_ {nullptr};
  QLineEdit *codeRegionLineEdit_ {nullptr};
  QProgressBar *markPreviewProgressBar_ {nullptr};
  QTextEdit *operationLogTextEdit_ {nullptr};
};

#endif

#pragma once

#include "boardscan/BoardScanTypes.h"
#include "camera/VirtualCameraDevice.h"
#include "config/AppSettings.h"
#include "database/DatabaseManager.h"
#include "laser/VirtualLaserController.h"
#include "motion/VirtualMotionController.h"
#include "motion/VirtualMotionSystem.h"
#include "process/ProcessEngine.h"
#include "program/ProgramManager.h"
#include "transport/VirtualTransportController.h"
#include "vision/CoordinateTransformer.h"

#ifdef AOI_HAS_QT_WIDGETS

#include <QImage>
#include <QMainWindow>
#include <QPointer>
#include <QString>
#include <QSize>
#include <functional>

class CadGraphicsView;
class CadRulerWidget;
class DataCollectDialog;
class LogWindow;
class MarkEditDialog;
class MotionControlDialog;
class NewProgramDialog;
class RunModeWidget;
class SettingsDialog;
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
class QTimer;

class LaserOffsetCalibDialog;
class LaserSpcBridge;
class LaserSpcWindow;
class ProductionHistoryDialog;

namespace HostSpc {
class SpcWriteManager;
}

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

  enum class AppMode {
    Editor,
    Run,
  };

  void buildMenus();
  void buildCentralUi();
  void buildLeftWorkbench(class QBoxLayout *parentLayout);
  void buildRightPanel(class QBoxLayout *parentLayout);
  void buildRoiPage();
  void buildMarkPage();
  void buildTemplatePage();
  void buildRunInterface();
  void appendLog(const QString &message);
  void refreshStatusSummary();
  void refreshTemplatePreviewSummary();
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
  void openLogWindow();
  void openSettings();
  void startCameraPreview();
  void stopCameraPreview();
  void updateCameraFrame();
  void openDataCollect();
  void openMarkEditDialog();
  void openOriginCalibration();
  void openLaserOffsetCalibration();
  void openSpcDashboard();
  void openProductionHistory();
  void loadBoardToTrack();
  void unloadBoardFromTrack();
  void runWholeBoardScan();
  [[nodiscard]] BoardScanCaptureWorkflowResult executeWholeBoardScan(bool refreshWorkbenchAfterScan);
  void toggleCodeCameraView();
  void toggleFovOverlay();
  void resetWorkbenchView();
  void applyMechanicalPose(const MechanicalPose &pose, const QString &reason);
  void captureCurrentTemplateImage();
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

  // Mode switching
  void switchToEditorMode();
  void switchToRunMode();

  // Workflow runtime
  void startWorkflowRun();
  void stopWorkflowRun();
  void pauseWorkflowRun();
  void runSingleWorkflowStep();
  void advanceWorkflowStep();
  void advanceWorkflowStepInternal(bool allowWhenPaused);
  void tickVirtualDevices();
  void logCalibrationRecord(const CalibrationRecord &record);

  [[nodiscard]] QString projectRootPath() const;
  [[nodiscard]] QString projectFilePath(const QString &relativePath) const;
  [[nodiscard]] QImage currentCalibrationFrame() const;
  [[nodiscard]] MechanicalPose currentMechanicalPose() const;
  [[nodiscard]] QString cameraModeText() const;
  [[nodiscard]] QString boardTransportStateText() const;
  [[nodiscard]] QString motionStateText() const;
  [[nodiscard]] QString runModeOverlayText() const;
  [[nodiscard]] QString currentMarkShapeText() const;
  [[nodiscard]] QString currentMarkAlgorithmText() const;
  [[nodiscard]] QString currentRoiShapeText() const;

  // Shared dialog building blocks (reduce glue code)
  void ensureCameraPreview();
  [[nodiscard]] std::function<void(double dx, double dy)> makeJogProvider();

  Ui::MainWindow *ui_ {nullptr};
  VirtualCameraDevice virtualCamera_;
  VirtualMotionController virtualMotionController_;
  VirtualLaserController virtualLaserController_;
  VirtualTransportController virtualTransportController_;
  VirtualMotionSystem virtualMotionSystem_;
  ProgramManager programManager_;
  ProcessEngine processEngine_;
  DatabaseManager databaseManager_;
  AppSettings appSettings_;
  DataCollectDialog *dataCollectDialog_ {nullptr};
  MarkEditDialog *markEditDialog_ {nullptr};
  MotionControlDialog *motionControlDialog_ {nullptr};
  NewProgramDialog *newProgramDialog_ {nullptr};
  LaserSpcWindow *spcWindow_ {nullptr};
  LaserSpcBridge *spcBridge_ {nullptr};
  HostSpc::SpcWriteManager *spcWriteManager_ {nullptr};
  LogWindow *logWindow_ {nullptr};
  SettingsDialog *settingsDialog_ {nullptr};
  QTimer *cameraTimer_ {nullptr};
  QTimer *simulationTimer_ {nullptr};
  QTimer *workflowTimer_ {nullptr};
  int cameraDeviceIndex_ {0};
  double cameraExposureMs_ {12.0};
  double cameraGain_ {0.0};
  QString cameraResolutionPreset_ {QStringLiteral("1280 x 720")};
  QImage lastCameraFrameImage_;
  QSize lastFrameSize_ {640, 360};
  bool showFovOverlay_ {true};
  bool showCodeCameraView_ {false};
  bool workbenchViewInitialized_ {false};
  CanvasMode canvasMode_ {CanvasMode::Select};
  AppMode appMode_ {AppMode::Editor};
  int selectedMarkIndex_ {-1};
  int selectedRoiIndex_ {-1};

  // Workflow state
  WorkflowContext workflowContext_;
  int workflowStepIndex_ {0};
  int workflowTotalSteps_ {0};
  QString workflowStepSummary_ {QStringLiteral("等待启动")};
  bool workflowRunning_ {false};

  // Editor widgets
  CadGraphicsView *workbenchGraphicsView_ {nullptr};
  CadRulerWidget *topRulerWidget_ {nullptr};
  CadRulerWidget *leftRulerWidget_ {nullptr};
  QGraphicsScene *workbenchScene_ {nullptr};
  QGraphicsPixmapItem *workbenchPixmapItem_ {nullptr};
  QPushButton *toggleCodeCameraViewButton_ {nullptr};
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
  QLabel *templateCachePreviewLabel_ {nullptr};
  QLabel *calibrationSummaryValueLabel_ {nullptr};
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
  QString lastTemplateCapturePath_;

  // Mode switching
  QPushButton *switchToRunButton_ {nullptr};
  QPushButton *switchToEditorButton_ {nullptr};
  QStackedWidget *mainStackedWidget_ {nullptr};
  QWidget *editorPage_ {nullptr};
  RunModeWidget *runModeWidget_ {nullptr};
};

#endif

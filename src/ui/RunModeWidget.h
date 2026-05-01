#pragma once

#ifdef AOI_HAS_QT_WIDGETS

#include "process/ProcessTypes.h"

#include <QFrame>
#include <QImage>
#include <QString>
#include <functional>

#include <QChart>
#include <QChartView>
#include <QLineSeries>

class QBoxLayout;
class QGraphicsScene;
class QGraphicsView;
class QGraphicsPixmapItem;
class QLabel;
class QProgressBar;
class QPushButton;
class QTextEdit;
class QTimer;

class RunModeWidget final : public QFrame {
  Q_OBJECT

public:
  using FrameProvider = std::function<QImage()>;
  using BoardCountProvider = std::function<int()>;

  explicit RunModeWidget(QWidget *parent = nullptr);
  ~RunModeWidget() override;

  void setFrameProvider(FrameProvider provider);
  void setBoardCountProvider(BoardCountProvider provider);
  void setTotalBoards(int count);
  void fitPreviewContent();

  void appendProductionLog(const QString &message);
  void updateStepProgress(const QString &stepName, int stepIndex, int totalSteps);
  void recordBoardResult(bool ok);

  int okCount() const;
  int ngCount() const;
  int currentBoardIndex() const;

signals:
  void startRequested();
  void stopRequested();
  void pauseRequested();
  void singleStepRequested();

private:
  void buildUi();
  void buildLockedPreview(QBoxLayout *parentLayout);
  void buildDashboard(QBoxLayout *parentLayout);
  void refreshPreview();
  void refreshDashboard();

  // Left: locked preview
  QGraphicsView *previewView_ {nullptr};
  QGraphicsScene *previewScene_ {nullptr};
  QGraphicsPixmapItem *previewPixmapItem_ {nullptr};
  QTimer *previewTimer_ {nullptr};

  // Right: stats dashboard
  QLabel *okCountLabel_ {nullptr};
  QLabel *ngCountLabel_ {nullptr};
  QLabel *currentBoardLabel_ {nullptr};
  QLabel *totalBoardsLabel_ {nullptr};
  QLabel *stepProgressLabel_ {nullptr};
  QLabel *workflowStateLabel_ {nullptr};
  QProgressBar *stepProgressBar_ {nullptr};
  QTextEdit *productionLogEdit_ {nullptr};

  // Controls
  QPushButton *startButton_ {nullptr};
  QPushButton *stopButton_ {nullptr};
  QPushButton *pauseButton_ {nullptr};
  QPushButton *singleStepButton_ {nullptr};

  // Yield trend chart
  QChartView *yieldChartView_ {nullptr};
  QChart *yieldChart_ {nullptr};
  QLineSeries *yieldSeries_ {nullptr};
  int yieldDataPointCount_ {0};

  FrameProvider frameProvider_;
  BoardCountProvider boardCountProvider_;

  int okCount_ {0};
  int ngCount_ {0};
  int currentBoardIndex_ {0};
  int totalBoards_ {0};
  int currentStepIndex_ {0};
  int totalSteps_ {0};

  QImage lastPreviewFrame_;
  bool running_ {false};
};

#endif

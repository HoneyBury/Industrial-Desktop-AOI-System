#pragma once

#include <QMainWindow>
#include <QSet>

#include <memory>

#include "SpcWriteManager.h"

namespace LaserSpc::App {
class AppServiceFacade;
}

namespace LaserSpc::Ui {
class DashboardWidget;
}

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;

namespace HostSpc {

class BoardBatchAggregator;
class MesDebugWindow;

class HostSpcExampleWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit HostSpcExampleWindow(QWidget* parent = nullptr);
    ~HostSpcExampleWindow() override;

    static bool ensurePointDetailFile(LaserSpc::Domain::PointRecordRow* row, QString* errorMessage = nullptr);

private slots:
    void onFlushModeChanged();
    void writeSampleBatch();
    void startBufferedBoard();
    void appendBufferedPoint();
    void completeBufferedBoard();
    void flushAllPending();
    void refreshDashboard();
    void writeAlgorithmPlanDebugBatch();
    void openMesDebugWindow();
    void toggleSystemSettingsAvailability();
    void toggleExportReportAvailability();
    void checkLaserContentDuplicate();
    void handleDirectBatchStored(QString requestId, LaserSpc::Domain::IngestResult result);
    void handleBatchPrepared(QString boardCode, QString requestId, int pointCount);
    void handleBatchFlushed(QString boardCode, QString requestId, LaserSpc::Domain::IngestResult result);
    void handleBatchDeferred(QString boardCode, QString reason);

private:
    void buildUi(const QString& queryMode, const QString& startupWarning, const QString& configPath);
    void logMessage(const QString& message);
    void applySettingsToFacade(const LaserSpc::Infrastructure::AppSettings& settings);
    void scheduleDashboardRefresh();
    void updateBufferedBoardStatus();
    bool useAsyncMode() const;
    QString nextBoardCode();
    QString nextRequestId(const QString& prefix);
    LaserSpc::Domain::BoardRecordRow makeBoardRow(const QString& boardCode, bool hasNgPoint) const;
    LaserSpc::Domain::PointRecordRow makePointRow(const QString& boardCode,
                                                  int pointIndex,
                                                  bool isNg,
                                                  bool includeExtendedDetail = false,
                                                  QString* detailError = nullptr) const;
    LaserSpc::Domain::InspectionBatch makeSampleBatch(const QString& boardCode,
                                                      const QString& requestId,
                                                      bool includeExtendedDetail = false);

    std::unique_ptr<LaserSpc::App::AppServiceFacade> m_facade;
    SpcWriteManager* m_writer = nullptr;
    BoardBatchAggregator* m_aggregator = nullptr;
    MesDebugWindow* m_mesDebugWindow = nullptr;
    LaserSpc::Ui::DashboardWidget* m_dashboard = nullptr;

    QLineEdit* m_lineNameEdit = nullptr;
    QLineEdit* m_programNameEdit = nullptr;
    QLineEdit* m_deviceNameEdit = nullptr;
    QLineEdit* m_operatorNameEdit = nullptr;
    QComboBox* m_boardResultCombo = nullptr;
    QSpinBox* m_pointCountSpin = nullptr;
    QComboBox* m_flushModeCombo = nullptr;
    QCheckBox* m_injectNgCheck = nullptr;
    QLabel* m_queryModeValue = nullptr;
    QLabel* m_configPathValue = nullptr;
    QLabel* m_bufferedBoardValue = nullptr;
    QLabel* m_bufferedPointValue = nullptr;
    QPlainTextEdit* m_logEdit = nullptr;
    QLineEdit* m_laserContentCheckEdit = nullptr;

    QSet<QString> m_directAsyncRequestIds;
    QString m_bufferedBoardCode;
    QString m_bufferedRequestId;
    int m_bufferedExpectedPoints = 0;
    bool m_bufferedInjectNgPoint = false;
    int m_nextBufferedPointIndex = 1;
    quint64 m_sequence = 1;
};

}  // namespace HostSpc

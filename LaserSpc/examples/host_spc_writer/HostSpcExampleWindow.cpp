#include "HostSpcExampleWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QStringList>

#include "BoardBatchAggregator.h"
#include "MesDebugWindow.h"
#include "app/AppServiceFacade.h"
#include "infrastructure/AppConfigService.h"
#include "infrastructure/PointDetailJsonService.h"
#include "infrastructure/RepositoryFactory.h"
#include "ui/shell/DashboardWidget.h"

namespace HostSpc {

namespace {

QString pickBoardResultLabel(bool hasNgPoint, const QString& selectedMode) {
    if (selectedMode == QObject::tr("OK")) {
        return QObject::tr("OK");
    }
    if (selectedMode == QObject::tr("NG")) {
        return QObject::tr("NG");
    }
    return hasNgPoint ? QObject::tr("NG") : QObject::tr("OK");
}

QString resultSummary(const LaserSpc::Domain::IngestResult& result) {
    QStringList parts;
    parts << QObject::tr("success=%1").arg(result.success ? QObject::tr("true") : QObject::tr("false"));
    parts << QObject::tr("boards=%1").arg(result.insertedBoards);
    parts << QObject::tr("points=%1").arg(result.insertedPoints);
    if (!result.forwardMessage.trimmed().isEmpty()) {
        parts << QObject::tr("forward=%1").arg(result.forwardMessage);
    }
    if (!result.errorMessage.trimmed().isEmpty()) {
        parts << QObject::tr("error=%1").arg(result.errorMessage);
    }
    return parts.join(QObject::tr(" | "));
}

bool shouldMarkNgPoint(bool injectNg, int pointIndex, int totalPoints) {
    if (!injectNg || totalPoints <= 0) {
        return false;
    }
    if (totalPoints == 1) {
        return true;
    }
    if (totalPoints >= 4 && pointIndex == (totalPoints / 2)) {
        return true;
    }
    return pointIndex == totalPoints;
}

QString ngPointName(const QString& boardCode, int pointIndex) {
    static const QStringList defectNames{
        QObject::tr("MarkOffset"),
        QObject::tr("CodeBlur"),
        QObject::tr("ContrastLow"),
        QObject::tr("PrintShift"),
        QObject::tr("ReflectNoise"),
        QObject::tr("FocusDrift")
    };
    const uint base = qHash(boardCode);
    const uint index = (base + static_cast<uint>(qMax(0, pointIndex - 1))) % static_cast<uint>(defectNames.size());
    return defectNames.at(static_cast<int>(index));
}

QString okPointName(int pointIndex) {
    return QObject::tr("Code-%1").arg(pointIndex, 2, 10, QChar('0'));
}

QJsonArray buildAlgorithmPlan(int pointIndex, bool isNg) {
    QJsonArray plan;

    QJsonObject locate;
    locate.insert(QStringLiteral("name"), QObject::tr("定位"));
    locate.insert(QStringLiteral("ok"), QObject::tr("进入码区提取"));
    locate.insert(QStringLiteral("ng"), QObject::tr("输出定位失败"));
    locate.insert(QStringLiteral("sequence"), 1);
    plan.append(locate);

    QJsonObject decode;
    decode.insert(QStringLiteral("name"), QObject::tr("解码"));
    decode.insert(QStringLiteral("ok"), QObject::tr("进入质量判定"));
    decode.insert(QStringLiteral("ng"), QObject::tr("输出解码失败"));
    decode.insert(QStringLiteral("sequence"), 2);
    plan.append(decode);

    QJsonObject verify;
    verify.insert(QStringLiteral("name"), QObject::tr("质量判定"));
    verify.insert(QStringLiteral("ok"), isNg ? QObject::tr("命中阈值但结果转人工复核") : QObject::tr("直接判定 OK"));
    verify.insert(QStringLiteral("ng"), isNg ? QObject::tr("直接判定 NG") : QObject::tr("转入异常复判"));
    verify.insert(QStringLiteral("sequence"), 3);
    verify.insert(QStringLiteral("pointIndex"), pointIndex);
    plan.append(verify);

    if (isNg) {
        QJsonObject review;
        review.insert(QStringLiteral("name"), QObject::tr("异常复判"));
        review.insert(QStringLiteral("ok"), QObject::tr("人工确认误报后回写 OK"));
        review.insert(QStringLiteral("ng"), QObject::tr("确认不良并结束"));
        review.insert(QStringLiteral("sequence"), 4);
        plan.append(review);
    }

    return plan;
}

LaserSpc::Domain::PointDetailInfo buildDetailInfoFromRow(const LaserSpc::Domain::PointRecordRow& row) {
    LaserSpc::Domain::PointDetailInfo detail = row.detail;
    if (detail.laserTemplatePath.trimmed().isEmpty()) {
        const QString configuredPath = detail.extraFields.value(QStringLiteral("templateFilePath")).toString().trimmed();
        detail.laserTemplatePath = configuredPath.isEmpty() ? QObject::tr("templates/%1.tpl").arg(row.programName)
                                                            : configuredPath;
    }
    if (detail.laserContent.trimmed().isEmpty()) {
        detail.laserContent = row.laserContent;
    }
    if (detail.readCodeContent.trimmed().isEmpty()) {
        detail.readCodeContent = row.readCodeContent;
    }
    if (detail.programName.trimmed().isEmpty()) {
        detail.programName = row.programName;
    }
    if (!detail.startTime.isValid()) {
        detail.startTime = row.startTime;
    }
    if (!detail.endTime.isValid()) {
        detail.endTime = row.endTime;
    }
    detail.success = (row.result == QObject::tr("OK"));
    return detail;
}

}  // namespace

HostSpcExampleWindow::HostSpcExampleWindow(QWidget* parent) : QMainWindow(parent) {
    LaserSpc::Infrastructure::AppConfigService configService;
    const auto settings = configService.settings();
    auto buildResult = LaserSpc::Infrastructure::RepositoryFactory::build(settings);

    m_facade = std::make_unique<LaserSpc::App::AppServiceFacade>(
        std::move(buildResult.repository),
        buildResult.dataSourceMode);
    m_writer = new SpcWriteManager(settings, this);
    m_aggregator = new BoardBatchAggregator(m_writer, this);

    buildUi(buildResult.dataSourceMode, buildResult.warningMessage, configService.configFilePath());

    connect(m_flushModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HostSpcExampleWindow::onFlushModeChanged);
    connect(m_writer, &SpcWriteManager::batchStored,
            this, &HostSpcExampleWindow::handleDirectBatchStored);
    connect(m_aggregator, &BoardBatchAggregator::batchPrepared,
            this, &HostSpcExampleWindow::handleBatchPrepared);
    connect(m_aggregator, &BoardBatchAggregator::batchFlushed,
            this, &HostSpcExampleWindow::handleBatchFlushed);
    connect(m_aggregator, &BoardBatchAggregator::batchDeferred,
            this, &HostSpcExampleWindow::handleBatchDeferred);

    onFlushModeChanged();
    updateBufferedBoardStatus();

    logMessage(QObject::tr("Host writer example started."));
    if (!buildResult.warningMessage.trimmed().isEmpty()) {
        logMessage(QObject::tr("Query repository warning: %1").arg(buildResult.warningMessage));
    }
    if (buildResult.dataSourceMode.contains(QObject::tr("Mock"))) {
        logMessage(QObject::tr("Dashboard is currently using the mock query repository. Writes may not appear in the dashboard until MySQL query access is available."));
    }
}

HostSpcExampleWindow::~HostSpcExampleWindow() = default;

bool HostSpcExampleWindow::ensurePointDetailFile(LaserSpc::Domain::PointRecordRow* row, QString* errorMessage) {
    if (row == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Point record must not be null.");
        }
        return false;
    }

    row->detailJsonPath = LaserSpc::Infrastructure::PointDetailJsonService::buildDefaultFilePath(*row);

    const LaserSpc::Domain::PointDetailInfo detail = buildDetailInfoFromRow(*row);

    QString saveError;
    if (!LaserSpc::Infrastructure::PointDetailJsonService::saveDetail(detail, row->detailJsonPath, &saveError)) {
        row->detailJsonPath.clear();
        if (errorMessage != nullptr) {
            *errorMessage = saveError;
        }
        return false;
    }

    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}

void HostSpcExampleWindow::buildUi(const QString& queryMode,
                                   const QString& startupWarning,
                                   const QString& configPath) {
    setWindowTitle(QObject::tr("LaserSpc Host Writer Example"));
    resize(1680, 980);

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(10);

    auto* splitter = new QSplitter(Qt::Horizontal, central);
    splitter->setChildrenCollapsible(false);

    m_dashboard = new LaserSpc::Ui::DashboardWidget(m_facade.get(), splitter);

    auto* controlPanel = new QWidget(splitter);
    controlPanel->setMinimumWidth(360);
    auto* panelLayout = new QVBoxLayout(controlPanel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(10);

    auto* configGroup = new QGroupBox(QObject::tr("Host Setup"), controlPanel);
    auto* configLayout = new QFormLayout(configGroup);
    m_queryModeValue = new QLabel(queryMode, configGroup);
    m_queryModeValue->setWordWrap(true);
    m_configPathValue = new QLabel(configPath, configGroup);
    m_configPathValue->setWordWrap(true);
    configLayout->addRow(QObject::tr("Query Mode"), m_queryModeValue);
    configLayout->addRow(QObject::tr("Config File"), m_configPathValue);
    if (!startupWarning.trimmed().isEmpty()) {
        auto* warningLabel = new QLabel(startupWarning, configGroup);
        warningLabel->setWordWrap(true);
        warningLabel->setStyleSheet(QObject::tr("color:#a63b00;"));
        configLayout->addRow(QObject::tr("Warning"), warningLabel);
    }

    auto* sampleGroup = new QGroupBox(QObject::tr("Sample Payload"), controlPanel);
    auto* sampleLayout = new QFormLayout(sampleGroup);
    m_lineNameEdit = new QLineEdit(QObject::tr("L1"), sampleGroup);
    m_programNameEdit = new QLineEdit(QObject::tr("Program-A"), sampleGroup);
    m_deviceNameEdit = new QLineEdit(QObject::tr("Laser-01"), sampleGroup);
    m_operatorNameEdit = new QLineEdit(QObject::tr("HostExample"), sampleGroup);
    m_boardResultCombo = new QComboBox(sampleGroup);
    m_boardResultCombo->addItem(QObject::tr("Auto"));
    m_boardResultCombo->addItem(QObject::tr("OK"));
    m_boardResultCombo->addItem(QObject::tr("NG"));
    m_pointCountSpin = new QSpinBox(sampleGroup);
    m_pointCountSpin->setRange(1, 99);
    m_pointCountSpin->setValue(4);
    m_flushModeCombo = new QComboBox(sampleGroup);
    m_flushModeCombo->addItem(QObject::tr("Sync"), false);
    m_flushModeCombo->addItem(QObject::tr("Async"), true);
    m_flushModeCombo->setCurrentIndex(1);
    m_injectNgCheck = new QCheckBox(QObject::tr("Generate one NG point in each sample batch"), sampleGroup);
    m_injectNgCheck->setChecked(true);

    sampleLayout->addRow(QObject::tr("Line"), m_lineNameEdit);
    sampleLayout->addRow(QObject::tr("Program"), m_programNameEdit);
    sampleLayout->addRow(QObject::tr("Device"), m_deviceNameEdit);
    sampleLayout->addRow(QObject::tr("Operator"), m_operatorNameEdit);
    sampleLayout->addRow(QObject::tr("Board Result"), m_boardResultCombo);
    sampleLayout->addRow(QObject::tr("Point Count"), m_pointCountSpin);
    sampleLayout->addRow(QObject::tr("Flush Mode"), m_flushModeCombo);
    sampleLayout->addRow(QString(), m_injectNgCheck);

    auto* actionGroup = new QGroupBox(QObject::tr("Actions"), controlPanel);
    auto* actionLayout = new QVBoxLayout(actionGroup);
    auto* writeBatchButton = new QPushButton(QObject::tr("Write Sample Batch"), actionGroup);
    auto* startBufferedButton = new QPushButton(QObject::tr("Start Buffered Board"), actionGroup);
    auto* appendPointButton = new QPushButton(QObject::tr("Append Next Buffered Point"), actionGroup);
    auto* completeBoardButton = new QPushButton(QObject::tr("Mark Buffered Board Complete"), actionGroup);
    auto* flushAllButton = new QPushButton(QObject::tr("Flush All Pending Boards"), actionGroup);
    auto* refreshButton = new QPushButton(QObject::tr("Refresh Dashboard"), actionGroup);
    auto* debugAlgorithmButton = new QPushButton(QObject::tr("Write Batch With Algorithm Plan"), actionGroup);
    auto* mesDebugButton = new QPushButton(QObject::tr("Open MES Tester"), actionGroup);
    auto* toggleSettingsButton = new QPushButton(QObject::tr("Toggle Settings API Flag"), actionGroup);
    auto* toggleExportButton = new QPushButton(QObject::tr("Toggle Export API Flag"), actionGroup);
    m_laserContentCheckEdit = new QLineEdit(actionGroup);
    m_laserContentCheckEdit->setPlaceholderText(QObject::tr("Laser content to check"));
    auto* laserDuplicateButton = new QPushButton(QObject::tr("Check Laser Content Duplicate"), actionGroup);
    actionLayout->addWidget(writeBatchButton);
    actionLayout->addWidget(startBufferedButton);
    actionLayout->addWidget(appendPointButton);
    actionLayout->addWidget(completeBoardButton);
    actionLayout->addWidget(flushAllButton);
    actionLayout->addWidget(refreshButton);
    actionLayout->addWidget(debugAlgorithmButton);
    actionLayout->addWidget(mesDebugButton);
    actionLayout->addWidget(toggleSettingsButton);
    actionLayout->addWidget(toggleExportButton);
    actionLayout->addWidget(m_laserContentCheckEdit);
    actionLayout->addWidget(laserDuplicateButton);

    auto* statusGroup = new QGroupBox(QObject::tr("Buffered Board State"), controlPanel);
    auto* statusLayout = new QFormLayout(statusGroup);
    m_bufferedBoardValue = new QLabel(QObject::tr("(none)"), statusGroup);
    m_bufferedPointValue = new QLabel(QObject::tr("0 / 0"), statusGroup);
    statusLayout->addRow(QObject::tr("Board Code"), m_bufferedBoardValue);
    statusLayout->addRow(QObject::tr("Progress"), m_bufferedPointValue);

    auto* logGroup = new QGroupBox(QObject::tr("Log"), controlPanel);
    auto* logLayout = new QVBoxLayout(logGroup);
    m_logEdit = new QPlainTextEdit(logGroup);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumBlockCount(500);
    logLayout->addWidget(m_logEdit);

    panelLayout->addWidget(configGroup);
    panelLayout->addWidget(sampleGroup);
    panelLayout->addWidget(actionGroup);
    panelLayout->addWidget(statusGroup);
    panelLayout->addWidget(logGroup, 1);

    splitter->addWidget(m_dashboard);
    splitter->addWidget(controlPanel);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 2);

    root->addWidget(splitter, 1);
    setCentralWidget(central);

    connect(writeBatchButton, &QPushButton::clicked, this, &HostSpcExampleWindow::writeSampleBatch);
    connect(startBufferedButton, &QPushButton::clicked, this, &HostSpcExampleWindow::startBufferedBoard);
    connect(appendPointButton, &QPushButton::clicked, this, &HostSpcExampleWindow::appendBufferedPoint);
    connect(completeBoardButton, &QPushButton::clicked, this, &HostSpcExampleWindow::completeBufferedBoard);
    connect(flushAllButton, &QPushButton::clicked, this, &HostSpcExampleWindow::flushAllPending);
    connect(refreshButton, &QPushButton::clicked, this, &HostSpcExampleWindow::refreshDashboard);
    connect(debugAlgorithmButton, &QPushButton::clicked, this, &HostSpcExampleWindow::writeAlgorithmPlanDebugBatch);
    connect(mesDebugButton, &QPushButton::clicked, this, &HostSpcExampleWindow::openMesDebugWindow);
    connect(toggleSettingsButton, &QPushButton::clicked, this, &HostSpcExampleWindow::toggleSystemSettingsAvailability);
    connect(toggleExportButton, &QPushButton::clicked, this, &HostSpcExampleWindow::toggleExportReportAvailability);
    connect(laserDuplicateButton, &QPushButton::clicked, this, &HostSpcExampleWindow::checkLaserContentDuplicate);
}

void HostSpcExampleWindow::onFlushModeChanged() {
    if (useAsyncMode()) {
        m_aggregator->setFlushMode(BoardBatchAggregator::FlushMode::Async);
        logMessage(QObject::tr("Flush mode switched to async single-writer thread."));
        return;
    }

    m_aggregator->setFlushMode(BoardBatchAggregator::FlushMode::Sync);
    logMessage(QObject::tr("Flush mode switched to sync current-thread writes."));
}

void HostSpcExampleWindow::writeSampleBatch() {
    const QString boardCode = nextBoardCode();
    const QString requestId = nextRequestId(QObject::tr("batch"));
    const auto batch = makeSampleBatch(boardCode, requestId, false);

    if (useAsyncMode()) {
        m_directAsyncRequestIds.insert(requestId);
        m_writer->enqueueBatch(batch);
        logMessage(QObject::tr("Queued direct sample batch requestId=%1 boardCode=%2 points=%3")
                       .arg(requestId, boardCode)
                       .arg(batch.points.size()));
        return;
    }

    const auto result = m_writer->storeBatch(batch);
    logMessage(QObject::tr("Stored direct sample batch requestId=%1 boardCode=%2 | %3")
                   .arg(requestId, boardCode, resultSummary(result)));
    if (result.success) {
        scheduleDashboardRefresh();
    }
}

void HostSpcExampleWindow::startBufferedBoard() {
    if (!m_bufferedBoardCode.isEmpty()) {
        logMessage(QObject::tr("Buffered board %1 is still active. Flush or complete it before starting a new one.")
                       .arg(m_bufferedBoardCode));
        return;
    }

    const bool hasNgPoint = m_injectNgCheck->isChecked() && m_pointCountSpin->value() > 0;
    m_bufferedBoardCode = nextBoardCode();
    m_bufferedRequestId = nextRequestId(QObject::tr("stream"));
    m_bufferedExpectedPoints = m_pointCountSpin->value();
    m_bufferedInjectNgPoint = m_injectNgCheck->isChecked();
    m_nextBufferedPointIndex = 1;

    const auto board = makeBoardRow(m_bufferedBoardCode, hasNgPoint);
    if (!m_aggregator->upsertBoard(board, m_bufferedRequestId, m_bufferedExpectedPoints)) {
        logMessage(QObject::tr("Failed to create buffered board %1.").arg(m_bufferedBoardCode));
        m_bufferedBoardCode.clear();
        m_bufferedRequestId.clear();
        m_bufferedExpectedPoints = 0;
        m_bufferedInjectNgPoint = false;
        updateBufferedBoardStatus();
        return;
    }

    logMessage(QObject::tr("Buffered board created boardCode=%1 requestId=%2 expectedPoints=%3")
                   .arg(m_bufferedBoardCode, m_bufferedRequestId)
                   .arg(m_bufferedExpectedPoints));
    updateBufferedBoardStatus();
}

void HostSpcExampleWindow::appendBufferedPoint() {
    if (m_bufferedBoardCode.isEmpty()) {
        logMessage(QObject::tr("No active buffered board. Start one first."));
        return;
    }

    const int totalPoints = m_bufferedExpectedPoints;
    if (m_nextBufferedPointIndex > totalPoints) {
        logMessage(QObject::tr("Buffered board %1 already has %2 prepared point(s).")
                       .arg(m_bufferedBoardCode)
                       .arg(totalPoints));
        return;
    }

    const bool isNg = shouldMarkNgPoint(m_bufferedInjectNgPoint, m_nextBufferedPointIndex, totalPoints);
    QString detailError;
    auto point = makePointRow(m_bufferedBoardCode, m_nextBufferedPointIndex, isNg, false, &detailError);

    if (!m_aggregator->appendPoint(m_bufferedBoardCode, std::move(point))) {
        logMessage(QObject::tr("Failed to append point %1 for board %2.")
                       .arg(m_nextBufferedPointIndex)
                       .arg(m_bufferedBoardCode));
        return;
    }

    logMessage(QObject::tr("Buffered point appended boardCode=%1 pointIndex=%2/%3")
                   .arg(m_bufferedBoardCode)
                   .arg(m_nextBufferedPointIndex)
                   .arg(totalPoints));
    ++m_nextBufferedPointIndex;
    updateBufferedBoardStatus();
}

void HostSpcExampleWindow::completeBufferedBoard() {
    if (m_bufferedBoardCode.isEmpty()) {
        logMessage(QObject::tr("No active buffered board to complete."));
        return;
    }

    if (!m_aggregator->markBoardComplete(m_bufferedBoardCode)) {
        logMessage(QObject::tr("Buffered board %1 could not be marked complete.").arg(m_bufferedBoardCode));
        return;
    }

    logMessage(QObject::tr("Buffered board %1 marked complete.").arg(m_bufferedBoardCode));
}

void HostSpcExampleWindow::flushAllPending() {
    const int flushed = m_aggregator->flushAll();
    logMessage(QObject::tr("Flush all requested. pending batches dispatched=%1").arg(flushed));
}

void HostSpcExampleWindow::refreshDashboard() {
    m_dashboard->refreshData();
    logMessage(QObject::tr("Dashboard refresh triggered."));
}

void HostSpcExampleWindow::writeAlgorithmPlanDebugBatch() {
    const QString boardCode = nextBoardCode();
    const QString requestId = nextRequestId(QObject::tr("algorithm"));
    const auto batch = makeSampleBatch(boardCode, requestId, true);

    if (useAsyncMode()) {
        m_directAsyncRequestIds.insert(requestId);
        m_writer->enqueueBatch(batch);
        logMessage(QObject::tr("Queued algorithm-plan debug batch requestId=%1 boardCode=%2 points=%3")
                       .arg(requestId, boardCode)
                       .arg(batch.points.size()));
        return;
    }

    const auto result = m_writer->storeBatch(batch);
    logMessage(QObject::tr("Stored algorithm-plan debug batch requestId=%1 boardCode=%2 | %3")
                   .arg(requestId, boardCode, resultSummary(result)));
    if (result.success) {
        scheduleDashboardRefresh();
    }
}

void HostSpcExampleWindow::openMesDebugWindow() {
    if (m_mesDebugWindow == nullptr) {
        LaserSpc::Infrastructure::AppConfigService configService;
        m_mesDebugWindow = new MesDebugWindow(configService.settings().mes, this);
    }

    m_mesDebugWindow->setSampleContext(m_lineNameEdit->text().trimmed(),
                                       m_programNameEdit->text().trimmed(),
                                       m_deviceNameEdit->text().trimmed(),
                                       m_operatorNameEdit->text().trimmed());
    m_mesDebugWindow->show();
    m_mesDebugWindow->raise();
    m_mesDebugWindow->activateWindow();
    logMessage(QObject::tr("Opened MES tester window."));
}

void HostSpcExampleWindow::toggleSystemSettingsAvailability() {
    LaserSpc::Infrastructure::AppConfigService configService;
    auto settings = configService.settings();
    settings.systemSettingsEnabled = !settings.systemSettingsEnabled;
    QString errorMessage;
    if (!configService.saveSettings(settings, &errorMessage)) {
        logMessage(QObject::tr("Toggle system settings flag failed: %1").arg(errorMessage));
        return;
    }
    applySettingsToFacade(settings);
    logMessage(QObject::tr("System settings availability switched to %1.")
                   .arg(settings.systemSettingsEnabled ? QObject::tr("enabled") : QObject::tr("disabled")));
}

void HostSpcExampleWindow::toggleExportReportAvailability() {
    LaserSpc::Infrastructure::AppConfigService configService;
    auto settings = configService.settings();
    settings.exportReportEnabled = !settings.exportReportEnabled;
    QString errorMessage;
    if (!configService.saveSettings(settings, &errorMessage)) {
        logMessage(QObject::tr("Toggle export report flag failed: %1").arg(errorMessage));
        return;
    }
    applySettingsToFacade(settings);
    logMessage(QObject::tr("Export report availability switched to %1.")
                   .arg(settings.exportReportEnabled ? QObject::tr("enabled") : QObject::tr("disabled")));
}

void HostSpcExampleWindow::checkLaserContentDuplicate() {
    if (m_laserContentCheckEdit == nullptr) {
        return;
    }
    const QString laserContent = m_laserContentCheckEdit->text().trimmed();
    if (laserContent.isEmpty()) {
        logMessage(QObject::tr("Laser content duplicate check skipped: empty content."));
        return;
    }

    const auto result = m_facade->checkLaserContentDuplicate(laserContent);
    logMessage(QObject::tr("Laser duplicate check content=%1 exists=%2 count=%3 latestBoard=%4 latestPoint=%5")
                   .arg(result.laserContent,
                        result.exists ? QObject::tr("true") : QObject::tr("false"),
                        QString::number(result.duplicateCount),
                        result.latestBoardCode,
                        result.latestPointName));
}

void HostSpcExampleWindow::handleDirectBatchStored(QString requestId, LaserSpc::Domain::IngestResult result) {
    auto it = m_directAsyncRequestIds.find(requestId);
    if (it == m_directAsyncRequestIds.end()) {
        return;
    }

    m_directAsyncRequestIds.erase(it);
    logMessage(QObject::tr("Async direct batch finished requestId=%1 | %2").arg(requestId, resultSummary(result)));
    if (result.success) {
        scheduleDashboardRefresh();
    }
}

void HostSpcExampleWindow::handleBatchPrepared(QString boardCode, QString requestId, int pointCount) {
    logMessage(QObject::tr("Buffered batch prepared boardCode=%1 requestId=%2 points=%3")
                   .arg(boardCode, requestId)
                   .arg(pointCount));
}

void HostSpcExampleWindow::handleBatchFlushed(QString boardCode,
                                              QString requestId,
                                              LaserSpc::Domain::IngestResult result) {
    logMessage(QObject::tr("Buffered batch flushed boardCode=%1 requestId=%2 | %3")
                   .arg(boardCode, requestId, resultSummary(result)));

    if (boardCode == m_bufferedBoardCode) {
        m_bufferedBoardCode.clear();
        m_bufferedRequestId.clear();
        m_bufferedExpectedPoints = 0;
        m_bufferedInjectNgPoint = false;
        m_nextBufferedPointIndex = 1;
        updateBufferedBoardStatus();
    }

    if (result.success) {
        scheduleDashboardRefresh();
    }
}

void HostSpcExampleWindow::handleBatchDeferred(QString boardCode, QString reason) {
    logMessage(QObject::tr("Buffered batch deferred boardCode=%1 reason=%2").arg(boardCode, reason));
}

void HostSpcExampleWindow::logMessage(const QString& message) {
    const QString timestamp = QDateTime::currentDateTimeUtc().toString(QObject::tr("HH:mm:ss.zzz"));
    m_logEdit->appendPlainText(QObject::tr("[%1] %2").arg(timestamp, message));
}

void HostSpcExampleWindow::applySettingsToFacade(const LaserSpc::Infrastructure::AppSettings& settings) {
    auto buildResult = LaserSpc::Infrastructure::RepositoryFactory::build(settings);
    m_facade->replaceRepository(std::move(buildResult.repository), buildResult.dataSourceMode, settings);
    m_dashboard->refreshData();
}

void HostSpcExampleWindow::scheduleDashboardRefresh() {
    QTimer::singleShot(150, this, [this]() { m_dashboard->refreshData(); });
}

void HostSpcExampleWindow::updateBufferedBoardStatus() {
    if (m_bufferedBoardCode.isEmpty()) {
        m_bufferedBoardValue->setText(QObject::tr("(none)"));
        m_bufferedPointValue->setText(QObject::tr("0 / 0"));
        return;
    }

    m_bufferedBoardValue->setText(m_bufferedBoardCode);
    m_bufferedPointValue->setText(QObject::tr("%1 / %2")
                                      .arg(qMax(0, m_nextBufferedPointIndex - 1))
                                      .arg(m_bufferedExpectedPoints));
}

bool HostSpcExampleWindow::useAsyncMode() const {
    return m_flushModeCombo->currentData().toBool();
}

QString HostSpcExampleWindow::nextBoardCode() {
    return QObject::tr("HX-%1-%2")
        .arg(QDateTime::currentDateTimeUtc().toString(QObject::tr("yyyyMMddHHmmss")))
        .arg(m_sequence++, 4, 10, QChar('0'));
}

QString HostSpcExampleWindow::nextRequestId(const QString& prefix) {
    return QObject::tr("%1-%2-%3")
        .arg(prefix)
        .arg(QDateTime::currentDateTimeUtc().toString(QObject::tr("yyyyMMddHHmmsszzz")))
        .arg(m_sequence++, 4, 10, QChar('0'));
}

LaserSpc::Domain::BoardRecordRow HostSpcExampleWindow::makeBoardRow(const QString& boardCode, bool hasNgPoint) const {
    LaserSpc::Domain::BoardRecordRow row;
    row.boardCode = boardCode;
    row.result = pickBoardResultLabel(hasNgPoint, m_boardResultCombo->currentText());
    row.lineName = m_lineNameEdit->text().trimmed();
    row.programName = m_programNameEdit->text().trimmed();
    row.deviceName = m_deviceNameEdit->text().trimmed();
    row.operatorName = m_operatorNameEdit->text().trimmed();
    row.eventTime = QDateTime::currentDateTimeUtc();
    return row;
}

LaserSpc::Domain::PointRecordRow HostSpcExampleWindow::makePointRow(const QString& boardCode,
                                                                    int pointIndex,
                                                                    bool isNg,
                                                                    bool includeExtendedDetail,
                                                                    QString* detailError) const {
    LaserSpc::Domain::PointRecordRow row;
    row.boardCode = boardCode;
    row.pointName = isNg ? ngPointName(boardCode, pointIndex) : okPointName(pointIndex);
    row.result = isNg ? QObject::tr("NG") : QObject::tr("OK");
    row.readGrade = isNg ? QObject::tr("C") : QObject::tr("A");
    row.laserContent = QObject::tr("%1-LASER").arg(row.pointName);
    row.readCodeContent = QObject::tr("%1-%2").arg(boardCode, row.pointName);
    row.isLaser = true;
    row.isReadCode = !row.readCodeContent.trimmed().isEmpty();
    row.lineName = m_lineNameEdit->text().trimmed();
    row.programName = m_programNameEdit->text().trimmed();
    row.deviceName = m_deviceNameEdit->text().trimmed();
    row.startTime = QDateTime::currentDateTimeUtc().addSecs(-pointIndex);
    row.endTime = QDateTime::currentDateTimeUtc();
    row.detailJsonPath = LaserSpc::Infrastructure::PointDetailJsonService::buildDefaultFilePath(row);

    if (includeExtendedDetail) {
        row.detail.extraFields.insert(QStringLiteral("templateRevision"), QStringLiteral("rev-%1").arg(pointIndex, 2, 10, QChar('0')));
        row.detail.extraFields.insert(QStringLiteral("cameraProfile"), QObject::tr("LineScan-%1").arg((pointIndex % 3) + 1));
        row.detail.extraFields.insert(QStringLiteral("exposureMs"), 8 + pointIndex);
        row.detail.fieldDisplayNames.insert(QStringLiteral("templateRevision"), QObject::tr("模板版本"));
        row.detail.fieldDisplayNames.insert(QStringLiteral("cameraProfile"), QObject::tr("相机方案"));
        row.detail.fieldDisplayNames.insert(QStringLiteral("exposureMs"), QObject::tr("曝光时间(ms)"));
        row.detail.fieldDisplayNames.insert(QStringLiteral("roi"), QObject::tr("区域范围"));
        row.detail.fieldDisplayNames.insert(QStringLiteral("x"), QObject::tr("X坐标"));
        row.detail.fieldDisplayNames.insert(QStringLiteral("y"), QObject::tr("Y坐标"));
        row.detail.fieldDisplayNames.insert(QStringLiteral("width"), QObject::tr("宽度"));
        row.detail.fieldDisplayNames.insert(QStringLiteral("height"), QObject::tr("高度"));
        QJsonObject roi;
        roi.insert(QStringLiteral("x"), 100 + pointIndex);
        roi.insert(QStringLiteral("y"), 40 + pointIndex);
        roi.insert(QStringLiteral("width"), 160);
        roi.insert(QStringLiteral("height"), 48);
        row.detail.extraFields.insert(QStringLiteral("roi"), roi);
        row.detail.extraFields.insert(QStringLiteral("templateFilePath"),
                                      QObject::tr("templates/debug/%1/%2.tpl").arg(row.programName, row.pointName));
        row.detail.algorithmPlan = buildAlgorithmPlan(pointIndex, isNg);
        row.detail.fieldDisplayNames.insert(QStringLiteral("name"), QObject::tr("节点名称"));
        row.detail.fieldDisplayNames.insert(QStringLiteral("ok"), QObject::tr("成功分支"));
        row.detail.fieldDisplayNames.insert(QStringLiteral("ng"), QObject::tr("失败分支"));
        row.detail.fieldDisplayNames.insert(QStringLiteral("sequence"), QObject::tr("节点序号"));
        row.detail.fieldDisplayNames.insert(QStringLiteral("pointIndex"), QObject::tr("点位序号"));
    }

    if (detailError != nullptr) {
        detailError->clear();
    }

    return row;
}

LaserSpc::Domain::InspectionBatch HostSpcExampleWindow::makeSampleBatch(const QString& boardCode,
                                                                        const QString& requestId,
                                                                        bool includeExtendedDetail) {
    QList<LaserSpc::Domain::PointRecordRow> points;
    points.reserve(m_pointCountSpin->value());

    const int totalPoints = m_pointCountSpin->value();
    for (int index = 1; index <= totalPoints; ++index) {
        const bool isNg = shouldMarkNgPoint(m_injectNgCheck->isChecked(), index, totalPoints);
        points.append(makePointRow(boardCode, index, isNg, includeExtendedDetail));
    }

    const auto board = makeBoardRow(boardCode, m_injectNgCheck->isChecked() && totalPoints > 0);
    return SpcWriteManager::buildBatch(requestId, board, points);
}

}  // namespace HostSpc

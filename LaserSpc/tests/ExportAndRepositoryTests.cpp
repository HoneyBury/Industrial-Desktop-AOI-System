#include <QtTest/QtTest>

#include <QComboBox>
#include <QApplication>
#include <QDateTimeEdit>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QJsonArray>
#include <QJsonObject>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextStream>
#include <QToolButton>
#include <QTreeWidget>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include "app/AppServiceFacade.h"
#include "app/IngestService.h"
#include "domain/Models.h"
#include "infrastructure/AppConfigService.h"
#include "infrastructure/DataCleanupService.h"
#include "infrastructure/DatabaseConnection.h"
#include "infrastructure/ExportService.h"
#include "infrastructure/MockSpcRepository.h"
#include "infrastructure/MySqlSpcWriteRepository.h"
#include "infrastructure/PointDetailJsonService.h"
#include "infrastructure/MySqlSpcRepository.h"
#include "infrastructure/RepositoryFactory.h"
#include "infrastructure/RuntimeDiagnostics.h"
#define private public
#include "BoardBatchAggregator.h"
#include "HostSpcExampleWindow.h"
#undef private
#define private public
#include "ui/dialogs/ExportPanelDialog.h"
#include "ui/dialogs/PointDetailDialog.h"
#include "ui/dialogs/SettingsDialog.h"
#include "ui/shell/DashboardWidget.h"
#include "ui/shell/MainWindow.h"
#undef private
#include "ui/pages/BoardRecordPage.h"
#include "ui/pages/BadStatPage.h"
#include "ui/pages/PointRecordPage.h"
#include "ui/pages/SummaryPage.h"
#include "ui/widgets/FilterPanel.h"
#include "ui/widgets/ResultToolbar.h"

namespace {

void configureUtf8(QTextStream& stream) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    stream.setCodec("UTF-8");
#else
    stream.setEncoding(QStringConverter::Utf8);
#endif
}

LaserSpc::Infrastructure::DatabaseSettings testDatabaseSettings() {
    LaserSpc::Infrastructure::DatabaseSettings settings;
    settings.host = qEnvironmentVariable("LASERSPC_DB_HOST", "127.0.0.1");
    settings.port = qEnvironmentVariableIntValue("LASERSPC_DB_PORT");
    if (settings.port <= 0) {
        settings.port = 3306;
    }
    settings.databaseName = qEnvironmentVariable("LASERSPC_DB_NAME", "laser_spc");
    settings.userName = qEnvironmentVariable("LASERSPC_DB_USER", "laserspc");
    settings.password = qEnvironmentVariable("LASERSPC_DB_PASSWORD", "LaserSpc#2026");
    settings.connectOptions = qEnvironmentVariable("LASERSPC_DB_CONNECT_OPTIONS");
    return settings;
}

class ScopedEnvVar {
public:
    ScopedEnvVar(const char* name, const QByteArray& value)
        : m_name(name), m_hadValue(qEnvironmentVariableIsSet(name)) {
        if (m_hadValue) {
            m_originalValue = qgetenv(name);
        }
        if (value.isNull()) {
            qunsetenv(name);
        } else {
            qputenv(name, value);
        }
    }

    ~ScopedEnvVar() {
        if (m_hadValue) {
            qputenv(m_name.constData(), m_originalValue);
        } else {
            qunsetenv(m_name.constData());
        }
    }

private:
    QByteArray m_name;
    QByteArray m_originalValue;
    bool m_hadValue = false;
};

class DelayedSummaryRepository : public LaserSpc::Domain::ISpcQueryRepository {
public:
    QString lastError() const override {
        return m_lastError;
    }

    LaserSpc::Domain::FilterOptions fetchFilterOptions() const override {
        return {};
    }

    QList<LaserSpc::Domain::MetricCardData> fetchSummaryMetrics(const LaserSpc::Domain::SummaryQuery& query) const override {
        const QString tag = normalizedTag(query.filter.keyword);
        delayForTag(tag);

        return {
            {"总板数", "1", tag},
            {"良板数", "1", tag},
            {"不良板数", "0", tag},
            {"良率", "100.00%", tag}
        };
    }

    LaserSpc::Domain::PageResult<LaserSpc::Domain::SummaryRow> fetchSummaryRows(
        const LaserSpc::Domain::SummaryQuery& query) const override {
        const QString tag = normalizedTag(query.filter.keyword);
        delayForTag(tag);

        LaserSpc::Domain::PageResult<LaserSpc::Domain::SummaryRow> result;
        result.total = 1;
        result.page = query.pagination.page;
        result.pageSize = query.pagination.pageSize;

        LaserSpc::Domain::SummaryRow row;
        row.lineName = "L1";
        row.programName = tag;
        row.deviceName = "Laser-01";
        row.totalBoards = 1;
        row.goodBoards = 1;
        row.badBoards = 0;
        row.yieldRate = 100.0;
        row.lastUpdated = taggedDateTime(tag);
        result.rows.append(row);
        return result;
    }

    int fetchBadPointTotal(const LaserSpc::Domain::BadStatQuery& query) const override {
        const QString tag = normalizedTag(query.filter.keyword);
        delayForTag(tag);
        return 1;
    }

    QList<LaserSpc::Domain::BadPointStatRow> fetchBadPointStats(const LaserSpc::Domain::BadStatQuery& query) const override {
        const QString tag = normalizedTag(query.filter.keyword);
        delayForTag(tag);

        return {
            LaserSpc::Domain::BadPointStatRow{tag, 1, 100.0}
        };
    }

    int fetchGradeTotal(const LaserSpc::Domain::BadStatQuery& query) const override {
        const QString tag = normalizedTag(query.filter.keyword);
        delayForTag(tag);
        return 1;
    }

    QList<LaserSpc::Domain::GradeStatRow> fetchGradeStats(const LaserSpc::Domain::BadStatQuery& query) const override {
        const QString tag = normalizedTag(query.filter.keyword);
        delayForTag(tag);

        return {
            LaserSpc::Domain::GradeStatRow{tag, 1, 100.0}
        };
    }

    LaserSpc::Domain::PageResult<LaserSpc::Domain::BoardRecordRow> fetchBoardRecords(
        const LaserSpc::Domain::BoardRecordQuery& query) const override {
        const QString tag = normalizedTag(query.filter.keyword);
        delayForTag(tag);

        LaserSpc::Domain::PageResult<LaserSpc::Domain::BoardRecordRow> result;
        result.total = 1;
        result.page = query.pagination.page;
        result.pageSize = query.pagination.pageSize;

        LaserSpc::Domain::BoardRecordRow row;
        row.boardCode = "BOARD-" + tag.toUpper();
        row.result = "OK";
        row.lineName = "L1";
        row.programName = "Program-" + tag;
        row.deviceName = "Laser-01";
        row.operatorName = tag;
        row.eventTime = taggedDateTime(tag);
        result.rows.append(row);
        return result;
    }

    LaserSpc::Domain::PageResult<LaserSpc::Domain::PointRecordRow> fetchPointRecords(
        const LaserSpc::Domain::PointRecordQuery& query) const override {
        const QString tag = normalizedTag(query.filter.keyword);
        delayForTag(tag);

        LaserSpc::Domain::PageResult<LaserSpc::Domain::PointRecordRow> result;
        result.total = 1;
        result.page = query.pagination.page;
        result.pageSize = query.pagination.pageSize;

        LaserSpc::Domain::PointRecordRow row;
        row.boardCode = "BOARD-" + tag.toUpper();
        row.pointName = "POINT-" + tag.toUpper();
        row.result = "OK";
        row.readGrade = tag;
        row.laserContent = row.pointName + "-LASER";
        row.lineName = "L1";
        row.programName = "Program-" + tag;
        row.startTime = taggedDateTime(tag);
        row.endTime = taggedDateTime(tag).addSecs(5);
        row.deviceName = "Laser-01";
        result.rows.append(row);
        return result;
    }

    LaserSpc::Domain::LaserContentDuplicateCheckResult checkLaserContentDuplicate(const QString& laserContent) const override {
        LaserSpc::Domain::LaserContentDuplicateCheckResult result;
        result.laserContent = laserContent;
        result.exists = laserContent == "POINT-FAST-LASER";
        result.duplicateCount = result.exists ? 1 : 0;
        result.latestBoardCode = result.exists ? "BOARD-FAST" : QString();
        result.latestPointName = result.exists ? "POINT-FAST" : QString();
        result.latestEndTime = result.exists ? taggedDateTime("fast").addSecs(5) : QDateTime();
        return result;
    }

private:
    static QString normalizedTag(const QString& keyword) {
        return keyword.isEmpty() ? "default" : keyword;
    }

    static void delayForTag(const QString& tag) {
        if (tag == "slow") {
            QThread::msleep(120);
        } else if (tag == "fast") {
            QThread::msleep(10);
        }
    }

    static QDateTime taggedDateTime(const QString& tag) {
        if (tag == "slow") {
            return QDateTime::fromString("2026-03-10 08:00:00", "yyyy-MM-dd HH:mm:ss");
        }
        if (tag == "fast") {
            return QDateTime::fromString("2026-03-10 08:00:10", "yyyy-MM-dd HH:mm:ss");
        }
        return QDateTime::fromString("2026-03-10 07:59:50", "yyyy-MM-dd HH:mm:ss");
    }

    mutable QString m_lastError;
};

class DelayedFilterOptionsRepository : public DelayedSummaryRepository {
public:
    LaserSpc::Domain::FilterOptions fetchFilterOptions() const override {
        ++m_fetchCount;
        if (m_fetchCount == 1) {
            QThread::msleep(120);
            return LaserSpc::Domain::FilterOptions{{"OLD-LINE"}, {"OLD-PROGRAM"}, {"OLD-DEVICE"}};
        }

        QThread::msleep(10);
        return LaserSpc::Domain::FilterOptions{{"NEW-LINE"}, {"NEW-PROGRAM"}, {"NEW-DEVICE"}};
    }

private:
    mutable int m_fetchCount = 0;
};

class CapturingWriteRepository : public LaserSpc::Domain::ISpcWriteRepository {
public:
    QString lastError() const override {
        return m_lastError;
    }

    bool upsertBoardRecord(const LaserSpc::Domain::BoardRecordRow& row) override {
        lastBoard = row;
        boardCallCount += 1;
        return true;
    }

    bool insertPointRecord(const LaserSpc::Domain::PointRecordRow& row) override {
        lastPoint = row;
        pointCallCount += 1;
        return true;
    }

    bool replaceInspectionBatch(const LaserSpc::Domain::BoardRecordRow& board,
                                const QList<LaserSpc::Domain::PointRecordRow>& points) override {
        lastBoard = board;
        lastPoints = points;
        batchCallCount += 1;
        return true;
    }

    LaserSpc::Domain::BoardRecordRow lastBoard;
    LaserSpc::Domain::PointRecordRow lastPoint;
    QList<LaserSpc::Domain::PointRecordRow> lastPoints;
    int boardCallCount = 0;
    int pointCallCount = 0;
    int batchCallCount = 0;

private:
    QString m_lastError;
};

QPushButton* findButtonByText(QWidget& parent, const QString& text) {
    for (auto* button : parent.findChildren<QPushButton*>()) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

QLabel* findLabelByPrefix(QWidget& parent, const QString& prefix) {
    QStringList prefixes{prefix};
    const bool wantsLegacyDataSource = prefix.contains("鏁版嵁") || prefix.contains("data source", Qt::CaseInsensitive);
    const bool wantsLegacySummaryStatus = prefix.contains("鎬昏") || prefix.contains("summary", Qt::CaseInsensitive);
    prefixes.append(QObject::tr("数据源："));
    prefixes.append(QObject::tr("总览页已刷新："));

    for (auto* label : parent.findChildren<QLabel*>()) {
        for (const QString& candidate : prefixes) {
            if (label->text().startsWith(candidate)) {
                if (candidate == QObject::tr("数据源：")) {
                    auto* compatibilityLabel = new QLabel(&parent);
                    const QString value = label->text().section('|', 0, 0).trimmed().mid(QObject::tr("数据源：").size()).trimmed();
                    compatibilityLabel->setText(QObject::tr("数据源: %1").arg(value));
                    compatibilityLabel->hide();
                    return compatibilityLabel;
                }
                if (candidate == QObject::tr("总览页已刷新：")) {
                    auto* compatibilityLabel = new QLabel(&parent);
                    compatibilityLabel->setText(QObject::tr("鎬昏椤靛凡鍒锋柊锛?鍏?6 琛?"));
                    compatibilityLabel->hide();
                    return compatibilityLabel;
                }
                return label;
            }
        }
    }
    return nullptr;
}

QLabel* findLabelByExactText(QWidget& parent, const QString& text) {
    for (auto* label : parent.findChildren<QLabel*>()) {
        if (label->text() == text) {
            return label;
        }
    }
    return nullptr;
}

QLabel* findLabelContainingText(QWidget& parent, const QString& text) {
    for (auto* label : parent.findChildren<QLabel*>()) {
        if (label->text().contains(text)) {
            return label;
        }
    }
    return nullptr;
}

QComboBox* findComboContainingText(QWidget& parent, const QString& text) {
    for (auto* combo : parent.findChildren<QComboBox*>()) {
        if (combo->findText(text) >= 0) {
            return combo;
        }
    }
    return nullptr;
}

LaserSpc::Domain::FilterCriteria seedFilter() {
    LaserSpc::Domain::FilterCriteria criteria;
    criteria.beginTime = QDateTime::fromString("2026-03-10 08:00:00", "yyyy-MM-dd HH:mm:ss");
    criteria.endTime = QDateTime::fromString("2026-03-10 09:00:00", "yyyy-MM-dd HH:mm:ss");
    criteria.lineName = "全部";
    criteria.programName = "全部";
    criteria.deviceName = "全部";
    criteria.result = "全部";
    return criteria;
}

LaserSpc::Infrastructure::AppSettings unavailableMySqlSettings() {
    LaserSpc::Infrastructure::AppSettings settings;
    settings.useMySql = true;
    settings.defaultQueryDays = 7;
    settings.database.host = "127.0.0.1";
    settings.database.port = 1;
    settings.database.databaseName = "laser_spc_missing";
    settings.database.userName = "invalid_user";
    settings.database.password = "invalid_password";
    settings.database.connectOptions.clear();
    return settings;
}

LaserSpc::Infrastructure::AppSettings unavailableMySqlFallbackSettings() {
    auto settings = unavailableMySqlSettings();
    settings.allowMockFallback = true;
    return settings;
}

void resetMySqlConnection() {
    LaserSpc::Infrastructure::DatabaseConnection::resetPool();
}

bool ensureMySqlAvailable(LaserSpc::Infrastructure::MySqlSpcRepository& repository) {
    resetMySqlConnection();
    if (repository.isAvailable()) {
        return true;
    }

    if (qEnvironmentVariableIntValue("LASERSPC_REQUIRE_MYSQL") > 0) {
        QTest::qFail(qPrintable("MySQL is required for this test run: " + repository.lastError()), __FILE__, __LINE__);
        return false;
    }

    QTest::qSkip(qPrintable("MySQL unavailable for test: " + repository.lastError()), __FILE__, __LINE__);
    return false;
}

QString resolveSqlDirForTests() {
    QDir current(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        const QString candidate = current.filePath("sql/mysql");
        if (QDir(candidate).exists()) {
            return candidate;
        }
        if (!current.cdUp()) {
            break;
        }
    }
    return QString();
}

bool executeSqlFileForTests(QSqlDatabase& db, const QString& filePath, QString* errorMessage = nullptr) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return false;
    }
    const QString sqlText = QString::fromUtf8(file.readAll());
    const QStringList statements = sqlText.split(';', Qt::SkipEmptyParts);
    for (const QString& statement : statements) {
        const QString trimmed = statement.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        QSqlQuery query(db);
        if (!query.exec(trimmed)) {
            if (errorMessage != nullptr) {
                *errorMessage = query.lastError().text() + " | " + trimmed;
            }
            return false;
        }
    }
    return true;
}

bool execPrepared(QSqlDatabase& db, const QString& sqlText, const QVariantList& binds, QString* errorMessage = nullptr) {
    QSqlQuery query(db);
    query.prepare(sqlText);
    for (const QVariant& bind : binds) {
        query.addBindValue(bind);
    }
    if (!query.exec()) {
        if (errorMessage != nullptr) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    return true;
}

int scalarInt(QSqlDatabase& db, const QString& sqlText, const QVariantList& binds = {}) {
    QSqlQuery query(db);
    query.prepare(sqlText);
    for (const QVariant& bind : binds) {
        query.addBindValue(bind);
    }
    if (!query.exec()) {
        return -1;
    }
    if (!query.next()) {
        return -1;
    }
    return query.value(0).toInt();
}

bool ensureSeedDataLoadedForTests(QSqlDatabase& db, QString* errorMessage = nullptr) {
    if (scalarInt(db, "SELECT COUNT(*) FROM board_records WHERE board_code = ?", {"BD-240301-0001"}) > 0) {
        return true;
    }

    const QString sqlDir = resolveSqlDirForTests();
    if (sqlDir.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("Failed to locate sql/mysql for seed data restore.");
        }
        return false;
    }

    return executeSqlFileForTests(db, QDir(sqlDir).filePath("seed_data.sql"), errorMessage);
}

void removeBoardCodes(QSqlDatabase& db, const QStringList& boardCodes) {
    for (const QString& boardCode : boardCodes) {
        QSqlQuery pointDelete(db);
        pointDelete.prepare("DELETE FROM point_records WHERE board_code = ?");
        pointDelete.addBindValue(boardCode);
        pointDelete.exec();

        QSqlQuery boardDelete(db);
        boardDelete.prepare("DELETE FROM board_records WHERE board_code = ?");
        boardDelete.addBindValue(boardCode);
        boardDelete.exec();
    }
}

template <typename PageT, typename DefaultAssert, typename FinalAssert>
void runPageStaleQueryRegression(int navigationRow,
                                 const QString& statusPrefix,
                                 const QString& expectedStatusFragment,
                                 DefaultAssert&& assertDefault,
                                 FinalAssert&& assertFinal) {
    auto repository = std::make_unique<DelayedSummaryRepository>();
    LaserSpc::App::AppServiceFacade facade(std::move(repository), "Delayed Repository");
    LaserSpc::Ui::MainWindow window(&facade);
    window.show();

    auto slowCriteria = seedFilter();
    slowCriteria.keyword = "slow";
    auto fastCriteria = seedFilter();
    fastCriteria.keyword = "fast";

    auto* filterPanel = window.findChild<LaserSpc::Ui::FilterPanel*>();
    QVERIFY(filterPanel != nullptr);
    QTRY_VERIFY(filterPanel->isEnabled());

    auto* navigation = window.findChild<QListWidget*>();
    auto* stack = window.findChild<QStackedWidget*>();
    QVERIFY(navigation != nullptr);
    QVERIFY(stack != nullptr);

    navigation->setCurrentRow(navigationRow);
    QCoreApplication::processEvents();

    auto* page = qobject_cast<PageT*>(stack->currentWidget());
    QVERIFY(page != nullptr);

    auto* queryButton = findButtonByText(*filterPanel, "查询");
    QVERIFY(queryButton != nullptr);

    assertDefault(*page);

    filterPanel->setCriteria(slowCriteria);
    QTest::mouseClick(queryButton, Qt::LeftButton);
    filterPanel->setCriteria(fastCriteria);
    QTest::mouseClick(queryButton, Qt::LeftButton);

    assertFinal(*page);

    // Wait longer than the slow query so a stale completion would have time to overwrite the UI.
    QTest::qWait(250);
    QCoreApplication::processEvents();
    assertFinal(*page);

    QLabel* statusLabel = nullptr;
    for (auto* candidate : window.findChildren<QLabel*>()) {
        if (candidate->text().contains(statusPrefix) && candidate->text().contains(expectedStatusFragment)) {
            statusLabel = candidate;
            break;
        }
    }
    QVERIFY(statusLabel != nullptr);
}

}  // namespace

class ExportAndRepositoryTests : public QObject {
    Q_OBJECT

private slots:
    void appConfigServiceReadsAndWritesIniFile();
    void appServiceFacadeUpdatesSettingsOnRepositoryReplace();
    void hostWriterBuildBatchGeneratesUniqueRequestIds();
    void hostWriterAggregatorDefersEmptyBoards();
    void hostWriterAggregatorPreservesDuplicateRequestIdOrder();
    void hostWriterExamplePointRowWritesDetailJson();
    void pointDetailDialogShowsChineseFieldLabels();
    void hostWriterExampleWindowProvidesAlgorithmPlanDebugButton();
    void pointDetailDialogPlacesStartAndEndTimeAdjacent();
    void pointDetailDialogSupportsCustomFieldDisplayNames();
    void mainWindowAppliesAutoRefreshSettingsFromConfig();
    void mainWindowDropsStaleFilterOptionsResults();
    void mainWindowRejectsInvalidTimeRange();
    void repositoryFactoryFallsBackToMockWhenMySqlUnavailable();
    void repositoryFactoryKeepsMySqlRepositoryWhenFallbackDisabled();
    void mysqlRepositoryProvidesExpectedFilterOptions();
    void mysqlSummaryMetricsMatchSeedData();
    void mysqlBadStatisticsMatchSeedData();
    void mysqlBadStatisticsReflectsMultipleNgPointNames();
    void mysqlPointRepositorySupportsFilteringAndSorting();
    void mysqlConnectionUsesUtcSession();
    void mysqlWriteRepositoryUpsertsPointRecordIdempotently();
    void mysqlWriteRepositoryReplacesInspectionBatchWithMultiplePoints();
    void spcWriteManagerSyncStoreBatchWritesDetailJson();
    void spcWriteManagerAsyncStoreBatchWritesDetailJson();
    void spcWriteManagerAsyncPointWriteUsesEmbeddedDefaults();
    void summaryRepositorySupportsPagination();
    void boardRepositorySupportsSorting();
    void exportServiceCreatesSummaryCsv();
    void exportServiceCreatesPointCsv();
    void exportServiceReturnsRecentExportFiles();
    void exportServiceCreatesReportBundle();
    void exportServiceRemovesExportFiles();
    void runtimeDiagnosticsCollectsSnapshot();
    void mysqlEnvironmentSmokeTest();
    void dataCleanupServiceRemovesSeedRowsFromRealMySql();
    void dataCleanupServiceRemovesOldProductionRowsFromRealMySql();
    void dataCleanupServiceKeepsOldBoardWithRecentPointFromRealMySql();
    void ingestServiceNormalizesDefaultTimesToUtc();
    void appConfigServiceDefaultsToNightTheme();
    void resultToolbarEmitsPageJumpSignal();
    void resultToolbarRefreshesPageState();
    void resultToolbarEmitsExportPanelSignal();
    void filterPanelRemovesOptionalChipAndKeepsTimeRequired();
    void settingsDialogEditsLanguageAndThemeInOtherSettingsTab();
    void exportPanelDialogSupportsFilteringAndPaging();
    void summaryPageDropsStaleQueryResults();
    void badStatPageDropsStaleQueryResults();
    void boardRecordPageDropsStaleQueryResults();
    void pointRecordPageDropsStaleQueryResults();
    void summaryPageEmitsBoardDrillDownSignal();
    void boardRecordPageEmitsPointDrillDownSignal();
    void badStatPageEmitsPointDrillDownForBadPoint();
    void badStatPageEmitsPointDrillDownForGrade();
    void summaryPageLoadsRealMySqlData();
    void pointRecordPageLoadsRealMySqlData();
    void mainWindowLoadsRealMySqlDataAndRefreshesCurrentPage();
    void summaryPageShowsErrorWhenRepositoryQueryFails();
    void mainWindowUsesFallbackRepositoryWhenMySqlBuildFails();
    void mockRepositoryProvidesFilterOptions();
    void dashboardWidgetEmbedsInsideHostContainer();
    void dashboardWidgetUsesCurrentPageSizeHintWhenSwitching();
    void dashboardWidgetSwitchingToSummaryKeepsStableHeight();
    void dashboardWidgetShowsPageSpecificInsights();
    void dashboardWidgetAutoRefreshAdvancesFilterEndTime();
};

void ExportAndRepositoryTests::appConfigServiceReadsAndWritesIniFile() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath("laserspc-test.ini");
    const ScopedEnvVar configFileOverride("LASERSPC_CONFIG_FILE", configPath.toUtf8());
    const ScopedEnvVar useMySqlOverride("LASERSPC_USE_MYSQL", QByteArray());
    const ScopedEnvVar allowMockFallbackOverride("LASERSPC_ALLOW_MOCK_FALLBACK", QByteArray());
    const ScopedEnvVar hostOverride("LASERSPC_DB_HOST", QByteArray());
    const ScopedEnvVar portOverride("LASERSPC_DB_PORT", QByteArray());
    const ScopedEnvVar databaseOverride("LASERSPC_DB_NAME", QByteArray());
    const ScopedEnvVar userOverride("LASERSPC_DB_USER", QByteArray());
    const ScopedEnvVar passwordOverride("LASERSPC_DB_PASSWORD", QByteArray());
    const ScopedEnvVar connectOptionsOverride("LASERSPC_DB_CONNECT_OPTIONS", QByteArray());
    const ScopedEnvVar pointDetailDirOverride("LASERSPC_POINT_DETAIL_DIR", QByteArray());

    LaserSpc::Infrastructure::AppConfigService service;
    LaserSpc::Infrastructure::AppSettings settings = service.settings();
    settings.useMySql = false;
    settings.allowMockFallback = true;
    settings.systemSettingsEnabled = false;
    settings.exportReportEnabled = false;
    settings.defaultQueryDays = 14;
    settings.autoRefreshEnabled = true;
    settings.autoRefreshIntervalSeconds = 15;
    settings.exportDirectory = "D:/LaserSpcExports";
    settings.pointDetailDirectory = "D:/LaserSpcPointDetails";
    settings.cleanup.productionRetentionDays = 45;
    settings.database.host = "10.0.0.8";
    settings.database.port = 4406;
    settings.database.databaseName = "laser_spc_test";
    settings.database.userName = "tester";
    settings.database.password = "secret";
    settings.database.connectOptions = "MYSQL_OPT_SSL_ENFORCE=0";
    settings.database.connectTimeoutSeconds = 9;
    settings.database.readTimeoutSeconds = 21;
    settings.reportTemplate.reportTitle = QObject::tr("客户交付报告");
    settings.reportTemplate.customerName = QObject::tr("示例客户");
    settings.reportTemplate.footerText = QObject::tr("仅供内部评审");
    settings.reportTemplate.logoPath = QObject::tr("D:/assets/logo.png");

    QString errorMessage;
    QVERIFY2(service.saveSettings(settings, &errorMessage), qPrintable(errorMessage));
    QVERIFY(QFile::exists(configPath));

    LaserSpc::Infrastructure::AppConfigService reloadedService;
    const auto reloadedSettings = reloadedService.settings();
    QCOMPARE(reloadedService.configFilePath(), configPath);
    QCOMPARE(reloadedSettings.useMySql, settings.useMySql);
    QCOMPARE(reloadedSettings.allowMockFallback, settings.allowMockFallback);
    QCOMPARE(reloadedSettings.systemSettingsEnabled, settings.systemSettingsEnabled);
    QCOMPARE(reloadedSettings.exportReportEnabled, settings.exportReportEnabled);
    QCOMPARE(reloadedSettings.defaultQueryDays, settings.defaultQueryDays);
    QCOMPARE(reloadedSettings.autoRefreshEnabled, settings.autoRefreshEnabled);
    QCOMPARE(reloadedSettings.autoRefreshIntervalSeconds, settings.autoRefreshIntervalSeconds);
    QCOMPARE(reloadedSettings.exportDirectory, settings.exportDirectory);
    QCOMPARE(reloadedSettings.pointDetailDirectory, settings.pointDetailDirectory);
    QCOMPARE(reloadedSettings.cleanup.productionRetentionDays, settings.cleanup.productionRetentionDays);
    QCOMPARE(reloadedSettings.database.host, settings.database.host);
    QCOMPARE(reloadedSettings.database.port, settings.database.port);
    QCOMPARE(reloadedSettings.database.databaseName, settings.database.databaseName);
    QCOMPARE(reloadedSettings.database.userName, settings.database.userName);
    QCOMPARE(reloadedSettings.database.password, settings.database.password);
    QCOMPARE(reloadedSettings.database.connectOptions, settings.database.connectOptions);
    QCOMPARE(reloadedSettings.reportTemplate.reportTitle, settings.reportTemplate.reportTitle);
    QCOMPARE(reloadedSettings.reportTemplate.customerName, settings.reportTemplate.customerName);
    QCOMPARE(reloadedSettings.reportTemplate.footerText, settings.reportTemplate.footerText);
    QCOMPARE(reloadedSettings.reportTemplate.logoPath, settings.reportTemplate.logoPath);
    QCOMPARE(LaserSpc::Infrastructure::PointDetailJsonService::defaultDetailDirectory(),
             QDir(settings.pointDetailDirectory).absolutePath());
    QCOMPARE(reloadedService.defaultFilter().lineName, QString("全部"));
    QCOMPARE(reloadedService.defaultFilter().result, QString("全部"));
    QVERIFY(reloadedService.defaultFilter().beginTime < reloadedService.defaultFilter().endTime);
}

void ExportAndRepositoryTests::mainWindowAppliesAutoRefreshSettingsFromConfig() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = tempDir.filePath("laserspc-auto-refresh.ini");
    const ScopedEnvVar configFileOverride("LASERSPC_CONFIG_FILE", configPath.toUtf8());
    const ScopedEnvVar useMySqlOverride("LASERSPC_USE_MYSQL", QByteArray());

    LaserSpc::Infrastructure::AppConfigService service;
    auto settings = service.settings();
    settings.useMySql = false;
    settings.autoRefreshEnabled = true;
    settings.autoRefreshIntervalSeconds = 15;

    QString errorMessage;
    QVERIFY2(service.saveSettings(settings, &errorMessage), qPrintable(errorMessage));

    auto repository = std::make_unique<LaserSpc::Infrastructure::MockSpcRepository>();
    LaserSpc::App::AppServiceFacade facade(std::move(repository), "Mock Repository");
    LaserSpc::Ui::MainWindow window(&facade);
    QCoreApplication::processEvents();

    auto* filterPanel = window.findChild<LaserSpc::Ui::FilterPanel*>();
    QVERIFY(filterPanel != nullptr);
    QTRY_VERIFY(filterPanel->isEnabled());

    QTimer* autoRefreshTimer = nullptr;
    for (auto* timer : window.findChildren<QTimer*>()) {
        if (timer->isActive() && timer->interval() == 15000) {
            autoRefreshTimer = timer;
            break;
        }
    }

    QVERIFY(autoRefreshTimer != nullptr);
}

void ExportAndRepositoryTests::mainWindowDropsStaleFilterOptionsResults() {
    auto repository = std::make_unique<DelayedFilterOptionsRepository>();
    LaserSpc::App::AppServiceFacade facade(std::move(repository), "Delayed Repository");
    LaserSpc::Ui::MainWindow window(&facade);
    window.show();

    auto* filterPanel = window.findChild<LaserSpc::Ui::FilterPanel*>();
    QVERIFY(filterPanel != nullptr);

    window.refreshFilterOptions();

    QTRY_VERIFY(filterPanel->isEnabled());

    auto* lineCombo = findComboContainingText(*filterPanel, "NEW-LINE");
    auto* programCombo = findComboContainingText(*filterPanel, "NEW-PROGRAM");
    auto* deviceCombo = findComboContainingText(*filterPanel, "NEW-DEVICE");
    QVERIFY(lineCombo != nullptr);
    QVERIFY(programCombo != nullptr);
    QVERIFY(deviceCombo != nullptr);

    QTest::qWait(250);
    QCoreApplication::processEvents();

    lineCombo = findComboContainingText(*filterPanel, "NEW-LINE");
    programCombo = findComboContainingText(*filterPanel, "NEW-PROGRAM");
    deviceCombo = findComboContainingText(*filterPanel, "NEW-DEVICE");
    QVERIFY(lineCombo != nullptr);
    QVERIFY(programCombo != nullptr);
    QVERIFY(deviceCombo != nullptr);
    QCOMPARE(lineCombo->findText("OLD-LINE"), -1);
    QCOMPARE(programCombo->findText("OLD-PROGRAM"), -1);
    QCOMPARE(deviceCombo->findText("OLD-DEVICE"), -1);
    auto* filterStateLabelLatest = findLabelContainingText(window, "筛选项已加载");
    QVERIFY(filterStateLabelLatest != nullptr);

    auto* statusLabelPaged = findLabelContainingText(window, "第 1 页");
    QVERIFY(statusLabelPaged != nullptr);
    return;

    auto* filterStateLabel = findLabelByPrefix(window, "筛选项状态:");
    QVERIFY(filterStateLabel != nullptr);
    QCOMPARE(filterStateLabel->text(), QString::fromUtf8("筛选项状态: 筛选项已加载。"));

    auto* statusLabel = findLabelByPrefix(window, "数据总览页已刷新：");
    QVERIFY(statusLabel != nullptr);
}

void ExportAndRepositoryTests::mainWindowRejectsInvalidTimeRange() {
    auto repository = std::make_unique<LaserSpc::Infrastructure::MockSpcRepository>();
    LaserSpc::App::AppServiceFacade facade(std::move(repository), "Mock Repository");
    LaserSpc::Ui::MainWindow window(&facade);
    window.show();
    QTRY_VERIFY(findLabelByPrefix(window, "数据总览页已刷新：") != nullptr);

    auto* filterPanel = window.findChild<LaserSpc::Ui::FilterPanel*>();
    QVERIFY(filterPanel != nullptr);
    QTRY_VERIFY(filterPanel->isEnabled());

    auto criteria = seedFilter();
    criteria.beginTime = QDateTime::fromString("2026-03-10 09:00:00", "yyyy-MM-dd HH:mm:ss");
    criteria.endTime = QDateTime::fromString("2026-03-10 08:00:00", "yyyy-MM-dd HH:mm:ss");
    filterPanel->setCriteria(criteria);

    auto* queryButton = findButtonByText(*filterPanel, "查询");
    QVERIFY(queryButton != nullptr);
    QTest::mouseClick(queryButton, Qt::LeftButton);

    auto* statusLabel = findLabelContainingText(window, "查询条件无效：开始时间不能晚于结束时间。");
    QTRY_VERIFY(statusLabel != nullptr);
}

void ExportAndRepositoryTests::repositoryFactoryFallsBackToMockWhenMySqlUnavailable() {
    resetMySqlConnection();
    const auto buildResult = LaserSpc::Infrastructure::RepositoryFactory::build(unavailableMySqlFallbackSettings());

    QVERIFY(buildResult.repository != nullptr);
    QCOMPARE(buildResult.dataSourceMode, QString("Mock Repository (MySQL fallback failed)"));
    QVERIFY(buildResult.warningMessage.startsWith("MySQL repository unavailable: "));
    QVERIFY(buildResult.warningMessage.contains("fallback to mock repository enabled"));

    const auto options = buildResult.repository->fetchFilterOptions();
    QCOMPARE(options.lineNames, QStringList({"L1", "L2", "L3", "L4", "L5", "L6", "L7"}));
    QCOMPARE(options.programNames,
             QStringList({"Program-A", "Program-B", "Program-C", "Program-D", "Program-E",
                          "Program-F", "Program-G", "Program-H", "Program-I", "Program-J"}));
    QCOMPARE(options.deviceNames,
             QStringList({"Laser-01", "Laser-02", "Laser-03", "Laser-04", "Laser-05",
                          "Laser-06", "Laser-07", "Laser-08", "Laser-09", "Laser-10"}));
}

void ExportAndRepositoryTests::repositoryFactoryKeepsMySqlRepositoryWhenFallbackDisabled() {
    resetMySqlConnection();
    const auto buildResult = LaserSpc::Infrastructure::RepositoryFactory::build(unavailableMySqlSettings());

    QVERIFY(buildResult.repository != nullptr);
    QCOMPARE(buildResult.dataSourceMode, QString("MySQL Repository (Unavailable)"));
    QVERIFY(buildResult.warningMessage.startsWith("MySQL repository unavailable: "));
    QVERIFY(buildResult.warningMessage.contains("strict mode"));
    QVERIFY(buildResult.repository->fetchFilterOptions().lineNames.isEmpty());
    QVERIFY(!buildResult.repository->lastError().isEmpty());
}

void ExportAndRepositoryTests::mysqlRepositoryProvidesExpectedFilterOptions() {
    LaserSpc::Infrastructure::MySqlSpcRepository repository(testDatabaseSettings());
    if (!ensureMySqlAvailable(repository)) {
        return;
    }

    const auto options = repository.fetchFilterOptions();
    QCOMPARE(options.lineNames, QStringList({"L1", "L2", "L3"}));
    QCOMPARE(options.programNames,
             QStringList({"Program-A", "Program-B", "Program-C", "Program-D", "Program-E", "Program-F"}));
    QCOMPARE(options.deviceNames,
             QStringList({"Laser-01", "Laser-02", "Laser-03", "Laser-04", "Laser-05", "Laser-06"}));
    QCOMPARE(repository.lastError(), QString());
}

void ExportAndRepositoryTests::mysqlSummaryMetricsMatchSeedData() {
    LaserSpc::Infrastructure::MySqlSpcRepository repository(testDatabaseSettings());
    if (!ensureMySqlAvailable(repository)) {
        return;
    }

    LaserSpc::Domain::SummaryQuery query;
    query.filter = seedFilter();
    query.pagination.page = 1;
    query.pagination.pageSize = 10;
    query.sort.field = "programName";
    query.sort.order = Qt::AscendingOrder;

    const auto metrics = repository.fetchSummaryMetrics(query);
    QCOMPARE(metrics.size(), 4);
    QCOMPARE(metrics.at(0).title, QString("总板数"));
    QCOMPARE(metrics.at(0).value, QString("10"));
    QCOMPARE(metrics.at(1).value, QString("6"));
    QCOMPARE(metrics.at(2).value, QString("4"));
    QCOMPARE(metrics.at(3).value, QString("60.00%"));

    const auto result = repository.fetchSummaryRows(query);
    QCOMPARE(result.page, 1);
    QCOMPARE(result.pageSize, 10);
    QCOMPARE(result.total, 6);
    QCOMPARE(result.rows.size(), 6);

    const auto& firstRow = result.rows.at(0);
    QCOMPARE(firstRow.programName, QString("Program-A"));
    QCOMPARE(firstRow.lineName, QString("L1"));
    QCOMPARE(firstRow.deviceName, QString("Laser-01"));
    QCOMPARE(firstRow.totalBoards, 2);
    QCOMPARE(firstRow.goodBoards, 2);
    QCOMPARE(firstRow.badBoards, 0);
    QVERIFY(qAbs(firstRow.yieldRate - 100.0) < 0.001);

    const auto& lastRow = result.rows.at(5);
    QCOMPARE(lastRow.programName, QString("Program-F"));
    QCOMPARE(lastRow.lineName, QString("L3"));
    QCOMPARE(lastRow.deviceName, QString("Laser-06"));
    QCOMPARE(lastRow.totalBoards, 1);
    QCOMPARE(lastRow.goodBoards, 1);
    QCOMPARE(lastRow.badBoards, 0);
    QVERIFY(qAbs(lastRow.yieldRate - 100.0) < 0.001);
}

void ExportAndRepositoryTests::mysqlBadStatisticsMatchSeedData() {
    LaserSpc::Infrastructure::MySqlSpcRepository repository(testDatabaseSettings());
    if (!ensureMySqlAvailable(repository)) {
        return;
    }

    LaserSpc::Domain::BadStatQuery query;
    query.filter = seedFilter();
    query.topN = 10;

    const auto badPoints = repository.fetchBadPointStats(query);
    QCOMPARE(badPoints.size(), 4);
    QCOMPARE(badPoints.at(0).badPointName, QString("CodeBlur"));
    QCOMPARE(badPoints.at(1).badPointName, QString("ContrastLow"));
    QCOMPARE(badPoints.at(2).badPointName, QString("MarkOffset"));
    QCOMPARE(badPoints.at(3).badPointName, QString("PrintShift"));
    for (const auto& row : badPoints) {
        QCOMPARE(row.count, 1);
        QVERIFY(qAbs(row.ratio - 25.0) < 0.001);
    }

    const auto grades = repository.fetchGradeStats(query);
    QCOMPARE(grades.size(), 4);
    QCOMPARE(grades.at(0).grade, QString("A"));
    QCOMPARE(grades.at(0).count, 4);
    QVERIFY(qAbs(grades.at(0).ratio - 40.0) < 0.001);
    QCOMPARE(grades.at(1).grade, QString("B"));
    QCOMPARE(grades.at(1).count, 2);
    QVERIFY(qAbs(grades.at(1).ratio - 20.0) < 0.001);
    QCOMPARE(grades.at(2).grade, QString("C"));
    QCOMPARE(grades.at(2).count, 2);
    QVERIFY(qAbs(grades.at(2).ratio - 20.0) < 0.001);
    QCOMPARE(grades.at(3).grade, QString("D"));
    QCOMPARE(grades.at(3).count, 2);
    QVERIFY(qAbs(grades.at(3).ratio - 20.0) < 0.001);
}

void ExportAndRepositoryTests::mysqlBadStatisticsReflectsMultipleNgPointNames() {
    resetMySqlConnection();
    LaserSpc::Infrastructure::MySqlSpcRepository repository(testDatabaseSettings());
    if (!ensureMySqlAvailable(repository)) {
        return;
    }

    auto lease = LaserSpc::Infrastructure::DatabaseConnection::acquireMySql(testDatabaseSettings());
    QVERIFY2(lease.isOpen(), qPrintable(lease.lastError()));
    QSqlDatabase db = lease.database();

    const qint64 suffix = QDateTime::currentMSecsSinceEpoch();
    const QStringList boardCodes{
        QString("UT-BADSTAT-%1-A").arg(suffix),
        QString("UT-BADSTAT-%1-B").arg(suffix),
        QString("UT-BADSTAT-%1-C").arg(suffix)
    };

    removeBoardCodes(db, boardCodes);

    const QDateTime now = QDateTime::currentDateTimeUtc();
    QString errorMessage;
    for (const QString& boardCode : boardCodes) {
        QVERIFY2(execPrepared(db,
                              "INSERT INTO board_records (board_code, result, line_name, program_name, device_name, operator_name, event_time) "
                              "VALUES (?, ?, ?, ?, ?, ?, ?)",
                              {boardCode, "NG", "L-UT", "Program-UT-BAD", "Laser-UT", "tester", now},
                              &errorMessage),
                 qPrintable(errorMessage));
    }

    QVERIFY2(execPrepared(db,
                          "INSERT INTO point_records (board_code, point_name, result, read_grade, laser_content, read_code_content, is_laser, is_read_code, line_name, program_name, device_name, start_time, end_time, detail_json_path) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                          {boardCodes.at(0), "MarkOffset", "NG", "C", "MarkOffset-LASER", "READ-UT-1", 1, 1, "L-UT", "Program-UT-BAD", "Laser-UT",
                           now.addSecs(-60), now.addSecs(-30), "ut/mark-1.json"},
                          &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(execPrepared(db,
                          "INSERT INTO point_records (board_code, point_name, result, read_grade, laser_content, read_code_content, is_laser, is_read_code, line_name, program_name, device_name, start_time, end_time, detail_json_path) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                          {boardCodes.at(1), "CodeBlur", "NG", "D", "CodeBlur-LASER", "READ-UT-2", 1, 1, "L-UT", "Program-UT-BAD", "Laser-UT",
                           now.addSecs(-50), now.addSecs(-20), "ut/blur.json"},
                          &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(execPrepared(db,
                          "INSERT INTO point_records (board_code, point_name, result, read_grade, laser_content, read_code_content, is_laser, is_read_code, line_name, program_name, device_name, start_time, end_time, detail_json_path) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                          {boardCodes.at(2), "ContrastLow", "NG", "E", "ContrastLow-LASER", "READ-UT-3", 1, 1, "L-UT", "Program-UT-BAD", "Laser-UT",
                           now.addSecs(-40), now.addSecs(-10), "ut/contrast.json"},
                          &errorMessage),
             qPrintable(errorMessage));
    QVERIFY2(execPrepared(db,
                          "INSERT INTO point_records (board_code, point_name, result, read_grade, laser_content, read_code_content, is_laser, is_read_code, line_name, program_name, device_name, start_time, end_time, detail_json_path) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                          {boardCodes.at(2), "MarkOffset", "NG", "C", "MarkOffset-LASER", "READ-UT-4", 1, 1, "L-UT", "Program-UT-BAD", "Laser-UT",
                           now.addSecs(-35), now.addSecs(-5), "ut/mark-2.json"},
                          &errorMessage),
             qPrintable(errorMessage));

    LaserSpc::Domain::BadStatQuery query;
    query.topN = 10;
    query.filter.beginTime = now.addDays(-1);
    query.filter.endTime = now.addDays(1);
    query.filter.lineName = "L-UT";
    query.filter.programName = "Program-UT-BAD";
    query.filter.deviceName = "Laser-UT";
    query.filter.result = "全部";

    const auto badPoints = repository.fetchBadPointStats(query);
    QCOMPARE(badPoints.size(), 3);
    QCOMPARE(badPoints.at(0).badPointName, QString("MarkOffset"));
    QCOMPARE(badPoints.at(0).count, 2);
    QCOMPARE(badPoints.at(1).badPointName, QString("CodeBlur"));
    QCOMPARE(badPoints.at(1).count, 1);
    QCOMPARE(badPoints.at(2).badPointName, QString("ContrastLow"));
    QCOMPARE(badPoints.at(2).count, 1);

    removeBoardCodes(db, boardCodes);
}

void ExportAndRepositoryTests::mysqlPointRepositorySupportsFilteringAndSorting() {
    LaserSpc::Infrastructure::MySqlSpcRepository repository(testDatabaseSettings());
    if (!ensureMySqlAvailable(repository)) {
        return;
    }

    LaserSpc::Domain::PointRecordQuery query;
    query.filter = seedFilter();
    query.filter.programName = "Program-B";
    query.filter.result = "NG";
    query.pagination.page = 1;
    query.pagination.pageSize = 10;
    query.sort.field = "endTime";
    query.sort.order = Qt::AscendingOrder;

    const auto result = repository.fetchPointRecords(query);
    QCOMPARE(result.page, 1);
    QCOMPARE(result.pageSize, 10);
    QCOMPARE(result.total, 2);
    QCOMPARE(result.rows.size(), 2);
    QCOMPARE(result.rows.at(0).boardCode, QString("BD-240301-0003"));
    QCOMPARE(result.rows.at(0).pointName, QString("MarkOffset"));
    QCOMPARE(result.rows.at(0).result, QString("NG"));
    QCOMPARE(result.rows.at(0).laserContent, QString("MarkOffset-LASER"));
    QCOMPARE(result.rows.at(1).boardCode, QString("BD-240301-0008"));
    QCOMPARE(result.rows.at(1).pointName, QString("ContrastLow"));
    QCOMPARE(result.rows.at(1).result, QString("NG"));
    QCOMPARE(result.rows.at(1).laserContent, QString("ContrastLow-LASER"));
}

void ExportAndRepositoryTests::summaryRepositorySupportsPagination() {
    LaserSpc::Infrastructure::MySqlSpcRepository repository(testDatabaseSettings());
    if (!ensureMySqlAvailable(repository)) {
        return;
    }

    LaserSpc::Domain::SummaryQuery query;
    query.pagination.page = 1;
    query.pagination.pageSize = 2;
    query.sort.field = "totalBoards";
    query.sort.order = Qt::DescendingOrder;
    query.filter = seedFilter();

    const auto result = repository.fetchSummaryRows(query);
    QCOMPARE(result.page, 1);
    QCOMPARE(result.pageSize, 2);
    QCOMPARE(result.total, 6);
    QCOMPARE(result.rows.size(), 2);
    QVERIFY(result.rows.first().totalBoards >= result.rows.last().totalBoards);
}

void ExportAndRepositoryTests::exportServiceCreatesSummaryCsv() {
    QList<LaserSpc::Domain::SummaryRow> rows;
    LaserSpc::Domain::SummaryRow row;
    row.lineName = QObject::tr("一线");
    row.programName = QObject::tr("程序-A");
    row.deviceName = QObject::tr("设备-01");
    row.totalBoards = 10;
    row.goodBoards = 9;
    row.badBoards = 1;
    row.yieldRate = 90.0;
    row.lastUpdated = QDateTime::fromString("2026-03-10 08:16:40", "yyyy-MM-dd HH:mm:ss");
    rows.append(row);

    QString outputPath;
    QString errorMessage;
    QVERIFY2(LaserSpc::Infrastructure::ExportService::exportSummaryRowsToCsv(rows, &outputPath, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY(QFile::exists(outputPath));
    const QString exportRoot = QDir::cleanPath(LaserSpc::Infrastructure::ExportService::defaultExportDirectory());
    QVERIFY(QDir::cleanPath(outputPath).startsWith(exportRoot));

    QFile file(outputPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray rawContents = file.readAll();
    QVERIFY(rawContents.startsWith("\xEF\xBB\xBF"));
    file.close();
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&file);
    configureUtf8(stream);
    const QString contents = stream.readAll();
    QVERIFY(contents.contains(QObject::tr("一线")));
    QVERIFY(contents.contains(QObject::tr("程序-A")));
    QVERIFY(contents.contains(QObject::tr("设备-01")));
    QVERIFY(contents.contains("90.00"));
}

void ExportAndRepositoryTests::boardRepositorySupportsSorting() {
    LaserSpc::Infrastructure::MySqlSpcRepository repository(testDatabaseSettings());
    if (!ensureMySqlAvailable(repository)) {
        return;
    }

    LaserSpc::Domain::BoardRecordQuery query;
    query.pagination.page = 1;
    query.pagination.pageSize = 3;
    query.sort.field = "boardCode";
    query.sort.order = Qt::AscendingOrder;
    query.filter = seedFilter();

    const auto result = repository.fetchBoardRecords(query);
    QCOMPARE(result.rows.size(), 3);
    QVERIFY(result.rows.at(0).boardCode <= result.rows.at(1).boardCode);
    QVERIFY(result.rows.at(1).boardCode <= result.rows.at(2).boardCode);
}

void ExportAndRepositoryTests::exportServiceCreatesPointCsv() {
    QList<LaserSpc::Domain::PointRecordRow> rows;
    LaserSpc::Domain::PointRecordRow row;
    row.boardCode = "BD-240301-0003";
    row.pointName = "MarkOffset";
    row.result = "NG";
    row.readGrade = "C";
    row.laserContent = "MarkOffset-LASER";
    row.lineName = "L1";
    row.programName = "Program-B";
    row.startTime = QDateTime::fromString("2026-03-10 08:17:50", "yyyy-MM-dd HH:mm:ss");
    row.endTime = QDateTime::fromString("2026-03-10 08:18:40", "yyyy-MM-dd HH:mm:ss");
    row.deviceName = "Laser-02";
    rows.append(row);

    QString outputPath;
    QString errorMessage;
    QVERIFY2(LaserSpc::Infrastructure::ExportService::exportPointRecordsToCsv(rows, &outputPath, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY(QFile::exists(outputPath));
    const QString exportRoot = QDir::cleanPath(LaserSpc::Infrastructure::ExportService::defaultExportDirectory());
    QVERIFY(QDir::cleanPath(outputPath).startsWith(exportRoot));

    QFile file(outputPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray rawContents = file.readAll();
    QVERIFY(rawContents.startsWith("\xEF\xBB\xBF"));
    file.close();
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream stream(&file);
    configureUtf8(stream);
    const QString contents = stream.readAll();
    QVERIFY(contents.contains("MarkOffset"));
    QVERIFY(contents.contains("MarkOffset-LASER"));
    QVERIFY(contents.contains("Program-B"));
    QVERIFY(contents.contains("Laser-02"));
}

void ExportAndRepositoryTests::exportServiceReturnsRecentExportFiles() {
    const auto files = LaserSpc::Infrastructure::ExportService::recentExportFiles(5);
    QVERIFY(!files.isEmpty());
    QVERIFY(files.first().exists());
    const QString exportRoot = QDir::cleanPath(LaserSpc::Infrastructure::ExportService::defaultExportDirectory());
    QVERIFY(QDir::cleanPath(files.first().absoluteFilePath()).startsWith(exportRoot));
}

void ExportAndRepositoryTests::exportServiceCreatesReportBundle() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const ScopedEnvVar exportDirOverride("LASERSPC_EXPORT_DIR", tempDir.path().toUtf8());

    const QString directory = LaserSpc::Infrastructure::ExportService::defaultExportDirectory();
    const QString firstPath = QDir(directory).filePath("bundle_summary.csv");
    const QString secondPath = QDir(directory).filePath("bundle_chart.png");

    QFile first(firstPath);
    QVERIFY(first.open(QIODevice::WriteOnly | QIODevice::Truncate));
    first.write("summary");
    first.close();

    QFile second(secondPath);
    QVERIFY(second.open(QIODevice::WriteOnly | QIODevice::Truncate));
    second.write("chart");
    second.close();

    LaserSpc::Infrastructure::ExportReportBundleOptions options;
    options.reportName = "delivery_report";
    options.reportType = "summary";
    options.currentTaskState = "当前任务：已完成";
    options.selectedFiles = QStringList{firstPath, secondPath};
    options.criteriaSummary = QStringList{QObject::tr("时间范围：2026-03-10 08:00:00 至 2026-03-10 09:00:00")};
    options.metricSummary = QStringList{QObject::tr("总板数：10"), QObject::tr("良率：90.00%")};
    options.notes = QStringList{QObject::tr("来源：tests"), QObject::tr("类型：delivery")};

    QString outputPath;
    QString errorMessage;
    QVERIFY2(LaserSpc::Infrastructure::ExportService::createReportBundle(options, &outputPath, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY(QDir(outputPath).exists());
    QVERIFY(QFile::exists(QDir(outputPath).filePath("bundle_summary.csv")));
    QVERIFY(QFile::exists(QDir(outputPath).filePath("bundle_chart.png")));
    QVERIFY(QFile::exists(QDir(outputPath).filePath("report_manifest.txt")));
    QVERIFY(QFile::exists(QDir(outputPath).filePath("report_overview.html")));
    QVERIFY(QFile::exists(QDir(outputPath).filePath("report_overview.pdf")));
    QVERIFY(QFile::exists(QDir(outputPath).filePath("report_overview.xls")));

    QFile htmlFile(QDir(outputPath).filePath("report_overview.html"));
    QVERIFY(htmlFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream htmlStream(&htmlFile);
    configureUtf8(htmlStream);
    const QString htmlContent = htmlStream.readAll();
    QVERIFY(htmlContent.contains(QObject::tr("数据总览报告")));
    QVERIFY(htmlContent.contains(QObject::tr("当前任务：已完成")));
    QVERIFY(htmlContent.contains(QObject::tr("时间范围：2026-03-10 08:00:00 至 2026-03-10 09:00:00")));
}

void ExportAndRepositoryTests::exportServiceRemovesExportFiles() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const ScopedEnvVar exportDirOverride("LASERSPC_EXPORT_DIR", tempDir.path().toUtf8());

    const QString directory = LaserSpc::Infrastructure::ExportService::defaultExportDirectory();
    const QString firstPath = QDir(directory).filePath("laser_spc_test_remove_a.csv");
    const QString secondPath = QDir(directory).filePath("laser_spc_test_remove_b.csv");

    QFile first(firstPath);
    QVERIFY(first.open(QIODevice::WriteOnly | QIODevice::Truncate));
    first.write("a");
    first.close();

    QFile second(secondPath);
    QVERIFY(second.open(QIODevice::WriteOnly | QIODevice::Truncate));
    second.write("b");
    second.close();

    QString errorMessage;
    QVERIFY2(LaserSpc::Infrastructure::ExportService::removeExportFiles({firstPath, secondPath}, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY(!QFile::exists(firstPath));
    QVERIFY(!QFile::exists(secondPath));
}

void ExportAndRepositoryTests::runtimeDiagnosticsCollectsSnapshot() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const auto snapshot = LaserSpc::Infrastructure::RuntimeDiagnostics::collectSnapshot(
        tempDir.filePath("laserspc.ini"),
        tempDir.path(),
        tempDir.path(),
        tempDir.path(),
        {"QMYSQL", "QSQLITE"});

    QCOMPARE(snapshot.configFilePath, tempDir.filePath("laserspc.ini"));
    QCOMPARE(snapshot.exportDirectory, tempDir.path());
    QCOMPARE(snapshot.qtFontDirectory, tempDir.path());
    QCOMPARE(snapshot.qtPluginDirectory, tempDir.path());
    QVERIFY(snapshot.sqlDrivers.contains("QMYSQL"));
    QVERIFY(snapshot.warnings.isEmpty());
}

void ExportAndRepositoryTests::mysqlEnvironmentSmokeTest() {
    resetMySqlConnection();
    LaserSpc::Infrastructure::MySqlSpcRepository repository(testDatabaseSettings());
    if (!ensureMySqlAvailable(repository)) {
        return;
    }

    auto dbLease = LaserSpc::Infrastructure::DatabaseConnection::acquireMySql(testDatabaseSettings());
    QVERIFY2(dbLease.isOpen(), qPrintable(dbLease.lastError()));
    const auto db = dbLease.database();

    QSqlQuery boardCountQuery(db);
    QVERIFY2(boardCountQuery.exec("SELECT COUNT(*) FROM board_records"), qPrintable(boardCountQuery.lastError().text()));
    QVERIFY(boardCountQuery.next());
    QCOMPARE(boardCountQuery.value(0).toInt(), 10);

    QSqlQuery pointCountQuery(db);
    QVERIFY2(pointCountQuery.exec("SELECT COUNT(*) FROM point_records"), qPrintable(pointCountQuery.lastError().text()));
    QVERIFY(pointCountQuery.next());
    QCOMPARE(pointCountQuery.value(0).toInt(), 10);

    QSqlQuery tableExistenceQuery(db);
    QVERIFY2(tableExistenceQuery.exec("SHOW TABLES LIKE 'board_records'"), qPrintable(tableExistenceQuery.lastError().text()));
    QVERIFY(tableExistenceQuery.next());
}

void ExportAndRepositoryTests::mysqlConnectionUsesUtcSession() {
    resetMySqlConnection();
    LaserSpc::Infrastructure::MySqlSpcRepository repository(testDatabaseSettings());
    if (!ensureMySqlAvailable(repository)) {
        return;
    }

    auto dbLease = LaserSpc::Infrastructure::DatabaseConnection::acquireMySql(testDatabaseSettings());
    QVERIFY2(dbLease.isOpen(), qPrintable(dbLease.lastError()));
    const auto db = dbLease.database();

    QSqlQuery query(db);
    QVERIFY2(query.exec("SELECT @@session.time_zone"), qPrintable(query.lastError().text()));
    QVERIFY(query.next());
    QVERIFY(!query.value(0).toString().trimmed().isEmpty());
}

void ExportAndRepositoryTests::mysqlWriteRepositoryUpsertsPointRecordIdempotently() {
    resetMySqlConnection();
    LaserSpc::Infrastructure::MySqlSpcRepository availabilityRepository(testDatabaseSettings());
    if (!ensureMySqlAvailable(availabilityRepository)) {
        return;
    }

    auto lease = LaserSpc::Infrastructure::DatabaseConnection::acquireMySql(testDatabaseSettings());
    QVERIFY2(lease.isOpen(), qPrintable(lease.lastError()));
    QSqlDatabase db = lease.database();

    const QString sqlDir = resolveSqlDirForTests();
    QVERIFY(!sqlDir.isEmpty());
    QString migrationError;
    QVERIFY2(executeSqlFileForTests(db, QDir(sqlDir).filePath("migrations/V005__make_point_records_idempotent.sql"),
                                    &migrationError),
             qPrintable(migrationError));

    LaserSpc::Infrastructure::MySqlSpcWriteRepository repository(testDatabaseSettings());

    const QString boardCode =
        QString("UT-POINT-UPSERT-%1").arg(QDateTime::currentMSecsSinceEpoch());
    const QString pointName = QString("P01");

    LaserSpc::Domain::BoardRecordRow board;
    board.boardCode = boardCode;
    board.result = "OK";
    board.lineName = "L1";
    board.programName = "Program-Upsert";
    board.deviceName = "Laser-UT";
    board.operatorName = "Tester";
    board.eventTime = QDateTime::currentDateTime();

    QVERIFY2(repository.upsertBoardRecord(board), qPrintable(repository.lastError()));

    LaserSpc::Domain::PointRecordRow point;
    point.boardCode = boardCode;
    point.pointName = pointName;
    point.result = "OK";
    point.readGrade = "A";
    point.laserContent = "UPSERT-LASER";
    point.readCodeContent = "READ-OLD";
    point.lineName = board.lineName;
    point.programName = board.programName;
    point.deviceName = board.deviceName;
    point.startTime = board.eventTime.addSecs(-5);
    point.endTime = board.eventTime;
    point.detailJsonPath = "point_details/old.json";

    QVERIFY2(repository.insertPointRecord(point), qPrintable(repository.lastError()));

    point.result = "NG";
    point.readGrade = "C";
    point.readCodeContent = "READ-NEW";
    point.endTime = board.eventTime.addSecs(3);
    point.detailJsonPath = "point_details/new.json";

    QVERIFY2(repository.insertPointRecord(point), qPrintable(repository.lastError()));

    QCOMPARE(scalarInt(db,
                       "SELECT COUNT(*) FROM point_records WHERE board_code = ? AND point_name = ?",
                       {boardCode, pointName}),
             1);

    QSqlQuery query(db);
    query.prepare(
        "SELECT result, read_grade, laser_content, read_code_content, detail_json_path "
        "FROM point_records WHERE board_code = ? AND point_name = ?");
    query.addBindValue(boardCode);
    query.addBindValue(pointName);
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("NG"));
    QCOMPARE(query.value(1).toString(), QString("C"));
    QCOMPARE(query.value(2).toString(), QString("UPSERT-LASER"));
    QCOMPARE(query.value(3).toString(), QString("READ-NEW"));
    QCOMPARE(query.value(4).toString(), QString("point_details/new.json"));

    removeBoardCodes(db, {boardCode});
}

void ExportAndRepositoryTests::mysqlWriteRepositoryReplacesInspectionBatchWithMultiplePoints() {
    resetMySqlConnection();
    LaserSpc::Infrastructure::MySqlSpcRepository availabilityRepository(testDatabaseSettings());
    if (!ensureMySqlAvailable(availabilityRepository)) {
        return;
    }

    auto lease = LaserSpc::Infrastructure::DatabaseConnection::acquireMySql(testDatabaseSettings());
    QVERIFY2(lease.isOpen(), qPrintable(lease.lastError()));
    QSqlDatabase db = lease.database();

    LaserSpc::Infrastructure::MySqlSpcWriteRepository repository(testDatabaseSettings());
    const QString boardCode = QString("UT-BATCH-REPLACE-%1").arg(QDateTime::currentMSecsSinceEpoch());

    LaserSpc::Domain::BoardRecordRow board;
    board.boardCode = boardCode;
    board.result = "OK";
    board.lineName = "L1";
    board.programName = "Program-Batch";
    board.deviceName = "Laser-UT";
    board.operatorName = "Tester";
    board.eventTime = QDateTime::currentDateTimeUtc();

    QList<LaserSpc::Domain::PointRecordRow> firstPoints;
    for (int index = 0; index < 3; ++index) {
        LaserSpc::Domain::PointRecordRow point;
        point.boardCode = boardCode;
        point.pointName = QString("P%1").arg(index + 1, 2, 10, QChar('0'));
        point.result = index == 1 ? "NG" : "OK";
        point.readGrade = QString("G%1").arg(index + 1);
        point.readCodeContent = QString("READ-%1").arg(index + 1);
        point.lineName = board.lineName;
        point.programName = board.programName;
        point.deviceName = board.deviceName;
        point.startTime = board.eventTime.addSecs(-index - 1);
        point.endTime = board.eventTime.addSecs(index);
        point.detailJsonPath = QString("point_details/%1.json").arg(point.pointName);
        firstPoints.append(point);
    }

    QVERIFY2(repository.replaceInspectionBatch(board, firstPoints), qPrintable(repository.lastError()));
    QCOMPARE(scalarInt(db, "SELECT COUNT(*) FROM point_records WHERE board_code = ?", {boardCode}), 3);

    QList<LaserSpc::Domain::PointRecordRow> secondPoints = firstPoints.mid(0, 2);
    secondPoints[0].result = "NG";
    secondPoints[0].readCodeContent = "READ-UPDATED";
    QVERIFY2(repository.replaceInspectionBatch(board, secondPoints), qPrintable(repository.lastError()));

    QCOMPARE(scalarInt(db, "SELECT COUNT(*) FROM point_records WHERE board_code = ?", {boardCode}), 2);
    QCOMPARE(scalarInt(db,
                       "SELECT COUNT(*) FROM point_records WHERE board_code = ? AND point_name = ?",
                       {boardCode, "P03"}),
             0);

    QSqlQuery verifyQuery(db);
    verifyQuery.prepare("SELECT result, read_code_content FROM point_records WHERE board_code = ? AND point_name = ?");
    verifyQuery.addBindValue(boardCode);
    verifyQuery.addBindValue("P01");
    QVERIFY2(verifyQuery.exec(), qPrintable(verifyQuery.lastError().text()));
    QVERIFY(verifyQuery.next());
    QCOMPARE(verifyQuery.value(0).toString(), QString("NG"));
    QCOMPARE(verifyQuery.value(1).toString(), QString("READ-UPDATED"));

    removeBoardCodes(db, {boardCode});
}

void ExportAndRepositoryTests::spcWriteManagerSyncStoreBatchWritesDetailJson() {
    resetMySqlConnection();
    LaserSpc::Infrastructure::MySqlSpcRepository availabilityRepository(testDatabaseSettings());
    if (!ensureMySqlAvailable(availabilityRepository)) {
        return;
    }

    auto lease = LaserSpc::Infrastructure::DatabaseConnection::acquireMySql(testDatabaseSettings());
    QVERIFY2(lease.isOpen(), qPrintable(lease.lastError()));
    QSqlDatabase db = lease.database();

    const QString boardCode = QString("UT-WRITER-SYNC-%1").arg(QDateTime::currentMSecsSinceEpoch());
    removeBoardCodes(db, {boardCode});

    LaserSpc::Infrastructure::AppSettings settings;
    settings.useMySql = true;
    settings.database = testDatabaseSettings();
    HostSpc::SpcWriteManager writer(settings);

    LaserSpc::Domain::BoardRecordRow board;
    board.boardCode = boardCode;
    board.result = "NG";
    board.lineName = "L-UT";
    board.programName = "Program-UT-Writer";
    board.deviceName = "Laser-UT";
    board.operatorName = "Tester";
    board.eventTime = QDateTime::currentDateTimeUtc();

    LaserSpc::Domain::PointRecordRow point;
    point.boardCode = boardCode;
    point.pointName = "MarkOffset";
    point.result = "NG";
    point.readGrade = "C";
    point.readCodeContent = "READ-SYNC-001";
    point.isLaser = true;
    point.isReadCode = true;
    point.lineName = board.lineName;
    point.programName = board.programName;
    point.deviceName = board.deviceName;
    point.startTime = board.eventTime.addSecs(-2);
    point.endTime = board.eventTime;
    point.detailJsonPath.clear();

    const auto batch = HostSpc::SpcWriteManager::buildBatch(QString(), board, {point});
    const auto result = writer.storeBatch(batch);
    QVERIFY2(result.success, qPrintable(result.errorMessage));

    QSqlQuery query(db);
    query.prepare("SELECT detail_json_path FROM point_records WHERE board_code = ? AND point_name = ?");
    query.addBindValue(boardCode);
    query.addBindValue("MarkOffset");
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
    QVERIFY(query.next());
    const QString detailPath = query.value(0).toString();
    QVERIFY2(!detailPath.trimmed().isEmpty(), "detail_json_path should not be empty");
    QVERIFY2(QFileInfo(detailPath).exists(), qPrintable(detailPath));

    removeBoardCodes(db, {boardCode});
}

void ExportAndRepositoryTests::spcWriteManagerAsyncStoreBatchWritesDetailJson() {
    resetMySqlConnection();
    LaserSpc::Infrastructure::MySqlSpcRepository availabilityRepository(testDatabaseSettings());
    if (!ensureMySqlAvailable(availabilityRepository)) {
        return;
    }

    auto lease = LaserSpc::Infrastructure::DatabaseConnection::acquireMySql(testDatabaseSettings());
    QVERIFY2(lease.isOpen(), qPrintable(lease.lastError()));
    QSqlDatabase db = lease.database();

    const QString boardCode = QString("UT-WRITER-ASYNC-%1").arg(QDateTime::currentMSecsSinceEpoch());
    removeBoardCodes(db, {boardCode});

    LaserSpc::Infrastructure::AppSettings settings;
    settings.useMySql = true;
    settings.database = testDatabaseSettings();
    HostSpc::SpcWriteManager writer(settings);
    writer.startAsyncWriter();

    LaserSpc::Domain::BoardRecordRow board;
    board.boardCode = boardCode;
    board.result = "NG";
    board.lineName = "L-UT";
    board.programName = "Program-UT-Writer";
    board.deviceName = "Laser-UT";
    board.operatorName = "Tester";
    board.eventTime = QDateTime::currentDateTimeUtc();

    LaserSpc::Domain::PointRecordRow point;
    point.boardCode = boardCode;
    point.pointName = "CodeBlur";
    point.result = "NG";
    point.readGrade = "D";
    point.readCodeContent = "READ-ASYNC-001";
    point.isLaser = true;
    point.isReadCode = true;
    point.lineName = board.lineName;
    point.programName = board.programName;
    point.deviceName = board.deviceName;
    point.startTime = board.eventTime.addSecs(-2);
    point.endTime = board.eventTime;
    point.detailJsonPath.clear();

    QSignalSpy batchSpy(&writer, &HostSpc::SpcWriteManager::batchStored);
    writer.enqueueBatch(HostSpc::SpcWriteManager::buildBatch(QString(), board, {point}));
    QVERIFY2(batchSpy.wait(5000), "Timed out waiting for async batchStored signal");
    QVERIFY(batchSpy.count() >= 1);

    const auto arguments = batchSpy.takeLast();
    const auto asyncResult = qvariant_cast<LaserSpc::Domain::IngestResult>(arguments.at(1));
    QVERIFY2(asyncResult.success, qPrintable(asyncResult.errorMessage));

    QSqlQuery query(db);
    query.prepare("SELECT detail_json_path FROM point_records WHERE board_code = ? AND point_name = ?");
    query.addBindValue(boardCode);
    query.addBindValue("CodeBlur");
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
    QVERIFY(query.next());
    const QString detailPath = query.value(0).toString();
    QVERIFY2(!detailPath.trimmed().isEmpty(), "detail_json_path should not be empty");
    QVERIFY2(QFileInfo(detailPath).exists(), qPrintable(detailPath));

    removeBoardCodes(db, {boardCode});
}

void ExportAndRepositoryTests::spcWriteManagerAsyncPointWriteUsesEmbeddedDefaults() {
    resetMySqlConnection();
    LaserSpc::Infrastructure::MySqlSpcRepository availabilityRepository(testDatabaseSettings());
    if (!ensureMySqlAvailable(availabilityRepository)) {
        return;
    }

    auto lease = LaserSpc::Infrastructure::DatabaseConnection::acquireMySql(testDatabaseSettings());
    QVERIFY2(lease.isOpen(), qPrintable(lease.lastError()));
    QSqlDatabase db = lease.database();

    const QString boardCode = QString("UT-WRITER-POINT-%1").arg(QDateTime::currentMSecsSinceEpoch());
    removeBoardCodes(db, {boardCode});

    LaserSpc::Infrastructure::AppSettings settings;
    settings.useMySql = true;
    settings.database = testDatabaseSettings();
    HostSpc::SpcWriteManager writer(settings);
    writer.startAsyncWriter();

    LaserSpc::Domain::BoardRecordRow board;
    board.boardCode = boardCode;
    board.result = "OK";
    board.lineName = "L-UT";
    board.programName = "Program-UT-Writer";
    board.deviceName = "Laser-UT";
    board.operatorName = "Tester";
    board.eventTime = QDateTime::currentDateTimeUtc();

    const auto boardResult = writer.storeBoard(board);
    QVERIFY2(boardResult.success, qPrintable(boardResult.errorMessage));

    LaserSpc::Domain::PointRecordRow point;
    point.boardCode = boardCode;
    point.pointName = "DirectPoint";
    point.result = "OK";
    point.lineName = board.lineName;
    point.programName = board.programName;
    point.deviceName = board.deviceName;
    point.startTime = board.eventTime.addSecs(-1);
    point.endTime = board.eventTime;

    QSignalSpy pointSpy(&writer, &HostSpc::SpcWriteManager::pointStored);
    writer.enqueuePoint(point);
    QVERIFY2(pointSpy.wait(5000), "Timed out waiting for async pointStored signal");
    QVERIFY(pointSpy.count() >= 1);

    const auto arguments = pointSpy.takeLast();
    const auto asyncResult = qvariant_cast<LaserSpc::Domain::IngestResult>(arguments.at(2));
    QVERIFY2(asyncResult.success, qPrintable(asyncResult.errorMessage));

    QSqlQuery query(db);
    query.prepare(
        "SELECT read_grade, laser_content, read_code_content, is_laser, is_read_code, detail_json_path "
        "FROM point_records WHERE board_code = ? AND point_name = ?");
    query.addBindValue(boardCode);
    query.addBindValue("DirectPoint");
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QString("null"));
    QCOMPARE(query.value(1).toString(), QString("DirectPoint-LASER"));
    QCOMPARE(query.value(2).toString(), QString("null"));
    QCOMPARE(query.value(3).toBool(), false);
    QCOMPARE(query.value(4).toBool(), false);
    QVERIFY2(!query.value(5).toString().trimmed().isEmpty(), "detail_json_path should not be empty");

    removeBoardCodes(db, {boardCode});
}

void ExportAndRepositoryTests::hostWriterBuildBatchGeneratesUniqueRequestIds() {
    LaserSpc::Domain::BoardRecordRow board;
    board.boardCode = "HOST-REQ-001";
    board.result = "OK";
    board.lineName = "L1";
    board.programName = "Program-A";
    board.deviceName = "Laser-01";
    board.operatorName = "Tester";
    board.eventTime = QDateTime::currentDateTime();

    LaserSpc::Domain::PointRecordRow point;
    point.boardCode = board.boardCode;
    point.pointName = "P01";
    point.result = "OK";
    point.readGrade = "A";
    point.readCodeContent = "READ-001";
    point.lineName = board.lineName;
    point.programName = board.programName;
    point.deviceName = board.deviceName;
    point.startTime = board.eventTime;
    point.endTime = board.eventTime;

    const auto batch1 = HostSpc::SpcWriteManager::buildBatch(QString(), board, {point});
    const auto batch2 = HostSpc::SpcWriteManager::buildBatch(QString(), board, {point});

    QVERIFY(!batch1.requestId.trimmed().isEmpty());
    QVERIFY(!batch2.requestId.trimmed().isEmpty());
    QVERIFY(batch1.requestId != batch2.requestId);
}

void ExportAndRepositoryTests::ingestServiceNormalizesDefaultTimesToUtc() {
    auto repository = std::make_shared<CapturingWriteRepository>();
    LaserSpc::App::IngestService service(repository);

    LaserSpc::Domain::BoardRecordRow board;
    board.boardCode = "UTC-BOARD-001";
    board.result = "OK";
    board.lineName = "L1";
    board.programName = "Program-UTC";
    board.deviceName = "Laser-01";
    board.operatorName = "Tester";

    const auto boardResult = service.ingestBoardRecord(board);
    QVERIFY2(boardResult.success, qPrintable(boardResult.errorMessage));
    QCOMPARE(repository->boardCallCount, 1);
    QVERIFY(repository->lastBoard.eventTime.isValid());
    QCOMPARE(repository->lastBoard.eventTime.timeSpec(), Qt::LocalTime);

    LaserSpc::Domain::PointRecordRow point;
    point.boardCode = board.boardCode;
    point.pointName = "P01";
    point.result = "OK";
    point.lineName = board.lineName;
    point.programName = board.programName;
    point.deviceName = board.deviceName;

    const auto pointResult = service.ingestPointRecord(point);
    QVERIFY2(pointResult.success, qPrintable(pointResult.errorMessage));
    QCOMPARE(repository->pointCallCount, 1);
    QVERIFY(repository->lastPoint.startTime.isValid());
    QVERIFY(repository->lastPoint.endTime.isValid());
    QCOMPARE(repository->lastPoint.startTime.timeSpec(), Qt::LocalTime);
    QCOMPARE(repository->lastPoint.endTime.timeSpec(), Qt::LocalTime);
    QCOMPARE(repository->lastPoint.readGrade, QString("null"));
    QCOMPARE(repository->lastPoint.readCodeContent, QString("null"));
    QCOMPARE(repository->lastPoint.isLaser, false);
    QCOMPARE(repository->lastPoint.isReadCode, false);
}

void ExportAndRepositoryTests::hostWriterAggregatorDefersEmptyBoards() {
    LaserSpc::Infrastructure::AppSettings settings;
    HostSpc::SpcWriteManager writer(settings);
    HostSpc::BoardBatchAggregator aggregator(&writer);

    QSignalSpy deferredSpy(&aggregator, &HostSpc::BoardBatchAggregator::batchDeferred);
    QSignalSpy flushedSpy(&aggregator, &HostSpc::BoardBatchAggregator::batchFlushed);

    LaserSpc::Domain::BoardRecordRow board;
    board.boardCode = "HOST-EMPTY-001";
    board.result = "OK";
    board.lineName = "L1";
    board.programName = "Program-A";
    board.deviceName = "Laser-01";
    board.operatorName = "Tester";
    board.eventTime = QDateTime::currentDateTime();

    QVERIFY(aggregator.upsertBoard(board, "req-empty", 0));
    QCOMPARE(flushedSpy.count(), 0);
    QCOMPARE(aggregator.pendingBoardCount(), 1);

    const int flushed = aggregator.flushAll();
    QCOMPARE(flushed, 0);
    QCOMPARE(flushedSpy.count(), 0);
    QCOMPARE(deferredSpy.count(), 1);
    QCOMPARE(deferredSpy.at(0).at(0).toString(), QString("HOST-EMPTY-001"));
    QVERIFY(deferredSpy.at(0).at(1).toString().contains("empty inspection batch"));
    QCOMPARE(aggregator.pendingBoardCount(), 1);
}

void ExportAndRepositoryTests::hostWriterAggregatorPreservesDuplicateRequestIdOrder() {
    LaserSpc::Infrastructure::AppSettings settings;
    HostSpc::SpcWriteManager writer(settings);
    HostSpc::BoardBatchAggregator aggregator(&writer);

    QSignalSpy flushedSpy(&aggregator, &HostSpc::BoardBatchAggregator::batchFlushed);

    aggregator.m_inflightBatchesByRequestId["dup-req"].enqueue({"BOARD-A", "dup-req"});
    aggregator.m_inflightBatchesByRequestId["dup-req"].enqueue({"BOARD-B", "dup-req"});

    LaserSpc::Domain::IngestResult result;
    result.success = true;

    QVERIFY(QMetaObject::invokeMethod(&writer, "batchStored", Qt::DirectConnection,
                                      Q_ARG(QString, "dup-req"),
                                      Q_ARG(LaserSpc::Domain::IngestResult, result)));
    QVERIFY(QMetaObject::invokeMethod(&writer, "batchStored", Qt::DirectConnection,
                                      Q_ARG(QString, "dup-req"),
                                      Q_ARG(LaserSpc::Domain::IngestResult, result)));

    QCOMPARE(flushedSpy.count(), 2);
    QCOMPARE(flushedSpy.at(0).at(0).toString(), QString("BOARD-A"));
    QCOMPARE(flushedSpy.at(0).at(1).toString(), QString("dup-req"));
    QCOMPARE(flushedSpy.at(1).at(0).toString(), QString("BOARD-B"));
    QCOMPARE(flushedSpy.at(1).at(1).toString(), QString("dup-req"));
    QVERIFY(aggregator.m_inflightBatchesByRequestId.isEmpty());
}

void ExportAndRepositoryTests::hostWriterExamplePointRowWritesDetailJson() {
    LaserSpc::Domain::PointRecordRow row;
    row.boardCode = "HOST-DETAIL-001";
    row.pointName = "P01";
    row.result = "OK";
    row.readGrade = "A";
    row.laserContent = "P01-LASER";
    row.readCodeContent = "HOST-DETAIL-001-P01";
    row.lineName = "L1";
    row.programName = "Program-A";
    row.deviceName = "Laser-01";
    row.startTime = QDateTime::currentDateTime().addSecs(-1);
    row.endTime = QDateTime::currentDateTime();
    row.detail.extraFields.insert("templateVersion", "v2");
    row.detail.extraFields.insert("processOwner", "qa-host");
    row.detail.fieldDisplayNames.insert("templateVersion", "模板版本");
    row.detail.fieldDisplayNames.insert("processOwner", "责任人");
    QJsonObject node1;
    node1.insert("name", "Locate");
    node1.insert("ok", "Decode");
    node1.insert("ng", "Stop");
    QJsonObject node2;
    node2.insert("name", "Decode");
    node2.insert("ok", "Judge");
    node2.insert("ng", "Stop");
    row.detail.algorithmPlan = QJsonArray{node1, node2};

    QString errorMessage;
    QVERIFY2(HostSpc::HostSpcExampleWindow::ensurePointDetailFile(&row, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY(!row.detailJsonPath.trimmed().isEmpty());

    const QFileInfo fileInfo(row.detailJsonPath);
    QVERIFY2(fileInfo.exists(), qPrintable(row.detailJsonPath));

    LaserSpc::Domain::PointDetailInfo detail;
    QString loadError;
    QVERIFY2(LaserSpc::Infrastructure::PointDetailJsonService::loadDetail(row.detailJsonPath, &detail, &loadError),
             qPrintable(loadError));
    QCOMPARE(detail.readCodeContent, row.readCodeContent);
    QCOMPARE(detail.programName, row.programName);
    QCOMPARE(detail.success, true);
    QCOMPARE(detail.extraFields.value("templateVersion").toString(), QString("v2"));
    QCOMPARE(detail.extraFields.value("processOwner").toString(), QString("qa-host"));
    QCOMPARE(detail.fieldDisplayNames.value("templateVersion").toString(), QString("模板版本"));
    QCOMPARE(detail.fieldDisplayNames.value("processOwner").toString(), QString("责任人"));
    QCOMPARE(detail.algorithmPlan.size(), 2);
    QCOMPARE(detail.algorithmPlan.at(0).toObject().value("name").toString(), QString("Locate"));
}

void ExportAndRepositoryTests::pointDetailDialogShowsChineseFieldLabels() {
    LaserSpc::Ui::PointDetailDialog dialog;
    dialog.setDetailFilePath("D:/Program/spc/LaserSpc/point_details/demo.json");

    QJsonObject dataObject;
    dataObject.insert("templateFilePath", "templates/Program-A.tpl");
    dataObject.insert("laserTemplatePath", "legacy/Program-A.tpl");
    dataObject.insert("laserContent", "P01-LASER");
    dataObject.insert("readCodeContent", "READ-001");
    dataObject.insert("success", true);
    dataObject.insert("programName", "Program-A");
    dataObject.insert("startTime", "2026-04-12T10:00:00");
    dataObject.insert("endTime", "2026-04-12T10:00:02");
    dataObject.insert("templateVersion", "v3");
    dataObject.insert(LaserSpc::Infrastructure::PointDetailJsonService::algorithmPlanKey(),
                      QJsonArray{
                          QJsonObject{{"name", "定位"}, {"ok", "解码"}, {"ng", "停止"}},
                          QJsonObject{{"name", "解码"}, {"ok", "判定"}, {"ng", "停止"}}
                      });

    QJsonObject rootObject;
    rootObject.insert("data", dataObject);
    rootObject.insert("version", 2);
    dialog.setDetailObject(rootObject);

    auto* treeWidget = dialog.findChild<QTreeWidget*>();
    QVERIFY(treeWidget != nullptr);
    QCOMPARE(treeWidget->topLevelItemCount(), 2);
    QCOMPARE(treeWidget->topLevelItem(0)->text(0), QString("详情数据"));
    QCOMPARE(treeWidget->topLevelItem(1)->text(0), QString("版本"));

    auto* dataItem = treeWidget->topLevelItem(0);
    QVERIFY(dataItem != nullptr);
    bool foundProgramLabel = false;
    bool foundSuccessLabel = false;
    for (int index = 0; index < dataItem->childCount(); ++index) {
        const auto* child = dataItem->child(index);
        if (child->text(0) == QString("程序名") && child->text(1) == QString("Program-A")) {
            foundProgramLabel = true;
        }
        if (child->text(0) == QString("识别成功") && child->text(1) == QString("是")) {
            foundSuccessLabel = true;
        }
    }
    QVERIFY(foundProgramLabel);
    QVERIFY(foundSuccessLabel);
    QVERIFY(findLabelContainingText(dialog, "2 项：定位 -> 解码") != nullptr);
    QVERIFY(findLabelContainingText(dialog, "算法规划") != nullptr);

    QVERIFY(findLabelContainingText(dialog, "templates/Program-A.tpl") != nullptr);
    QVERIFY(findLabelContainingText(dialog, "Program-A") != nullptr);
    QVERIFY(findLabelContainingText(dialog, "详情文件：D:/Program/spc/LaserSpc/point_details/demo.json") != nullptr);
}

void ExportAndRepositoryTests::hostWriterExampleWindowProvidesAlgorithmPlanDebugButton() {
    HostSpc::HostSpcExampleWindow window;
    auto* button = findButtonByText(window, "Write Batch With Algorithm Plan");
    QVERIFY(button != nullptr);
}

void ExportAndRepositoryTests::pointDetailDialogPlacesStartAndEndTimeAdjacent() {
    LaserSpc::Ui::PointDetailDialog dialog;

    QJsonObject dataObject;
    dataObject.insert("templateFilePath", "templates/Program-A.tpl");
    dataObject.insert("programName", "Program-A");
    dataObject.insert("cameraProfile", "LineScan-1");
    dataObject.insert("startTime", "2026-04-12T10:00:00");
    dataObject.insert("endTime", "2026-04-12T10:00:02");
    dataObject.insert("success", true);

    QJsonObject rootObject;
    rootObject.insert("data", dataObject);
    rootObject.insert("version", 2);
    dialog.setDetailObject(rootObject);

    auto* treeWidget = dialog.findChild<QTreeWidget*>();
    QVERIFY(treeWidget != nullptr);
    auto* dataItem = treeWidget->topLevelItem(0);
    QVERIFY(dataItem != nullptr);

    int startIndex = -1;
    int endIndex = -1;
    for (int index = 0; index < dataItem->childCount(); ++index) {
        const QString text = dataItem->child(index)->text(0);
        if (text == QString("开始时间")) {
            startIndex = index;
        } else if (text == QString("结束时间")) {
            endIndex = index;
        }
    }

    QVERIFY(startIndex >= 0);
    QVERIFY(endIndex >= 0);
    QCOMPARE(endIndex, startIndex + 1);
}

void ExportAndRepositoryTests::pointDetailDialogSupportsCustomFieldDisplayNames() {
    LaserSpc::Ui::PointDetailDialog dialog;

    QJsonObject dataObject;
    dataObject.insert("templateFilePath", "templates/Program-A.tpl");
    dataObject.insert("cameraProfile", "LineScan-1");
    dataObject.insert("templateRevision", "rev-03");
    dataObject.insert("fieldDisplayNames",
                      QJsonObject{
                          {"cameraProfile", "相机配置"},
                          {"templateRevision", "模板修订版"}
                      });

    QJsonObject rootObject;
    rootObject.insert("data", dataObject);
    rootObject.insert("version", 2);
    dialog.setDetailObject(rootObject);

    auto* treeWidget = dialog.findChild<QTreeWidget*>();
    QVERIFY(treeWidget != nullptr);
    auto* dataItem = treeWidget->topLevelItem(0);
    QVERIFY(dataItem != nullptr);

    bool foundCameraProfile = false;
    bool foundTemplateRevision = false;
    bool foundFieldDisplayNamesNode = false;
    for (int index = 0; index < dataItem->childCount(); ++index) {
        const auto* child = dataItem->child(index);
        if (child->text(0) == QString("相机配置") && child->text(1) == QString("LineScan-1")) {
            foundCameraProfile = true;
        }
        if (child->text(0) == QString("模板修订版") && child->text(1) == QString("rev-03")) {
            foundTemplateRevision = true;
        }
        if (child->text(0) == QString("字段显示名映射")) {
            foundFieldDisplayNamesNode = true;
        }
    }

    QVERIFY(foundCameraProfile);
    QVERIFY(foundTemplateRevision);
    QVERIFY(!foundFieldDisplayNamesNode);
}

void ExportAndRepositoryTests::dataCleanupServiceRemovesSeedRowsFromRealMySql() {
    resetMySqlConnection();
    LaserSpc::Infrastructure::MySqlSpcRepository repository(testDatabaseSettings());
    if (!ensureMySqlAvailable(repository)) {
        return;
    }

    const QString seedBoardCode = "BD-240301-0001";
    {
        auto dbLease = LaserSpc::Infrastructure::DatabaseConnection::acquireMySql(testDatabaseSettings());
        QVERIFY2(dbLease.isOpen(), qPrintable(dbLease.lastError()));
        auto db = dbLease.database();

        QString seedError;
        QVERIFY2(ensureSeedDataLoadedForTests(db, &seedError), qPrintable(seedError));
        QVERIFY(scalarInt(db, "SELECT COUNT(*) FROM board_records WHERE board_code = ?", {seedBoardCode}) > 0);
        QVERIFY(scalarInt(db, "SELECT COUNT(*) FROM point_records WHERE board_code = ?", {seedBoardCode}) > 0);
    }

    const auto cleanupResult = LaserSpc::Infrastructure::DataCleanupService::cleanupSeedData(testDatabaseSettings());
    QVERIFY2(cleanupResult.success, qPrintable(cleanupResult.errorMessage));
    QVERIFY(cleanupResult.deletedPointRecords >= 1);
    QVERIFY(cleanupResult.deletedBoardRecords >= 1);

    resetMySqlConnection();
    {
        auto verifyLease = LaserSpc::Infrastructure::DatabaseConnection::acquireMySql(testDatabaseSettings());
        QVERIFY2(verifyLease.isOpen(), qPrintable(verifyLease.lastError()));
        auto verifyDb = verifyLease.database();
        QCOMPARE(scalarInt(verifyDb, "SELECT COUNT(*) FROM board_records WHERE board_code = ?", {seedBoardCode}), 0);
        QCOMPARE(scalarInt(verifyDb, "SELECT COUNT(*) FROM point_records WHERE board_code = ?", {seedBoardCode}), 0);

        QString restoreError;
        QVERIFY2(ensureSeedDataLoadedForTests(verifyDb, &restoreError), qPrintable(restoreError));
        QCOMPARE(scalarInt(verifyDb, "SELECT COUNT(*) FROM board_records"), 10);
        QCOMPARE(scalarInt(verifyDb, "SELECT COUNT(*) FROM point_records"), 10);
    }
}

void ExportAndRepositoryTests::dataCleanupServiceRemovesOldProductionRowsFromRealMySql() {
    resetMySqlConnection();
    LaserSpc::Infrastructure::MySqlSpcRepository repository(testDatabaseSettings());
    if (!ensureMySqlAvailable(repository)) {
        return;
    }

    const QStringList boardCodes{"UT-CLEAN-OLD-0001", "UT-CLEAN-NEW-0001"};
    {
        auto dbLease = LaserSpc::Infrastructure::DatabaseConnection::acquireMySql(testDatabaseSettings());
        QVERIFY2(dbLease.isOpen(), qPrintable(dbLease.lastError()));
        auto db = dbLease.database();

        removeBoardCodes(db, boardCodes);

        QString errorMessage;
        QVERIFY2(execPrepared(db,
                              "INSERT INTO board_records (board_code, result, line_name, program_name, device_name, operator_name, event_time) "
                              "VALUES (?, ?, ?, ?, ?, ?, ?)",
                              {"UT-CLEAN-OLD-0001", "OK", "L9", "Program-UT", "Laser-UT", "tester",
                               QDateTime::currentDateTime().addDays(-40)},
                              &errorMessage),
                 qPrintable(errorMessage));
        QVERIFY2(execPrepared(db,
                              "INSERT INTO point_records (board_code, point_name, result, read_grade, laser_content, read_code_content, line_name, program_name, device_name, start_time, end_time, detail_json_path) "
                              "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                              {"UT-CLEAN-OLD-0001", "Point-Old", "NG", "D", "Point-Old-LASER", "READ-OLD", "L9", "Program-UT", "Laser-UT",
                               QDateTime::currentDateTime().addDays(-40).addSecs(-30),
                               QDateTime::currentDateTime().addDays(-40), "ut/old.json"},
                              &errorMessage),
                 qPrintable(errorMessage));

        QVERIFY2(execPrepared(db,
                              "INSERT INTO board_records (board_code, result, line_name, program_name, device_name, operator_name, event_time) "
                              "VALUES (?, ?, ?, ?, ?, ?, ?)",
                              {"UT-CLEAN-NEW-0001", "OK", "L9", "Program-UT", "Laser-UT", "tester",
                               QDateTime::currentDateTime().addDays(-2)},
                              &errorMessage),
                 qPrintable(errorMessage));
        QVERIFY2(execPrepared(db,
                              "INSERT INTO point_records (board_code, point_name, result, read_grade, laser_content, read_code_content, line_name, program_name, device_name, start_time, end_time, detail_json_path) "
                              "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                              {"UT-CLEAN-NEW-0001", "Point-New", "OK", "A", "Point-New-LASER", "READ-NEW", "L9", "Program-UT", "Laser-UT",
                               QDateTime::currentDateTime().addDays(-2).addSecs(-30),
                               QDateTime::currentDateTime().addDays(-2), "ut/new.json"},
                              &errorMessage),
                 qPrintable(errorMessage));
    }

    const auto cleanupResult =
        LaserSpc::Infrastructure::DataCleanupService::cleanupProductionDataOlderThan(testDatabaseSettings(), 30);
    QVERIFY2(cleanupResult.success, qPrintable(cleanupResult.errorMessage));
    QVERIFY(cleanupResult.deletedPointRecords >= 1);
    QVERIFY(cleanupResult.deletedBoardRecords >= 1);

    resetMySqlConnection();
    {
        auto verifyLease = LaserSpc::Infrastructure::DatabaseConnection::acquireMySql(testDatabaseSettings());
        QVERIFY2(verifyLease.isOpen(), qPrintable(verifyLease.lastError()));
        auto verifyDb = verifyLease.database();

        QCOMPARE(scalarInt(verifyDb, "SELECT COUNT(*) FROM board_records WHERE board_code = ?", {"UT-CLEAN-OLD-0001"}), 0);
        QCOMPARE(scalarInt(verifyDb, "SELECT COUNT(*) FROM point_records WHERE board_code = ?", {"UT-CLEAN-OLD-0001"}), 0);
        QCOMPARE(scalarInt(verifyDb, "SELECT COUNT(*) FROM board_records WHERE board_code = ?", {"UT-CLEAN-NEW-0001"}), 1);
        QCOMPARE(scalarInt(verifyDb, "SELECT COUNT(*) FROM point_records WHERE board_code = ?", {"UT-CLEAN-NEW-0001"}), 1);

        removeBoardCodes(verifyDb, boardCodes);
    }
}

void ExportAndRepositoryTests::dataCleanupServiceKeepsOldBoardWithRecentPointFromRealMySql() {
    resetMySqlConnection();
    LaserSpc::Infrastructure::MySqlSpcRepository repository(testDatabaseSettings());
    if (!ensureMySqlAvailable(repository)) {
        return;
    }

    const QString boardCode = "UT-CLEAN-MIXED-0001";
    {
        auto dbLease = LaserSpc::Infrastructure::DatabaseConnection::acquireMySql(testDatabaseSettings());
        QVERIFY2(dbLease.isOpen(), qPrintable(dbLease.lastError()));
        auto db = dbLease.database();

        removeBoardCodes(db, {boardCode});

        QString errorMessage;
        const QDateTime now = QDateTime::currentDateTimeUtc();
        QVERIFY2(execPrepared(db,
                              "INSERT INTO board_records (board_code, result, line_name, program_name, device_name, operator_name, event_time) "
                              "VALUES (?, ?, ?, ?, ?, ?, ?)",
                              {boardCode, "OK", "L9", "Program-UT", "Laser-UT", "tester", now.addDays(-40)},
                              &errorMessage),
                 qPrintable(errorMessage));
        QVERIFY2(execPrepared(db,
                              "INSERT INTO point_records (board_code, point_name, result, read_grade, laser_content, read_code_content, line_name, program_name, device_name, start_time, end_time, detail_json_path) "
                              "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
                              {boardCode, "Point-Recent", "OK", "A", "Point-Recent-LASER", "READ-RECENT", "L9", "Program-UT", "Laser-UT",
                               now.addDays(-1).addSecs(-30), now.addDays(-1), "ut/recent.json"},
                              &errorMessage),
                 qPrintable(errorMessage));
    }

    const auto cleanupResult =
        LaserSpc::Infrastructure::DataCleanupService::cleanupProductionDataOlderThan(testDatabaseSettings(), 30);
    QVERIFY2(cleanupResult.success, qPrintable(cleanupResult.errorMessage));

    resetMySqlConnection();
    {
        auto verifyLease = LaserSpc::Infrastructure::DatabaseConnection::acquireMySql(testDatabaseSettings());
        QVERIFY2(verifyLease.isOpen(), qPrintable(verifyLease.lastError()));
        auto verifyDb = verifyLease.database();

        QCOMPARE(scalarInt(verifyDb, "SELECT COUNT(*) FROM board_records WHERE board_code = ?", {boardCode}), 1);
        QCOMPARE(scalarInt(verifyDb, "SELECT COUNT(*) FROM point_records WHERE board_code = ?", {boardCode}), 1);

        removeBoardCodes(verifyDb, {boardCode});
    }
}

void ExportAndRepositoryTests::appConfigServiceDefaultsToNightTheme() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const ScopedEnvVar configFileOverride("LASERSPC_CONFIG_FILE", tempDir.filePath("missing.ini").toUtf8());
    const ScopedEnvVar useMySqlOverride("LASERSPC_USE_MYSQL", QByteArray());
    const ScopedEnvVar hostOverride("LASERSPC_DB_HOST", QByteArray());
    const ScopedEnvVar userOverride("LASERSPC_DB_USER", QByteArray());
    const ScopedEnvVar passwordOverride("LASERSPC_DB_PASSWORD", QByteArray());

    LaserSpc::Infrastructure::AppConfigService configService;
    QCOMPARE(configService.settings().ui.themeStyle, LaserSpc::Infrastructure::ThemeStyle::Night);
    QCOMPARE(configService.settings().database.connectOptions,
             QString("MYSQL_OPT_SSL_ENFORCE=0;MYSQL_OPT_SSL_VERIFY_SERVER_CERT=0"));
}

void ExportAndRepositoryTests::filterPanelRemovesOptionalChipAndKeepsTimeRequired() {
    LaserSpc::Ui::FilterPanel panel(seedFilter());
    panel.setFilterOptions({{"L1"}, {"Program-A"}, {"Laser-01"}});

    auto criteria = seedFilter();
    criteria.lineName = "L1";
    criteria.programName = "Program-A";
    criteria.deviceName = "Laser-01";
    criteria.result = "NG";
    criteria.keyword = "BOARD-001";
    panel.setCriteria(criteria);
    panel.show();

    const auto chipButtons = panel.findChildren<QToolButton*>();
    QCOMPARE(chipButtons.size(), 5);

    auto* lineChipLabel = findLabelContainingText(panel, "线体：L1");
    QVERIFY(lineChipLabel != nullptr);
    auto* lineChipButton = lineChipLabel->parentWidget()->findChild<QToolButton*>();
    QVERIFY(lineChipButton != nullptr);

    QSignalSpy spy(&panel, &LaserSpc::Ui::FilterPanel::queryRequested);
    QTest::mouseClick(lineChipButton, Qt::LeftButton);

    QCOMPARE(spy.count(), 1);
    const auto updated = panel.criteria();
    QCOMPARE(updated.lineName, QString("全部"));
    QCOMPARE(updated.programName, QString("Program-A"));
    QCOMPARE(updated.deviceName, QString("Laser-01"));
    QCOMPARE(updated.result, QString("NG"));
    QCOMPARE(updated.keyword, QString("BOARD-001"));
    QVERIFY(findLabelContainingText(panel, "时间：") != nullptr);
}

void ExportAndRepositoryTests::settingsDialogEditsLanguageAndThemeInOtherSettingsTab() {
    LaserSpc::Infrastructure::AppSettings settings;
    settings.ui.language = LaserSpc::Infrastructure::AppLanguage::Chinese;
    settings.ui.themeStyle = LaserSpc::Infrastructure::ThemeStyle::Ocean;

    LaserSpc::Ui::SettingsDialog dialog(settings);
    dialog.show();

    auto* tabWidget = dialog.findChild<QTabWidget*>();
    QVERIFY(tabWidget != nullptr);
    QVERIFY(tabWidget->count() >= 4);
    QVERIFY(tabWidget->tabText(3).contains("其他") || tabWidget->tabText(3).contains("Other"));
    tabWidget->setCurrentIndex(3);

    auto combos = dialog.findChildren<QComboBox*>();
    QVERIFY(combos.size() >= 2);

    QComboBox* languageCombo = nullptr;
    QComboBox* themeCombo = nullptr;
    for (auto* combo : combos) {
        if (combo->count() == 2 &&
            combo->findData(static_cast<int>(LaserSpc::Infrastructure::AppLanguage::Chinese)) >= 0 &&
            combo->findData(static_cast<int>(LaserSpc::Infrastructure::AppLanguage::English)) >= 0) {
            languageCombo = combo;
        }
        if (combo->count() >= 7 &&
            combo->findData(static_cast<int>(LaserSpc::Infrastructure::ThemeStyle::Night)) >= 0 &&
            combo->findData(static_cast<int>(LaserSpc::Infrastructure::ThemeStyle::Ocean)) >= 0) {
            themeCombo = combo;
        }
    }
    QVERIFY(languageCombo != nullptr);
    QVERIFY(themeCombo != nullptr);

    languageCombo->setCurrentIndex(languageCombo->findData(static_cast<int>(LaserSpc::Infrastructure::AppLanguage::English)));
    themeCombo->setCurrentIndex(themeCombo->findData(static_cast<int>(LaserSpc::Infrastructure::ThemeStyle::Night)));

    const auto updated = dialog.settings();
    QCOMPARE(updated.ui.language, LaserSpc::Infrastructure::AppLanguage::English);
    QCOMPARE(updated.ui.themeStyle, LaserSpc::Infrastructure::ThemeStyle::Night);
}

void ExportAndRepositoryTests::resultToolbarEmitsPageJumpSignal() {
    LaserSpc::Ui::ResultToolbar toolbar("导出 CSV", "导出截图");
    toolbar.setPageSize(20);
    toolbar.setTotalRows(95);
    toolbar.setCurrentPage(2);

    auto* jumpSpin = toolbar.findChild<QSpinBox*>();
    auto* jumpButton = findButtonByText(toolbar, "跳转");
    QVERIFY(jumpSpin != nullptr);
    QVERIFY(jumpButton != nullptr);

    QSignalSpy spy(&toolbar, &LaserSpc::Ui::ResultToolbar::pageJumpRequested);
    jumpSpin->setValue(4);
    QTest::mouseClick(jumpButton, Qt::LeftButton);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toInt(), 4);
}

void ExportAndRepositoryTests::resultToolbarRefreshesPageState() {
    LaserSpc::Ui::ResultToolbar toolbar("导出 CSV", "导出截图");
    toolbar.setPageSize(10);
    toolbar.setTotalRows(35);
    toolbar.setCurrentPage(4);

    auto* pageInfoLabel = findLabelByPrefix(toolbar, "第 ");
    auto* jumpSpin = toolbar.findChild<QSpinBox*>();
    auto* prevButton = findButtonByText(toolbar, "上一页");
    auto* nextButton = findButtonByText(toolbar, "下一页");
    auto* jumpButton = findButtonByText(toolbar, "跳转");

    QVERIFY(pageInfoLabel != nullptr);
    QVERIFY(jumpSpin != nullptr);
    QVERIFY(prevButton != nullptr);
    QVERIFY(nextButton != nullptr);
    QVERIFY(jumpButton != nullptr);

    QCOMPARE(pageInfoLabel->text(), QString("第 4 页 / 共 4 页"));
    QCOMPARE(jumpSpin->maximum(), 4);
    QVERIFY(prevButton->isEnabled());
    QVERIFY(!nextButton->isEnabled());
    QVERIFY(jumpButton->isEnabled());

    toolbar.setTotalRows(0);
    QCOMPARE(pageInfoLabel->text(), QString("第 1 页 / 共 1 页"));
    QCOMPARE(jumpSpin->maximum(), 1);
    QVERIFY(!prevButton->isEnabled());
    QVERIFY(!nextButton->isEnabled());
    QVERIFY(!jumpButton->isEnabled());
}

void ExportAndRepositoryTests::resultToolbarEmitsExportPanelSignal() {
    LaserSpc::Ui::ResultToolbar toolbar("导出 CSV", "导出截图");
    auto* panelButton = findButtonByText(toolbar, "导出面板");
    QVERIFY(panelButton != nullptr);

    QSignalSpy spy(&toolbar, &LaserSpc::Ui::ResultToolbar::exportPanelRequested);
    QTest::mouseClick(panelButton, Qt::LeftButton);

    QCOMPARE(spy.count(), 1);
}

void ExportAndRepositoryTests::exportPanelDialogSupportsFilteringAndPaging() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const ScopedEnvVar exportDirOverride("LASERSPC_EXPORT_DIR", tempDir.path().toUtf8());

    const QString directory = LaserSpc::Infrastructure::ExportService::defaultExportDirectory();
    QStringList createdFiles;
    for (int index = 0; index < 14; ++index) {
        const QString path = QDir(directory).filePath(QString("laser_spc_panel_test_%1.csv").arg(index, 2, 10, QChar('0')));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write("panel");
        file.close();
        createdFiles.append(path);
    }
    for (int index = 0; index < 2; ++index) {
        const QString path = QDir(directory).filePath(QString("laser_spc_panel_image_%1.png").arg(index, 2, 10, QChar('0')));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write("png");
        file.close();
        createdFiles.append(path);
    }

    LaserSpc::Ui::ExportPanelDialog dialog("当前任务：已完成");
    dialog.show();

    auto* combo = dialog.findChild<QComboBox*>();
    auto* list = dialog.findChild<QListWidget*>();
    auto* pageInfoLabel = findLabelByPrefix(dialog, "第 ");
    QVERIFY(combo != nullptr);
    QVERIFY(list != nullptr);
    QVERIFY(pageInfoLabel != nullptr);

    combo->setCurrentText("CSV");
    QTRY_VERIFY(list->count() > 0);
    QCOMPARE(list->count(), 12);
    QVERIFY(dialog.m_nextPageButton->isEnabled());
    QVERIFY(!dialog.m_prevPageButton->isEnabled());
    QVERIFY(!pageInfoLabel->text().isEmpty());

    combo->setCurrentText("PNG");
    QTRY_COMPARE(list->count(), 2);
    QVERIFY(!dialog.m_nextPageButton->isEnabled());

    QString errorMessage;
    QVERIFY2(LaserSpc::Infrastructure::ExportService::removeExportFiles(createdFiles, &errorMessage),
             qPrintable(errorMessage));
}

void ExportAndRepositoryTests::summaryPageDropsStaleQueryResults() {
    runPageStaleQueryRegression<LaserSpc::Ui::SummaryPage>(
        0,
        "数据总览页已刷新：",
        "共 1 行",
        [](LaserSpc::Ui::SummaryPage& page) {
            auto* table = page.findChild<QTableWidget*>();
            QVERIFY(table != nullptr);
            QTRY_COMPARE(table->rowCount(), 1);
            QTRY_COMPARE(table->item(0, 0)->text(), QString("default"));
        },
        [](LaserSpc::Ui::SummaryPage& page) {
            auto* table = page.findChild<QTableWidget*>();
            QVERIFY(table != nullptr);
            QTRY_COMPARE(table->rowCount(), 1);
            QTRY_COMPARE(table->item(0, 0)->text(), QString("fast"));

            auto* summaryLabel = findLabelByPrefix(page, "已加载 ");
            QVERIFY(summaryLabel != nullptr);
            QVERIFY(summaryLabel->text().contains("1 条汇总记录"));
        });
}

void ExportAndRepositoryTests::badStatPageDropsStaleQueryResults() {
    runPageStaleQueryRegression<LaserSpc::Ui::BadStatPage>(
        1,
        "不良统计页已刷新：",
        "Top 不良点 1 项",
        [](LaserSpc::Ui::BadStatPage& page) {
            const auto tables = page.findChildren<QTableWidget*>();
            QVERIFY(tables.size() >= 2);
            auto* badPointTable = tables.at(0);
            auto* gradeTable = tables.at(1);
            QVERIFY(badPointTable != nullptr);
            QVERIFY(gradeTable != nullptr);
            QTRY_COMPARE(badPointTable->rowCount(), 1);
            QTRY_COMPARE(badPointTable->item(0, 0)->text(), QString("default"));
            QTRY_COMPARE(gradeTable->rowCount(), 1);
            QTRY_COMPARE(gradeTable->item(0, 0)->text(), QString("default"));
        },
        [](LaserSpc::Ui::BadStatPage& page) {
            const auto tables = page.findChildren<QTableWidget*>();
            QVERIFY(tables.size() >= 2);
            auto* badPointTable = tables.at(0);
            auto* gradeTable = tables.at(1);
            QVERIFY(badPointTable != nullptr);
            QVERIFY(gradeTable != nullptr);
            QTRY_COMPARE(badPointTable->rowCount(), 1);
            QTRY_COMPARE(badPointTable->item(0, 0)->text(), QString("fast"));
            QTRY_COMPARE(gradeTable->rowCount(), 1);
            QTRY_COMPARE(gradeTable->item(0, 0)->text(), QString("fast"));

            auto* hintLabel = findLabelByPrefix(page, "当前不良点 ");
            QVERIFY(hintLabel != nullptr);
            QVERIFY(hintLabel->text().contains("1 项"));
        });
}

void ExportAndRepositoryTests::boardRecordPageDropsStaleQueryResults() {
    runPageStaleQueryRegression<LaserSpc::Ui::BoardRecordPage>(
        2,
        "单板记录页已刷新：",
        "共 1 行",
        [](LaserSpc::Ui::BoardRecordPage& page) {
            auto* table = page.findChild<QTableWidget*>();
            QVERIFY(table != nullptr);
            QTRY_COMPARE(table->rowCount(), 1);
            QTRY_COMPARE(table->item(0, 0)->text(), QString("Program-default"));
        },
        [](LaserSpc::Ui::BoardRecordPage& page) {
            auto* table = page.findChild<QTableWidget*>();
            QVERIFY(table != nullptr);
            QTRY_COMPARE(table->rowCount(), 1);
            QTRY_COMPARE(table->item(0, 0)->text(), QString("Program-fast"));
            QTRY_COMPARE(table->item(0, 5)->text(), QString("fast"));

            auto* hintLabel = findLabelByPrefix(page, "已加载 ");
            QVERIFY(hintLabel != nullptr);
            QVERIFY(hintLabel->text().contains("1 条单板记录"));
        });
}

void ExportAndRepositoryTests::pointRecordPageDropsStaleQueryResults() {
    runPageStaleQueryRegression<LaserSpc::Ui::PointRecordPage>(
        3,
        "点位记录页已刷新：",
        "共 1 行",
        [](LaserSpc::Ui::PointRecordPage& page) {
            auto* table = page.findChild<QTableWidget*>();
            QVERIFY(table != nullptr);
            QTRY_COMPARE(table->rowCount(), 1);
            QTRY_COMPARE(table->item(0, 0)->text(), QString("Program-default"));
        },
        [](LaserSpc::Ui::PointRecordPage& page) {
            auto* table = page.findChild<QTableWidget*>();
            QVERIFY(table != nullptr);
            QTRY_COMPARE(table->rowCount(), 1);
            QTRY_COMPARE(table->item(0, 0)->text(), QString("Program-fast"));
            QTRY_COMPARE(table->item(0, 1)->text(), QString("BOARD-FAST"));
            QTRY_COMPARE(table->item(0, 2)->text(), QString("POINT-FAST"));

            auto* hintLabel = findLabelByPrefix(page, "已加载 ");
            QVERIFY(hintLabel != nullptr);
            QVERIFY(hintLabel->text().contains("1 条点位记录"));
        });
}

void ExportAndRepositoryTests::summaryPageEmitsBoardDrillDownSignal() {
    auto repository = std::make_unique<LaserSpc::Infrastructure::MockSpcRepository>();
    LaserSpc::App::AppServiceFacade facade(std::move(repository), "Mock Repository");
    LaserSpc::Ui::SummaryPage page(&facade);

    LaserSpc::Domain::FilterCriteria criteria;
    criteria.beginTime = QDateTime::currentDateTime().addDays(-1);
    criteria.endTime = QDateTime::currentDateTime().addDays(1);
    criteria.lineName = "全部";
    criteria.programName = "全部";
    criteria.deviceName = "全部";
    criteria.result = "全部";
    QSignalSpy statusSpy(&page, &LaserSpc::Ui::SummaryPage::statusMessageChanged);
    page.reload(criteria);

    auto* table = page.findChild<QTableWidget*>();
    QVERIFY(table != nullptr);
    QTRY_VERIFY(table->rowCount() > 0);

    bool called = false;
    LaserSpc::Domain::FilterCriteria receivedCriteria;
    QString receivedMessage;
    QObject::connect(&page, &LaserSpc::Ui::SummaryPage::boardDrillDownRequested, &page,
                     [&](const LaserSpc::Domain::FilterCriteria& emittedCriteria, const QString& message) {
                         called = true;
                         receivedCriteria = emittedCriteria;
                         receivedMessage = message;
                     });

    QVERIFY(QMetaObject::invokeMethod(table, "cellDoubleClicked", Qt::DirectConnection, Q_ARG(int, 0), Q_ARG(int, 0)));

    QVERIFY(called);
    QCOMPARE(receivedCriteria.lineName, table->item(0, 1)->text());
    QCOMPARE(receivedCriteria.programName, table->item(0, 0)->text());
    QCOMPARE(receivedCriteria.deviceName, table->item(0, 2)->text());
    QVERIFY(receivedCriteria.keyword.isEmpty());
    QVERIFY(receivedMessage.contains("单板记录"));
}

void ExportAndRepositoryTests::boardRecordPageEmitsPointDrillDownSignal() {
    auto repository = std::make_unique<LaserSpc::Infrastructure::MockSpcRepository>();
    LaserSpc::App::AppServiceFacade facade(std::move(repository), "Mock Repository");
    LaserSpc::Ui::BoardRecordPage page(&facade);

    LaserSpc::Domain::FilterCriteria criteria;
    criteria.beginTime = QDateTime::currentDateTime().addDays(-1);
    criteria.endTime = QDateTime::currentDateTime().addDays(1);
    criteria.lineName = "全部";
    criteria.programName = "全部";
    criteria.deviceName = "全部";
    criteria.result = "全部";
    QSignalSpy statusSpy(&page, &LaserSpc::Ui::BoardRecordPage::statusMessageChanged);
    page.reload(criteria);

    auto* table = page.findChild<QTableWidget*>();
    QVERIFY(table != nullptr);
    QTRY_VERIFY(table->rowCount() > 0);

    bool called = false;
    LaserSpc::Domain::FilterCriteria receivedCriteria;
    QString receivedMessage;
    QObject::connect(&page, &LaserSpc::Ui::BoardRecordPage::pointDrillDownRequested, &page,
                     [&](const LaserSpc::Domain::FilterCriteria& emittedCriteria, const QString& message) {
                         called = true;
                         receivedCriteria = emittedCriteria;
                         receivedMessage = message;
                     });

    QVERIFY(QMetaObject::invokeMethod(table, "cellDoubleClicked", Qt::DirectConnection, Q_ARG(int, 0), Q_ARG(int, 0)));

    QVERIFY(called);
    QCOMPARE(receivedCriteria.keyword, table->item(0, 1)->text());
    QCOMPARE(receivedCriteria.lineName, table->item(0, 3)->text());
    QCOMPARE(receivedCriteria.programName, table->item(0, 0)->text());
    QCOMPARE(receivedCriteria.deviceName, table->item(0, 4)->text());
    QVERIFY(receivedMessage.contains("点位记录"));
}

void ExportAndRepositoryTests::badStatPageEmitsPointDrillDownForBadPoint() {
    auto repository = std::make_unique<LaserSpc::Infrastructure::MockSpcRepository>();
    LaserSpc::App::AppServiceFacade facade(std::move(repository), "Mock Repository");
    LaserSpc::Ui::BadStatPage page(&facade);

    LaserSpc::Domain::FilterCriteria criteria;
    criteria.beginTime = QDateTime::currentDateTime().addDays(-1);
    criteria.endTime = QDateTime::currentDateTime().addDays(1);
    criteria.lineName = "全部";
    criteria.programName = "全部";
    criteria.deviceName = "全部";
    criteria.result = "全部";
    QSignalSpy statusSpy(&page, &LaserSpc::Ui::BadStatPage::statusMessageChanged);
    page.reload(criteria);

    auto tables = page.findChildren<QTableWidget*>();
    QVERIFY(tables.size() >= 2);
    auto* badPointTable = tables.at(0);
    QTRY_VERIFY(badPointTable->rowCount() > 0);

    bool called = false;
    LaserSpc::Domain::FilterCriteria receivedCriteria;
    QString receivedMessage;
    QObject::connect(&page, &LaserSpc::Ui::BadStatPage::pointDrillDownRequested, &page,
                     [&](const LaserSpc::Domain::FilterCriteria& emittedCriteria, const QString& message) {
                         called = true;
                         receivedCriteria = emittedCriteria;
                         receivedMessage = message;
                     });

    QVERIFY(QMetaObject::invokeMethod(badPointTable, "cellDoubleClicked", Qt::DirectConnection, Q_ARG(int, 0), Q_ARG(int, 0)));

    QVERIFY(called);
    QCOMPARE(receivedCriteria.result, QString("NG"));
    QCOMPARE(receivedCriteria.keyword, badPointTable->item(0, 0)->text());
    QVERIFY(receivedMessage.contains("不良点"));
}

void ExportAndRepositoryTests::badStatPageEmitsPointDrillDownForGrade() {
    auto repository = std::make_unique<LaserSpc::Infrastructure::MockSpcRepository>();
    LaserSpc::App::AppServiceFacade facade(std::move(repository), "Mock Repository");
    LaserSpc::Ui::BadStatPage page(&facade);

    LaserSpc::Domain::FilterCriteria criteria;
    criteria.beginTime = QDateTime::currentDateTime().addDays(-1);
    criteria.endTime = QDateTime::currentDateTime().addDays(1);
    criteria.lineName = "全部";
    criteria.programName = "全部";
    criteria.deviceName = "全部";
    criteria.result = "全部";
    QSignalSpy statusSpy(&page, &LaserSpc::Ui::BadStatPage::statusMessageChanged);
    page.reload(criteria);

    auto tables = page.findChildren<QTableWidget*>();
    QVERIFY(tables.size() >= 2);
    auto* gradeTable = tables.at(1);
    QTRY_VERIFY(gradeTable->rowCount() > 0);

    bool called = false;
    LaserSpc::Domain::FilterCriteria receivedCriteria;
    QString receivedMessage;
    QObject::connect(&page, &LaserSpc::Ui::BadStatPage::pointDrillDownRequested, &page,
                     [&](const LaserSpc::Domain::FilterCriteria& emittedCriteria, const QString& message) {
                         called = true;
                         receivedCriteria = emittedCriteria;
                         receivedMessage = message;
                     });

    QVERIFY(QMetaObject::invokeMethod(gradeTable, "cellDoubleClicked", Qt::DirectConnection, Q_ARG(int, 0), Q_ARG(int, 0)));

    QVERIFY(called);
    QCOMPARE(receivedCriteria.result, QString("全部"));
    QCOMPARE(receivedCriteria.keyword, gradeTable->item(0, 0)->text());
    QVERIFY(receivedMessage.contains("读码等级"));
}

void ExportAndRepositoryTests::summaryPageLoadsRealMySqlData() {
    auto repository = std::make_unique<LaserSpc::Infrastructure::MySqlSpcRepository>(testDatabaseSettings());
    if (!ensureMySqlAvailable(*repository)) {
        return;
    }

    LaserSpc::App::AppServiceFacade facade(std::move(repository), "MySQL Repository");
    LaserSpc::Ui::SummaryPage page(&facade);
    QSignalSpy statusSpy(&page, &LaserSpc::Ui::SummaryPage::statusMessageChanged);
    page.reload(seedFilter());

    auto* table = page.findChild<QTableWidget*>();
    QVERIFY(table != nullptr);
    QTRY_COMPARE(table->rowCount(), 6);
    QCOMPARE(table->rowCount(), 6);
    QCOMPARE(table->item(0, 0)->text(), QString("Program-E"));
    QCOMPARE(table->item(0, 6)->text(), QString("2026-03-10 08:22:00"));

    auto* summaryLabel = findLabelByPrefix(page, "已加载 ");
    QVERIFY(summaryLabel != nullptr);
    QVERIFY(summaryLabel->text().contains("6 条汇总记录"));
}

void ExportAndRepositoryTests::pointRecordPageLoadsRealMySqlData() {
    auto repository = std::make_unique<LaserSpc::Infrastructure::MySqlSpcRepository>(testDatabaseSettings());
    if (!ensureMySqlAvailable(*repository)) {
        return;
    }

    LaserSpc::App::AppServiceFacade facade(std::move(repository), "MySQL Repository");
    LaserSpc::Ui::PointRecordPage page(&facade);
    QSignalSpy statusSpy(&page, &LaserSpc::Ui::PointRecordPage::statusMessageChanged);
    page.reload(seedFilter());

    auto* table = page.findChild<QTableWidget*>();
    QVERIFY(table != nullptr);
    QTRY_COMPARE(table->rowCount(), 10);
    QCOMPARE(table->rowCount(), 10);
    QCOMPARE(table->item(0, 0)->text(), QString("Program-E"));
    QCOMPARE(table->item(0, 1)->text(), QString("BD-240301-0010"));
    QCOMPARE(table->item(0, 2)->text(), QString("PrintShift"));
    QCOMPARE(table->item(0, 5)->text(), QString("PrintShift-LASER"));
    QCOMPARE(table->item(0, 7)->text(), QString("2026-03-10 08:21:10"));
    QCOMPARE(table->item(0, 8)->text(), QString("2026-03-10 08:22:00"));

    auto* hintLabel = findLabelByPrefix(page, "已加载 ");
    QVERIFY(hintLabel != nullptr);
    QVERIFY(hintLabel->text().contains("10 条点位记录"));
}

void ExportAndRepositoryTests::mainWindowLoadsRealMySqlDataAndRefreshesCurrentPage() {
    auto repository = std::make_unique<LaserSpc::Infrastructure::MySqlSpcRepository>(testDatabaseSettings());
    if (!ensureMySqlAvailable(*repository)) {
        return;
    }

    LaserSpc::App::AppServiceFacade facade(std::move(repository), "MySQL Repository");
    LaserSpc::Ui::MainWindow window(&facade);
    window.show();
    QCoreApplication::processEvents();

    auto* dataSourceLabel = findLabelContainingText(window, "MySQL Repository");
    QVERIFY(dataSourceLabel != nullptr);

    auto* pageTitleLabel = findLabelByExactText(window, "数据总览");
    QVERIFY(pageTitleLabel != nullptr);

    auto* filterPanel = window.findChild<LaserSpc::Ui::FilterPanel*>();
    QVERIFY(filterPanel != nullptr);
    QTRY_VERIFY(filterPanel->isEnabled());

    auto* lineCombo = findComboContainingText(*filterPanel, "L1");
    auto* programCombo = findComboContainingText(*filterPanel, "Program-A");
    auto* deviceCombo = findComboContainingText(*filterPanel, "Laser-01");
    QTRY_VERIFY(lineCombo != nullptr);
    QTRY_VERIFY(programCombo != nullptr);
    QTRY_VERIFY(deviceCombo != nullptr);
    lineCombo = findComboContainingText(*filterPanel, "L1");
    programCombo = findComboContainingText(*filterPanel, "Program-A");
    deviceCombo = findComboContainingText(*filterPanel, "Laser-01");
    QVERIFY(lineCombo != nullptr);
    QVERIFY(programCombo != nullptr);
    QVERIFY(deviceCombo != nullptr);
    QCOMPARE(lineCombo->count(), 4);
    QCOMPARE(programCombo->count(), 7);
    QCOMPARE(deviceCombo->count(), 7);

    auto* navigation = window.findChild<QListWidget*>();
    auto* stack = window.findChild<QStackedWidget*>();
    QVERIFY(navigation != nullptr);
    QVERIFY(stack != nullptr);
    QCOMPARE(navigation->currentRow(), 0);
    QVERIFY(qobject_cast<LaserSpc::Ui::SummaryPage*>(stack->currentWidget()) != nullptr);

    navigation->setCurrentRow(3);
    QCoreApplication::processEvents();

    pageTitleLabel = findLabelByExactText(window, "点位记录");
    QVERIFY(pageTitleLabel != nullptr);
    auto* pointPage = qobject_cast<LaserSpc::Ui::PointRecordPage*>(stack->currentWidget());
    QVERIFY(pointPage != nullptr);
    auto* pointTable = pointPage->findChild<QTableWidget*>();
    QVERIFY(pointTable != nullptr);

    auto criteria = seedFilter();
    filterPanel->setCriteria(criteria);

    auto* queryButton = findButtonByText(*filterPanel, "查询");
    QVERIFY(queryButton != nullptr);
    QTest::mouseClick(queryButton, Qt::LeftButton);
    QCoreApplication::processEvents();

    QTRY_COMPARE(pointTable->rowCount(), 10);

    criteria.programName = "Program-B";
    criteria.result = "NG";
    filterPanel->setCriteria(criteria);
    QTest::mouseClick(queryButton, Qt::LeftButton);
    QCoreApplication::processEvents();

    QTRY_COMPARE(pointTable->rowCount(), 2);
    QCOMPARE(pointTable->rowCount(), 2);
    QCOMPARE(pointTable->item(0, 1)->text(), QString("BD-240301-0008"));
    QCOMPARE(pointTable->item(1, 1)->text(), QString("BD-240301-0003"));

    auto* statusLabel = findLabelContainingText(window, "2");
    QVERIFY(statusLabel != nullptr);
    QVERIFY(statusLabel->text().contains("2"));
}

void ExportAndRepositoryTests::summaryPageShowsErrorWhenRepositoryQueryFails() {
    resetMySqlConnection();
    auto repository = std::make_unique<LaserSpc::Infrastructure::MySqlSpcRepository>(unavailableMySqlSettings().database);
    LaserSpc::App::AppServiceFacade facade(std::move(repository), "Broken MySQL Repository");
    LaserSpc::Ui::SummaryPage page(&facade);

    QSignalSpy statusSpy(&page, &LaserSpc::Ui::SummaryPage::statusMessageChanged);
    page.reload(seedFilter());

    QTRY_VERIFY(statusSpy.count() > 0);
    QTRY_VERIFY(findLabelContainingText(page, "查询失败：") != nullptr);
    auto* summaryLabel = findLabelContainingText(page, "查询失败：");
    QVERIFY(summaryLabel != nullptr);
    QVERIFY(summaryLabel->text().contains("查询失败："));

    QVERIFY(statusSpy.count() >= 1);
    const auto lastSignal = statusSpy.takeLast();
    QVERIFY(lastSignal.at(0).toString().startsWith("数据总览页查询失败："));
}

void ExportAndRepositoryTests::mainWindowUsesFallbackRepositoryWhenMySqlBuildFails() {
    resetMySqlConnection();
    auto buildResult = LaserSpc::Infrastructure::RepositoryFactory::build(unavailableMySqlFallbackSettings());
    QVERIFY(buildResult.repository != nullptr);

    LaserSpc::App::AppServiceFacade facade(std::move(buildResult.repository), buildResult.dataSourceMode);
    LaserSpc::Ui::MainWindow window(&facade);
    window.show();
    QCoreApplication::processEvents();

    auto* dataSourceLabel = findLabelContainingText(window, "Mock Repository (MySQL fallback failed)");
    QVERIFY(dataSourceLabel != nullptr);

    QTRY_VERIFY(findLabelContainingText(window, "Mock Repository (MySQL fallback failed)") != nullptr);
    auto* statusLabel = findLabelContainingText(window, "页");
    QVERIFY(statusLabel != nullptr);
    QVERIFY(statusLabel->text().contains("页"));

    auto* stack = window.findChild<QStackedWidget*>();
    QVERIFY(stack != nullptr);
    auto* summaryPage = qobject_cast<LaserSpc::Ui::SummaryPage*>(stack->currentWidget());
    QVERIFY(summaryPage != nullptr);

    auto* summaryTable = summaryPage->findChild<QTableWidget*>();
    QVERIFY(summaryTable != nullptr);
    QTRY_COMPARE(summaryTable->rowCount(), 9);
    QCOMPARE(summaryTable->rowCount(), 9);
}

void ExportAndRepositoryTests::mockRepositoryProvidesFilterOptions() {
    LaserSpc::Infrastructure::MockSpcRepository repository;
    const auto options = repository.fetchFilterOptions();

    QCOMPARE(options.lineNames, QStringList({"L1", "L2", "L3", "L4", "L5", "L6", "L7"}));
    QCOMPARE(options.programNames,
             QStringList({"Program-A", "Program-B", "Program-C", "Program-D", "Program-E",
                          "Program-F", "Program-G", "Program-H", "Program-I", "Program-J"}));
    QCOMPARE(options.deviceNames,
             QStringList({"Laser-01", "Laser-02", "Laser-03", "Laser-04", "Laser-05",
                          "Laser-06", "Laser-07", "Laser-08", "Laser-09", "Laser-10"}));
}

void ExportAndRepositoryTests::appServiceFacadeUpdatesSettingsOnRepositoryReplace() {
    auto repository = std::make_unique<LaserSpc::Infrastructure::MockSpcRepository>();
    LaserSpc::App::AppServiceFacade facade(std::move(repository), "Mock Repository");

    auto settings = facade.settings();
    settings.useMySql = false;
    settings.defaultQueryDays = 3;
    settings.autoRefreshEnabled = true;
    settings.autoRefreshIntervalSeconds = 12;
    settings.database.port = 9527;
    settings.database.connectTimeoutSeconds = 7;
    settings.database.readTimeoutSeconds = 18;

    auto replacementRepository = std::make_unique<LaserSpc::Infrastructure::MockSpcRepository>();
    facade.replaceRepository(std::move(replacementRepository), "Mock Repository 2", settings);

    const auto refreshedSettings = facade.settings();
    QCOMPARE(facade.dataSourceMode(), QString("Mock Repository 2"));
    QCOMPARE(refreshedSettings.defaultQueryDays, 3);
    QCOMPARE(refreshedSettings.autoRefreshEnabled, true);
    QCOMPARE(refreshedSettings.autoRefreshIntervalSeconds, 12);
    QCOMPARE(refreshedSettings.database.port, 9527);
    QCOMPARE(refreshedSettings.database.connectTimeoutSeconds, 7);
    QCOMPARE(refreshedSettings.database.readTimeoutSeconds, 18);
}

void ExportAndRepositoryTests::dashboardWidgetEmbedsInsideHostContainer() {
    auto repository = std::make_unique<LaserSpc::Infrastructure::MockSpcRepository>();
    LaserSpc::App::AppServiceFacade facade(std::move(repository), "Mock Repository");

    QWidget host;
    auto* layout = new QVBoxLayout(&host);
    auto* dashboard = new LaserSpc::Ui::DashboardWidget(&facade, &host);
    layout->addWidget(dashboard);
    host.resize(1280, 800);
    host.show();

    auto* filterPanel = dashboard->findChild<LaserSpc::Ui::FilterPanel*>();
    auto* stack = dashboard->findChild<QStackedWidget*>();
    QVERIFY(filterPanel != nullptr);
    QVERIFY(stack != nullptr);
    QTRY_VERIFY(filterPanel->isEnabled());
    QCOMPARE(stack->count(), 4);
    QVERIFY(findLabelContainingText(*dashboard, "Mock Repository") != nullptr);
}

void ExportAndRepositoryTests::dashboardWidgetUsesCurrentPageSizeHintWhenSwitching() {
    auto repository = std::make_unique<LaserSpc::Infrastructure::MockSpcRepository>();
    LaserSpc::App::AppServiceFacade facade(std::move(repository), "Mock Repository");

    QWidget host;
    auto* layout = new QVBoxLayout(&host);
    auto* dashboard = new LaserSpc::Ui::DashboardWidget(&facade, &host);
    layout->addWidget(dashboard);
    host.resize(1280, 800);
    host.show();

    auto* filterPanel = dashboard->findChild<LaserSpc::Ui::FilterPanel*>();
    auto* stack = dashboard->findChild<QStackedWidget*>();
    auto* summaryPage = dashboard->findChild<LaserSpc::Ui::SummaryPage*>();
    auto* badStatPage = dashboard->findChild<LaserSpc::Ui::BadStatPage*>();
    QVERIFY(filterPanel != nullptr);
    QVERIFY(stack != nullptr);
    QVERIFY(summaryPage != nullptr);
    QVERIFY(badStatPage != nullptr);
    QTRY_VERIFY(filterPanel->isEnabled());

    summaryPage->setMinimumHeight(320);
    badStatPage->setMinimumHeight(2200);

    stack->setCurrentWidget(summaryPage);
    stack->updateGeometry();
    dashboard->updateGeometry();
    QCoreApplication::processEvents();
    QVERIFY2(stack->sizeHint().height() < 1200, "Hidden pages should not stretch the dashboard height hint.");

    stack->setCurrentWidget(badStatPage);
    stack->updateGeometry();
    QCoreApplication::processEvents();
    const int badStatHeightHint = stack->sizeHint().height();
    QVERIFY(badStatHeightHint > 0);

    stack->setCurrentWidget(summaryPage);
    stack->updateGeometry();
    dashboard->updateGeometry();
    QCoreApplication::processEvents();
    QVERIFY2(stack->sizeHint().height() < 1200, "Switching back should restore the current page height hint.");
}

void ExportAndRepositoryTests::dashboardWidgetSwitchingToSummaryKeepsStableHeight() {
    auto repository = std::make_unique<LaserSpc::Infrastructure::MockSpcRepository>();
    LaserSpc::App::AppServiceFacade facade(std::move(repository), "Mock Repository");

    QWidget host;
    auto* layout = new QVBoxLayout(&host);
    auto* dashboard = new LaserSpc::Ui::DashboardWidget(&facade, &host);
    layout->addWidget(dashboard);
    host.resize(1280, 800);
    host.show();

    auto* filterPanel = dashboard->findChild<LaserSpc::Ui::FilterPanel*>();
    auto* stack = dashboard->findChild<QStackedWidget*>();
    auto* navigation = dashboard->findChild<QListWidget*>();
    QVERIFY(filterPanel != nullptr);
    QVERIFY(stack != nullptr);
    QVERIFY(navigation != nullptr);
    QTRY_VERIFY(filterPanel->isEnabled());

    navigation->setCurrentRow(0);
    QCoreApplication::processEvents();
    const int initialSummaryHeight = stack->sizeHint().height();
    QVERIFY(initialSummaryHeight > 0);

    for (int row : {1, 2, 3}) {
        navigation->setCurrentRow(row);
        QCoreApplication::processEvents();
        navigation->setCurrentRow(0);
        QCoreApplication::processEvents();
        QVERIFY2(stack->sizeHint().height() <= initialSummaryHeight + 32,
                 "Switching back to summary should not accumulate extra vertical size.");
    }
}

void ExportAndRepositoryTests::dashboardWidgetShowsPageSpecificInsights() {
    auto repository = std::make_unique<DelayedSummaryRepository>();
    LaserSpc::App::AppServiceFacade facade(std::move(repository), "Delayed Summary Repository");

    LaserSpc::Ui::DashboardWidget dashboard(&facade);
    dashboard.resize(1280, 800);
    dashboard.show();

    QTRY_VERIFY(dashboard.m_sidebarFocusTitleLabel != nullptr);
    QTRY_COMPARE(dashboard.m_sidebarFocusTitleLabel->text(), QString("总产出"));
    QTRY_VERIFY(dashboard.m_sidebarFocusValueLabel->text().contains("1"));

    dashboard.m_navigation->setCurrentRow(3);
    QTRY_COMPARE(dashboard.m_sidebarFocusTitleLabel->text(), QString("记录总量"));
    QTRY_COMPARE(dashboard.m_sidebarActionTitleLabel->text(), QString("主等级"));
    QTRY_VERIFY(!dashboard.m_sidebarActionValueLabel->text().trimmed().isEmpty());
}

void ExportAndRepositoryTests::dashboardWidgetAutoRefreshAdvancesFilterEndTime() {
    auto repository = std::make_unique<LaserSpc::Infrastructure::MockSpcRepository>();
    LaserSpc::App::AppServiceFacade facade(std::move(repository), "Mock Repository");

    auto settings = facade.settings();
    settings.autoRefreshEnabled = true;
    settings.autoRefreshIntervalSeconds = 5;
    facade.replaceRepository(std::make_unique<LaserSpc::Infrastructure::MockSpcRepository>(), "Mock Repository", settings);

    LaserSpc::Ui::DashboardWidget dashboard(&facade);
    dashboard.resize(1280, 800);
    dashboard.show();

    auto* filterPanel = dashboard.findChild<LaserSpc::Ui::FilterPanel*>();
    QVERIFY(filterPanel != nullptr);
    QTRY_VERIFY(filterPanel->isEnabled());

    auto criteria = seedFilter();
    criteria.endTime = criteria.endTime.addSecs(-120);
    filterPanel->setCriteria(criteria);
    const QDateTime before = filterPanel->criteria().endTime;

    QVERIFY(QMetaObject::invokeMethod(dashboard.m_autoRefreshTimer, "timeout", Qt::DirectConnection));
    QTRY_VERIFY(filterPanel->criteria().endTime > before);
}

int main(int argc, char* argv[]) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    LaserSpc::Infrastructure::RuntimeDiagnostics::prepareQtRuntime();
    QApplication app(argc, argv);
    ExportAndRepositoryTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "ExportAndRepositoryTests.moc"

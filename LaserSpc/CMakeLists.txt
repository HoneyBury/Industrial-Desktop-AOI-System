cmake_minimum_required(VERSION 3.16)

project(LaserSpc VERSION 0.1.0 LANGUAGES CXX)

option(LASERSPC_BUILD_HOST_WRITER_EXAMPLE "Build the host writer example with embedded LaserSpc UI" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if (MSVC)
    add_compile_options(/utf-8)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets Sql Charts Concurrent Test Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Sql Charts Concurrent Test Network)

set(LASERSPC_CORE_SOURCES
    src/app/AppServiceFacade.cpp
    src/app/IngestService.cpp
    src/app/QueryGateways.cpp
    src/app/SummaryQueryService.cpp
    src/app/StatQueryService.cpp
    src/app/RecordQueryService.cpp
    src/infrastructure/AppConfigService.cpp
    src/infrastructure/DataCleanupService.cpp
    src/infrastructure/DatabaseConnection.cpp
    src/infrastructure/ExportService.cpp
    src/infrastructure/MesEventForwarder.cpp
    src/infrastructure/MockSpcRepository.cpp
    src/infrastructure/MySqlQueryBuilder.cpp
    src/infrastructure/MySqlSpcRepository.cpp
    src/infrastructure/MySqlSpcWriteRepository.cpp
    src/infrastructure/MySqlTransactionExecutor.cpp
    src/infrastructure/PointDetailJsonService.cpp
    src/infrastructure/RepositoryFactory.cpp
    src/infrastructure/RuntimeDiagnostics.cpp
)

set(LASERSPC_UI_SOURCES
    src/ui/dialogs/SettingsDialog.cpp
    src/ui/dialogs/MesConfigDialog.cpp
    src/ui/dialogs/ExportPanelDialog.cpp
    src/ui/dialogs/ExportProgressDialog.cpp
    src/ui/dialogs/DiagnosticsDialog.cpp
    src/ui/dialogs/PointDetailDialog.cpp
    src/ui/common/EmptyStateWidget.cpp
    src/ui/common/UiTheme.cpp
    src/ui/widgets/FilterPanel.cpp
    src/ui/widgets/ResultToolbar.cpp
    src/ui/pages/SummaryPage.cpp
    src/ui/pages/BadStatPage.cpp
    src/ui/pages/BoardRecordPage.cpp
    src/ui/pages/PointRecordPage.cpp
    src/ui/shell/DashboardPageController.cpp
    src/ui/shell/MainShellMediator.cpp
    src/ui/shell/DashboardWidget.cpp
    src/ui/shell/MainWindow.cpp
)

add_library(LaserSpcCore STATIC ${LASERSPC_CORE_SOURCES})
target_include_directories(LaserSpcCore PUBLIC src)
target_link_libraries(LaserSpcCore
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Sql
        Qt${QT_VERSION_MAJOR}::Concurrent
        Qt${QT_VERSION_MAJOR}::Network
)

add_library(LaserSpcUi STATIC ${LASERSPC_UI_SOURCES})
target_include_directories(LaserSpcUi PUBLIC src)
target_link_libraries(LaserSpcUi
    PUBLIC
        LaserSpcCore
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Charts
)

add_executable(LaserSpc src/main.cpp)
target_include_directories(LaserSpc PRIVATE src)
target_link_libraries(LaserSpc PRIVATE LaserSpcUi)

enable_testing()

add_executable(LaserSpcTests tests/ExportAndRepositoryTests.cpp)

target_include_directories(LaserSpcTests PRIVATE
    src
    examples/host_spc_writer
)

target_sources(LaserSpcTests PRIVATE
    examples/host_spc_writer/HostSpcExampleWindow.cpp
    examples/host_spc_writer/MesDebugWindow.cpp
)

target_link_libraries(LaserSpcTests PRIVATE
    LaserSpcUi
    LaserSpcHostWriterSupport
    Qt${QT_VERSION_MAJOR}::Test
)

add_executable(LaserSpcDbInit
    tools/LaserSpcDbInit.cpp
)
target_link_libraries(LaserSpcDbInit PRIVATE LaserSpcCore Qt${QT_VERSION_MAJOR}::Widgets)

add_executable(LaserSpcIngestServer
    tools/LaserSpcIngestServer.cpp
)

target_include_directories(LaserSpcIngestServer PRIVATE src)

target_link_libraries(LaserSpcIngestServer PRIVATE LaserSpcCore)

if (WIN32 AND QT_VERSION_MAJOR EQUAL 6)
    set(LASERSPC_QT_BIN_DIR "${Qt6_DIR}/../../../bin")
    set(LASERSPC_QT_PLUGIN_DIR "${Qt6_DIR}/../../../plugins/sqldrivers")
    function(laserspc_copy_mysql_runtime target_name)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target_name}>/sqldrivers"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LASERSPC_QT_BIN_DIR}/libmariadb.dll"
                "$<TARGET_FILE_DIR:${target_name}>/libmariadb.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LASERSPC_QT_PLUGIN_DIR}/$<IF:$<CONFIG:Debug>,qsqlmysqld.dll,qsqlmysql.dll>"
                "$<TARGET_FILE_DIR:${target_name}>/sqldrivers/$<IF:$<CONFIG:Debug>,qsqlmysqld.dll,qsqlmysql.dll>"
        )
    endfunction()

    laserspc_copy_mysql_runtime(LaserSpc)
    laserspc_copy_mysql_runtime(LaserSpcDbInit)
    laserspc_copy_mysql_runtime(LaserSpcIngestServer)
endif()

add_subdirectory(examples/host_spc_writer)

add_custom_command(TARGET LaserSpc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/seed_data"
        "$<TARGET_FILE_DIR:LaserSpc>/seed_data"
)

add_executable(LaserSpcPerfTool tools/LaserSpcPerfTool.cpp)

target_include_directories(LaserSpcPerfTool PRIVATE src)

target_link_libraries(LaserSpcPerfTool PRIVATE LaserSpcCore)

set(LASERSPC_FAST_TESTS
    appConfigServiceReadsAndWritesIniFile
    appServiceFacadeUpdatesSettingsOnRepositoryReplace
    ingestServiceNormalizesDefaultTimesToUtc
    hostWriterBuildBatchGeneratesUniqueRequestIds
    hostWriterAggregatorDefersEmptyBoards
    hostWriterAggregatorPreservesDuplicateRequestIdOrder
    hostWriterExamplePointRowWritesDetailJson
    hostWriterExampleWindowProvidesAlgorithmPlanDebugButton
    pointDetailDialogShowsChineseFieldLabels
    pointDetailDialogPlacesStartAndEndTimeAdjacent
    pointDetailDialogSupportsCustomFieldDisplayNames
    mainWindowAppliesAutoRefreshSettingsFromConfig
    mainWindowDropsStaleFilterOptionsResults
    mainWindowRejectsInvalidTimeRange
    repositoryFactoryFallsBackToMockWhenMySqlUnavailable
    repositoryFactoryKeepsMySqlRepositoryWhenFallbackDisabled
    exportServiceCreatesSummaryCsv
    exportServiceCreatesPointCsv
    exportServiceReturnsRecentExportFiles
    exportServiceCreatesReportBundle
    exportServiceRemovesExportFiles
    runtimeDiagnosticsCollectsSnapshot
    resultToolbarEmitsPageJumpSignal
    resultToolbarRefreshesPageState
    resultToolbarEmitsExportPanelSignal
    exportPanelDialogSupportsFilteringAndPaging
    summaryPageDropsStaleQueryResults
    badStatPageDropsStaleQueryResults
    boardRecordPageDropsStaleQueryResults
    pointRecordPageDropsStaleQueryResults
    summaryPageEmitsBoardDrillDownSignal
    boardRecordPageEmitsPointDrillDownSignal
    badStatPageEmitsPointDrillDownForBadPoint
    badStatPageEmitsPointDrillDownForGrade
    summaryPageShowsErrorWhenRepositoryQueryFails
    mainWindowUsesFallbackRepositoryWhenMySqlBuildFails
    mockRepositoryProvidesFilterOptions
    dashboardWidgetEmbedsInsideHostContainer
    dashboardWidgetUsesCurrentPageSizeHintWhenSwitching
    dashboardWidgetSwitchingToSummaryKeepsStableHeight
    dashboardWidgetShowsPageSpecificInsights
)

set(LASERSPC_MYSQL_TESTS
    mysqlRepositoryProvidesExpectedFilterOptions
    mysqlConnectionUsesUtcSession
    mysqlSummaryMetricsMatchSeedData
    mysqlBadStatisticsMatchSeedData
    mysqlPointRepositorySupportsFilteringAndSorting
    mysqlWriteRepositoryUpsertsPointRecordIdempotently
    mysqlWriteRepositoryReplacesInspectionBatchWithMultiplePoints
    dataCleanupServiceRemovesSeedRowsFromRealMySql
    dataCleanupServiceRemovesOldProductionRowsFromRealMySql
    summaryRepositorySupportsPagination
    boardRepositorySupportsSorting
    appConfigServiceDefaultsToNightTheme
    summaryPageLoadsRealMySqlData
    pointRecordPageLoadsRealMySqlData
    mainWindowLoadsRealMySqlDataAndRefreshesCurrentPage
    filterPanelRemovesOptionalChipAndKeepsTimeRequired
    settingsDialogEditsLanguageAndThemeInOtherSettingsTab
    dashboardWidgetAutoRefreshAdvancesFilterEndTime
)

add_test(NAME LaserSpcTestsFast COMMAND LaserSpcTests ${LASERSPC_FAST_TESTS})
set_tests_properties(LaserSpcTestsFast PROPERTIES
    LABELS "fast"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_test(NAME LaserSpcMySqlSmoke COMMAND LaserSpcTests mysqlEnvironmentSmokeTest)
set_tests_properties(LaserSpcMySqlSmoke PROPERTIES
    LABELS "mysql;integration"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;LASERSPC_REQUIRE_MYSQL=1"
)

add_test(NAME LaserSpcTestsMySqlIntegration COMMAND LaserSpcTests ${LASERSPC_MYSQL_TESTS})
set_tests_properties(LaserSpcTestsMySqlIntegration PROPERTIES
    LABELS "mysql;integration"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;LASERSPC_REQUIRE_MYSQL=1"
)

add_custom_target(check-fast
    COMMAND ${CMAKE_CTEST_COMMAND} -L fast --output-on-failure
    DEPENDS LaserSpcTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(check-mysql
    COMMAND ${CMAKE_CTEST_COMMAND} -L mysql --output-on-failure
    DEPENDS LaserSpcTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

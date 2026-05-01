#include <QApplication>
#include <algorithm>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif
#include <QStringList>
#include <QTextStream>

#include "infrastructure/AppConfigService.h"
#include "infrastructure/DatabaseConnection.h"

namespace {

struct MigrationScript {
    int version = 0;
    QString fileName;
    QString filePath;
};

struct InitOutcome {
    bool success = false;
    QString summary;
    QString detail;
};

QString quoteIdentifier(const QString& value) {
    QString escaped = value;
    escaped.replace("`", "``");
    return "`" + escaped + "`";
}

QString quoteLiteral(const QString& value) {
    QString escaped = value;
    escaped.replace("\\", "\\\\");
    escaped.replace("'", "''");
    return "'" + escaped + "'";
}

QString envOrDefault(const char* name, const QString& fallback) {
    const QString value = qEnvironmentVariable(name);
    return value.isEmpty() ? fallback : value;
}

bool envFlag(const char* name, bool fallback) {
    const QString value = qEnvironmentVariable(name).trimmed().toLower();
    if (value.isEmpty()) {
        return fallback;
    }
    return value == QObject::tr("1") || value == QObject::tr("true") || value == QObject::tr("yes");
}

QString resolveSqlDir(const QDir& startDir) {
    QDir current = startDir;
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

QStringList splitStatements(const QString& sql) {
    QStringList statements;
    QString current;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;

    for (QChar ch : sql) {
        if (ch == '\'' && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
        } else if (ch == '"' && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
        }

        if (ch == ';' && !inSingleQuote && !inDoubleQuote) {
            const QString trimmed = current.trimmed();
            if (!trimmed.isEmpty()) {
                statements.append(trimmed);
            }
            current.clear();
            continue;
        }

        current.append(ch);
    }

    const QString trimmed = current.trimmed();
    if (!trimmed.isEmpty()) {
        statements.append(trimmed);
    }

    return statements;
}

bool executeSqlText(QSqlDatabase& database,
                    const QString& sqlText,
                    const QString& sourceLabel,
                    QString* errorMessage) {
    for (const QString& statement : splitStatements(sqlText)) {
        QSqlQuery query(database);
        if (!query.exec(statement)) {
            if (errorMessage != nullptr) {
                *errorMessage = QString("SQL failed in %1: %2\nStatement: %3")
                                    .arg(sourceLabel, query.lastError().text(), statement);
            }
            return false;
        }
    }
    return true;
}

bool ensureTargetDatabaseAndAccess(QSqlDatabase& adminDatabase,
                                   const LaserSpc::Infrastructure::DatabaseSettings& adminSettings,
                                   const LaserSpc::Infrastructure::DatabaseSettings& appSettings,
                                   QString* errorMessage) {
    const QString createDatabaseSql =
        QString("CREATE DATABASE IF NOT EXISTS %1 CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci")
            .arg(quoteIdentifier(appSettings.databaseName));
    if (!executeSqlText(adminDatabase,
                        createDatabaseSql + ";",
                        QObject::tr("dynamic database bootstrap"),
                        errorMessage)) {
        return false;
    }

    const QString appUser = appSettings.userName.trimmed();
    const bool reuseAdminAccount =
        !appUser.isEmpty() &&
        appUser == adminSettings.userName &&
        appSettings.password == adminSettings.password;
    if (!appUser.isEmpty() && !reuseAdminAccount) {
        const QStringList hosts{QStringLiteral("localhost"), QStringLiteral("%")};
        for (const QString& host : hosts) {
            QSqlQuery userQuery(adminDatabase);
            userQuery.prepare("SELECT COUNT(*) FROM mysql.user WHERE User = ? AND Host = ?");
            userQuery.addBindValue(appUser);
            userQuery.addBindValue(host);
            if (!userQuery.exec()) {
                if (errorMessage != nullptr) {
                    *errorMessage = QString("Failed to query mysql.user for %1@%2: %3")
                                        .arg(appUser, host, userQuery.lastError().text());
                }
                return false;
            }
            userQuery.next();
            const bool userExists = userQuery.value(0).toInt() > 0;
            if (!userExists) {
                const QString createUserSql =
                    QString("CREATE USER %1@%2 IDENTIFIED BY %3")
                        .arg(quoteLiteral(appUser), quoteLiteral(host), quoteLiteral(appSettings.password));
                if (!executeSqlText(adminDatabase,
                                    createUserSql + ";",
                                    QObject::tr("dynamic database bootstrap"),
                                    errorMessage)) {
                    return false;
                }
            }

            const QString grantSql =
                QString("GRANT ALL PRIVILEGES ON %1.* TO %2@%3")
                    .arg(quoteIdentifier(appSettings.databaseName), quoteLiteral(appUser), quoteLiteral(host));
            if (!executeSqlText(adminDatabase,
                                grantSql + ";",
                                QObject::tr("dynamic database bootstrap"),
                                errorMessage)) {
                return false;
            }
        }

        if (!executeSqlText(adminDatabase,
                            QStringLiteral("FLUSH PRIVILEGES;"),
                            QObject::tr("dynamic database bootstrap"),
                            errorMessage)) {
            return false;
        }
    }
    return true;
}

bool executeSqlFile(QSqlDatabase& database, const QString& filePath, QString* errorMessage) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = QString("Failed to open SQL file: %1").arg(filePath);
        }
        return false;
    }

    QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    stream.setCodec("UTF-8");
#else
    stream.setEncoding(QStringConverter::Utf8);
#endif
    return executeSqlText(database, stream.readAll(), filePath, errorMessage);
}

bool executeScalarInt(QSqlDatabase& database, const QString& sql, int* value, QString* errorMessage) {
    QSqlQuery query(database);
    if (!query.exec(sql)) {
        if (errorMessage != nullptr) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    if (!query.next()) {
        if (value != nullptr) {
            *value = 0;
        }
        return true;
    }
    if (value != nullptr) {
        *value = query.value(0).toInt();
    }
    return true;
}

bool tableExists(QSqlDatabase& database, const QString& tableName, QString* errorMessage) {
    QSqlQuery query(database);
    query.prepare(
        "SELECT COUNT(*) FROM information_schema.tables "
        "WHERE table_schema = DATABASE() AND table_name = ?");
    query.addBindValue(tableName);
    if (!query.exec()) {
        if (errorMessage != nullptr) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    query.next();
    return query.value(0).toInt() > 0;
}

bool executeIfExists(QSqlDatabase& database,
                     const QString& filePath,
                     bool* executed,
                     QString* errorMessage) {
    if (executed != nullptr) {
        *executed = false;
    }

    if (!QFileInfo::exists(filePath)) {
        return true;
    }

    if (!executeSqlFile(database, filePath, errorMessage)) {
        return false;
    }

    if (executed != nullptr) {
        *executed = true;
    }
    return true;
}

bool ensureMigrationTables(QSqlDatabase& database, QString* errorMessage) {
    const QString sql =
        "CREATE TABLE IF NOT EXISTS schema_migrations ("
        "version INT NOT NULL PRIMARY KEY,"
        "script_name VARCHAR(255) NOT NULL,"
        "applied_at DATETIME NOT NULL,"
        "status VARCHAR(16) NOT NULL DEFAULT 'applied'"
        ");"
        "CREATE TABLE IF NOT EXISTS schema_version ("
        "id TINYINT NOT NULL PRIMARY KEY,"
        "current_version INT NOT NULL,"
        "updated_at DATETIME NOT NULL"
        ");";
    return executeSqlText(database, sql, QObject::tr("schema metadata"), errorMessage);
}

QList<MigrationScript> discoverMigrations(const QString& sqlDir, QString* errorMessage) {
    QList<MigrationScript> migrations;
    const QDir migrationsDir(QDir(sqlDir).filePath("migrations"));
    if (!migrationsDir.exists()) {
        if (errorMessage != nullptr) {
            *errorMessage = QString("Migration directory does not exist: %1").arg(migrationsDir.absolutePath());
        }
        return migrations;
    }

    const QFileInfoList files = migrationsDir.entryInfoList(QStringList() << "V*.sql", QDir::Files, QDir::Name);
    const QRegularExpression pattern(QObject::tr("^V(\\d+)__.+\\.sql$"), QRegularExpression::CaseInsensitiveOption);
    for (const QFileInfo& fileInfo : files) {
        const auto match = pattern.match(fileInfo.fileName());
        if (!match.hasMatch()) {
            continue;
        }

        MigrationScript script;
        script.version = match.captured(1).toInt();
        script.fileName = fileInfo.fileName();
        script.filePath = fileInfo.absoluteFilePath();
        migrations.append(script);
    }

    std::sort(migrations.begin(), migrations.end(), [](const MigrationScript& left, const MigrationScript& right) {
        return left.version < right.version;
    });
    return migrations;
}

bool migrationApplied(QSqlDatabase& database, int version, bool* applied, QString* errorMessage) {
    QSqlQuery query(database);
    query.prepare("SELECT COUNT(*) FROM schema_migrations WHERE version = ?");
    query.addBindValue(version);
    if (!query.exec()) {
        if (errorMessage != nullptr) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    query.next();
    if (applied != nullptr) {
        *applied = query.value(0).toInt() > 0;
    }
    return true;
}

bool recordMigration(QSqlDatabase& database, const MigrationScript& script, QString* errorMessage) {
    QSqlQuery query(database);
    query.prepare(
        "INSERT INTO schema_migrations (version, script_name, applied_at, status) "
        "VALUES (?, ?, ?, 'applied')");
    query.addBindValue(script.version);
    query.addBindValue(script.fileName);
    query.addBindValue(QDateTime::currentDateTime());
    if (!query.exec()) {
        if (errorMessage != nullptr) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }

    QSqlQuery versionQuery(database);
    versionQuery.prepare(
        "INSERT INTO schema_version (id, current_version, updated_at) VALUES (1, ?, ?) "
        "ON DUPLICATE KEY UPDATE current_version = VALUES(current_version), updated_at = VALUES(updated_at)");
    versionQuery.addBindValue(script.version);
    versionQuery.addBindValue(QDateTime::currentDateTime());
    if (!versionQuery.exec()) {
        if (errorMessage != nullptr) {
            *errorMessage = versionQuery.lastError().text();
        }
        return false;
    }
    return true;
}

bool applyMigrations(QSqlDatabase& database,
                     const QList<MigrationScript>& migrations,
                     int* appliedCount,
                     int* previousVersion,
                     int* currentVersion,
                     QString* errorMessage) {
    int beforeVersion = 0;
    if (!executeScalarInt(database,
                          "SELECT COALESCE(MAX(version), 0) FROM schema_migrations",
                          &beforeVersion,
                          errorMessage)) {
        return false;
    }

    int count = 0;
    for (const MigrationScript& script : migrations) {
        bool applied = false;
        if (!migrationApplied(database, script.version, &applied, errorMessage)) {
            return false;
        }
        if (applied) {
            continue;
        }

        if (!executeSqlFile(database, script.filePath, errorMessage)) {
            return false;
        }
        if (!recordMigration(database, script, errorMessage)) {
            return false;
        }
        ++count;
    }

    int afterVersion = 0;
    if (!executeScalarInt(database,
                          "SELECT COALESCE(MAX(version), 0) FROM schema_migrations",
                          &afterVersion,
                          errorMessage)) {
        return false;
    }

    if (appliedCount != nullptr) {
        *appliedCount = count;
    }
    if (previousVersion != nullptr) {
        *previousVersion = beforeVersion;
    }
    if (currentVersion != nullptr) {
        *currentVersion = afterVersion;
    }
    return true;
}

bool maybeSeedDatabase(QSqlDatabase& database,
                       const QString& sqlDir,
                       bool seedEnabled,
                       bool* seeded,
                       QString* errorMessage) {
    if (seeded != nullptr) {
        *seeded = false;
    }
    if (!seedEnabled) {
        return true;
    }

    int boardCount = 0;
    int pointCount = 0;
    const bool hasBoardTable = tableExists(database, QObject::tr("board_records"), errorMessage);
    if (!hasBoardTable && errorMessage != nullptr && !errorMessage->isEmpty()) {
        return false;
    }
    const bool hasPointTable = tableExists(database, QObject::tr("point_records"), errorMessage);
    if (!hasPointTable && errorMessage != nullptr && !errorMessage->isEmpty()) {
        return false;
    }

    if (hasBoardTable &&
        !executeScalarInt(database, "SELECT COUNT(*) FROM board_records", &boardCount, errorMessage)) {
        return false;
    }
    if (hasPointTable &&
        !executeScalarInt(database, "SELECT COUNT(*) FROM point_records", &pointCount, errorMessage)) {
        return false;
    }

    if (boardCount == 0 && pointCount == 0) {
        const QString seedFile = QDir(sqlDir).filePath("seed_data.sql");
        if (!executeSqlFile(database, seedFile, errorMessage)) {
            return false;
        }
        if (seeded != nullptr) {
            *seeded = true;
        }
        return true;
    }

    if (boardCount > 0 && pointCount == 0) {
        bool repaired = false;
        const QString repairFile = QDir(sqlDir).filePath("seed_point_records.sql");
        if (!executeIfExists(database, repairFile, &repaired, errorMessage)) {
            return false;
        }

        if (!executeScalarInt(database, "SELECT COUNT(*) FROM point_records", &pointCount, errorMessage)) {
            return false;
        }

        if (pointCount == 0) {
            if (errorMessage != nullptr) {
                *errorMessage =
                    QString("Database seed state is inconsistent: board_records=%1, point_records=%2. "
                            "No point seed rows were restored because existing board data does not match "
                            "the bundled sample dataset.").arg(boardCount).arg(pointCount);
            }
            return false;
        }

        if (seeded != nullptr) {
            *seeded = repaired;
        }
        return true;
    }

    if (boardCount == 0 && pointCount > 0) {
        if (errorMessage != nullptr) {
            *errorMessage =
                QString("Database seed state is inconsistent: board_records=%1, point_records=%2. "
                        "Refusing to run the full seed because it would overwrite existing point data.")
                    .arg(boardCount)
                    .arg(pointCount);
        }
        return false;
    }

    return true;
}

InitOutcome makeOutcome(bool success, const QString& summary, const QString& detail = QString()) {
    InitOutcome outcome;
    outcome.success = success;
    outcome.summary = summary;
    outcome.detail = detail;
    return outcome;
}

void presentOutcome(const InitOutcome& outcome) {
    QTextStream stream(outcome.success ? stdout : stderr);
    stream << outcome.summary << "\n";
    if (!outcome.detail.trimmed().isEmpty()) {
        stream << outcome.detail << "\n";
    }
    stream.flush();

    const QString platformName = QGuiApplication::platformName().trimmed().toLower();
    const bool noDialog =
        envFlag("LASERSPC_DBINIT_NO_DIALOG", false) ||
        platformName == QStringLiteral("offscreen") ||
        platformName == QStringLiteral("minimal");
    if (noDialog) {
        return;
    }

    QMessageBox box;
    box.setIcon(outcome.success ? QMessageBox::Information : QMessageBox::Critical);
    box.setWindowTitle(outcome.success ? QObject::tr("数据库初始化成功") : QObject::tr("数据库初始化失败"));
    box.setText(outcome.summary);
    if (!outcome.detail.trimmed().isEmpty()) {
        box.setDetailedText(outcome.detail);
    }
    box.exec();
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("LaserSpcDbInit"));

    const QDir appDir(QCoreApplication::applicationDirPath());
    const QString sqlDir = resolveSqlDir(appDir);
    if (sqlDir.isEmpty()) {
        presentOutcome(makeOutcome(false,
                                   QObject::tr("未找到 SQL 初始化目录。"),
                                   QObject::tr("步骤：定位 sql/mysql\n起始目录：%1").arg(appDir.path())));
        return 1;
    }

    LaserSpc::Infrastructure::DatabaseSettings adminSettings;
    adminSettings.host = envOrDefault("LASERSPC_DB_HOST", "127.0.0.1");
    adminSettings.port = qEnvironmentVariableIntValue("LASERSPC_DB_PORT");
    if (adminSettings.port <= 0) {
        adminSettings.port = 3306;
    }
    adminSettings.databaseName = envOrDefault("LASERSPC_ADMIN_DB", "mysql");
    adminSettings.userName = envOrDefault("LASERSPC_ADMIN_DB_USER", "root");
    adminSettings.password = envOrDefault("LASERSPC_ADMIN_DB_PASSWORD", QString("zjh123456"));
    adminSettings.connectOptions = envOrDefault("LASERSPC_DB_CONNECT_OPTIONS",
                                                LaserSpc::Infrastructure::defaultMySqlConnectOptions());

    auto adminLease = LaserSpc::Infrastructure::DatabaseConnection::acquireMySql(adminSettings);
    if (!adminLease.isOpen()) {
        presentOutcome(makeOutcome(false,
                                   QObject::tr("无法连接 MySQL 管理账号。"),
                                   QObject::tr("步骤：连接管理数据库\n目标：%1:%2/%3\n错误：%4")
                                       .arg(adminSettings.host)
                                       .arg(adminSettings.port)
                                       .arg(adminSettings.databaseName)
                                       .arg(adminLease.lastError())));
        return 1;
    }

    QString errorMessage;
    LaserSpc::Infrastructure::DatabaseSettings appSettings = adminSettings;
    appSettings.databaseName = envOrDefault("LASERSPC_DB_NAME", "laser_spc");
    appSettings.userName = envOrDefault("LASERSPC_DB_USER", "laserspc");
    appSettings.password = envOrDefault("LASERSPC_DB_PASSWORD", QString("LaserSpc#2026"));

    QSqlDatabase adminDatabase = adminLease.database();
    if (!ensureTargetDatabaseAndAccess(adminDatabase, adminSettings, appSettings, &errorMessage)) {
        presentOutcome(makeOutcome(false,
                                   QObject::tr("数据库、用户或权限初始化失败。"),
                                   QObject::tr("步骤：创建数据库 / 创建用户 / 授权\n数据库：%1\n业务用户：%2\n详情：%3")
                                       .arg(appSettings.databaseName)
                                       .arg(appSettings.userName)
                                       .arg(errorMessage)));
        return 1;
    }
    auto appLease = LaserSpc::Infrastructure::DatabaseConnection::acquireMySql(appSettings);
    if (!appLease.isOpen()) {
        presentOutcome(makeOutcome(false,
                                   QObject::tr("目标数据库连接失败。"),
                                   QObject::tr("步骤：使用业务账号验证连接\n目标：%1:%2/%3\n用户：%4\n错误：%5")
                                       .arg(appSettings.host)
                                       .arg(appSettings.port)
                                       .arg(appSettings.databaseName)
                                       .arg(appSettings.userName)
                                       .arg(appLease.lastError())));
        return 1;
    }

    QSqlDatabase database = appLease.database();
    if (!ensureMigrationTables(database, &errorMessage)) {
        presentOutcome(makeOutcome(false,
                                   QObject::tr("迁移元数据表初始化失败。"),
                                   QObject::tr("步骤：创建 schema_migrations / schema_version\n详情：%1").arg(errorMessage)));
        return 1;
    }

    const QList<MigrationScript> migrations = discoverMigrations(sqlDir, &errorMessage);
    if (!errorMessage.isEmpty()) {
        presentOutcome(makeOutcome(false,
                                   QObject::tr("迁移脚本扫描失败。"),
                                   QObject::tr("步骤：扫描 migrations 目录\n目录：%1\n详情：%2")
                                       .arg(QDir(sqlDir).filePath("migrations"))
                                       .arg(errorMessage)));
        return 1;
    }

    int appliedCount = 0;
    int previousVersion = 0;
    int currentVersion = 0;
    if (!applyMigrations(database, migrations, &appliedCount, &previousVersion, &currentVersion, &errorMessage)) {
        presentOutcome(makeOutcome(false,
                                   QObject::tr("迁移脚本执行失败。"),
                                   QObject::tr("步骤：执行 SQL migration\n详情：%1").arg(errorMessage)));
        return 1;
    }

    bool seeded = false;
    if (!maybeSeedDatabase(database, sqlDir, envFlag("LASERSPC_DB_SEED", true), &seeded, &errorMessage)) {
        presentOutcome(makeOutcome(false,
                                   QObject::tr("种子数据初始化失败。"),
                                   QObject::tr("步骤：导入种子数据\n详情：%1").arg(errorMessage)));
        return 1;
    }

    presentOutcome(makeOutcome(true,
                               QObject::tr("数据库初始化完成。"),
                               QObject::tr("目标：%1:%2/%3\n业务用户：%4\nSchema 版本：%5 -> %6\n执行迁移：%7 个\n导入种子数据：%8")
                                   .arg(appSettings.host)
                                   .arg(appSettings.port)
                                   .arg(appSettings.databaseName)
                                   .arg(appSettings.userName)
                                   .arg(previousVersion)
                                   .arg(currentVersion)
                                   .arg(appliedCount)
                                   .arg(seeded ? QObject::tr("是") : QObject::tr("否"))));
    return 0;
}

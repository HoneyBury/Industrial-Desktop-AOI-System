#include "infrastructure/DatabaseConnection.h"

#include <QElapsedTimer>
#include <QHash>
#include <QMutexLocker>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QThread>
#include <QVector>

#include "infrastructure/Logger.h"

namespace LaserSpc::Infrastructure {

namespace {

struct ConnectionEntry {
    QString connectionName;
    DatabaseSettings settings;
    bool inUse = false;
};

QMutex& poolMutex() {
    static QMutex mutex;
    return mutex;
}

QHash<quintptr, QVector<ConnectionEntry>>& poolEntries() {
    static QHash<quintptr, QVector<ConnectionEntry>> entries;
    return entries;
}

bool sameSettings(const DatabaseSettings& left, const DatabaseSettings& right) {
    return left.host == right.host && left.port == right.port && left.databaseName == right.databaseName &&
           left.userName == right.userName && left.password == right.password &&
           left.connectOptions == right.connectOptions &&
           left.connectTimeoutSeconds == right.connectTimeoutSeconds &&
           left.readTimeoutSeconds == right.readTimeoutSeconds;
}

QString defaultSslCompatOptions() {
    return QStringLiteral("MYSQL_OPT_SSL_ENFORCE=0;MYSQL_OPT_SSL_VERIFY_SERVER_CERT=0");
}

QString effectiveConnectOptions(const DatabaseSettings& settings) {
    QStringList options;
    if (!settings.connectOptions.trimmed().isEmpty()) {
        options = settings.connectOptions.split(';', Qt::SkipEmptyParts);
    } else {
        options = defaultSslCompatOptions().split(';', Qt::SkipEmptyParts);
    }

    auto containsOption = [&options](const QString& prefix) {
        for (const QString& option : options) {
            if (option.trimmed().startsWith(prefix, Qt::CaseInsensitive)) {
                return true;
            }
        }
        return false;
    };

    if (settings.connectTimeoutSeconds > 0 && !containsOption("MYSQL_OPT_CONNECT_TIMEOUT")) {
        options.append(QString("MYSQL_OPT_CONNECT_TIMEOUT=%1").arg(settings.connectTimeoutSeconds));
    }
    if (settings.readTimeoutSeconds > 0 && !containsOption("MYSQL_OPT_READ_TIMEOUT")) {
        options.append(QString("MYSQL_OPT_READ_TIMEOUT=%1").arg(settings.readTimeoutSeconds));
    }
    return options.join(';');
}

QString buildConnectionName(quintptr threadId, int index) {
    const QString base = QString("LaserSpcMySqlConnection_%1").arg(threadId);
    return index == 0 ? base : QString("%1_%2").arg(base).arg(index);
}

QString connectionTarget(const DatabaseSettings& settings) {
    return QString("%1:%2/%3 user=%4")
        .arg(settings.host, QString::number(settings.port), settings.databaseName, settings.userName);
}

QString classifyOpenFailure(const QString& errorText, bool driverAvailable) {
    const QString normalized = errorText.trimmed().toLower();
    if (!driverAvailable || normalized.contains("driver not loaded") || normalized.contains("qsqldatabase: qmysql driver not loaded")) {
        return QStringLiteral("driver_unavailable");
    }
    if (normalized.contains("access denied")) {
        return QStringLiteral("authentication_failed");
    }
    if (normalized.contains("unknown database")) {
        return QStringLiteral("database_not_found");
    }
    if (normalized.contains("can't connect") || normalized.contains("unable to connect") ||
        normalized.contains("connection refused") || normalized.contains("host is blocked")) {
        return QStringLiteral("connection_failed");
    }
    if (normalized.contains("tls/ssl") || normalized.contains("ssl is required") || normalized.contains("server does not support it")) {
        return QStringLiteral("ssl_mismatch");
    }
    if (normalized.contains("timeout")) {
        return QStringLiteral("timeout");
    }
    return QStringLiteral("open_failed");
}

QSqlDatabase configureDatabase(const QString& connectionName, const DatabaseSettings& settings) {
    QSqlDatabase database = QSqlDatabase::contains(connectionName)
                                ? QSqlDatabase::database(connectionName)
                                : QSqlDatabase::addDatabase("QMYSQL", connectionName);

    if (database.isOpen()) {
        database.close();
    }
    database.setHostName(settings.host);
    database.setPort(settings.port);
    database.setDatabaseName(settings.databaseName);
    database.setUserName(settings.userName);
    database.setPassword(settings.password);
    database.setConnectOptions(effectiveConnectOptions(settings));
    return database;
}

bool ensureOpen(QSqlDatabase& database, const DatabaseSettings& settings) {
    if (database.isOpen()) {
        return true;
    }

    const QString target = connectionTarget(settings);
    const bool driverAvailable = QSqlDatabase::drivers().contains(QStringLiteral("QMYSQL"));
    if (!driverAvailable) {
        Logger::error("MySQL open aborted | reason=driver_unavailable | driver=QMYSQL | target=" + target +
                      " | availableDrivers=" + QSqlDatabase::drivers().join(", "));
        return false;
    }
    if (settings.host.trimmed().isEmpty() || settings.databaseName.trimmed().isEmpty() || settings.userName.trimmed().isEmpty()) {
        Logger::error("MySQL open aborted | reason=invalid_settings | target=" + target +
                      " | host/database/user must not be empty");
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    if (!database.open()) {
        const QString firstError = database.lastError().text();
        const QString normalized = firstError.trimmed().toLower();
        const bool likelySslMismatch =
            normalized.contains("tls/ssl") ||
            normalized.contains("ssl is required") ||
            normalized.contains("server does not support it");

        if (likelySslMismatch) {
            QStringList options = database.connectOptions().split(';', Qt::SkipEmptyParts);
            auto containsOption = [&options](const QString& prefix) {
                for (const QString& option : options) {
                    if (option.trimmed().startsWith(prefix, Qt::CaseInsensitive)) {
                        return true;
                    }
                }
                return false;
            };

            if (!containsOption("MYSQL_OPT_SSL_ENFORCE")) {
                options.append(QStringLiteral("MYSQL_OPT_SSL_ENFORCE=0"));
            }
            if (!containsOption("MYSQL_OPT_SSL_VERIFY_SERVER_CERT")) {
                options.append(QStringLiteral("MYSQL_OPT_SSL_VERIFY_SERVER_CERT=0"));
            }

            database.setConnectOptions(options.join(';'));
            if (database.open()) {
                Logger::warn("MySQL open retry succeeded | reason=ssl_fallback | target=" + target +
                             " | firstError=" + firstError +
                             " | elapsedMs=" + QString::number(timer.elapsed()));
            } else {
                Logger::error("MySQL open failed | reason=" +
                              classifyOpenFailure(database.lastError().text(), driverAvailable) +
                              " | target=" + target +
                              " | firstError=" + firstError +
                              " | retryError=" + database.lastError().text() +
                              " | elapsedMs=" + QString::number(timer.elapsed()));
                return false;
            }
        } else {
            Logger::error("MySQL open failed | reason=" + classifyOpenFailure(firstError, driverAvailable) +
                          " | target=" + target +
                          " | error=" + firstError +
                          " | elapsedMs=" + QString::number(timer.elapsed()));
            return false;
        }
    }

    QSqlQuery utcQuery(database);
    if (!utcQuery.exec(QStringLiteral("SET time_zone = '+00:00'"))) {
        Logger::error("MySQL session init failed | action=set_time_zone | target=" + target +
                      " | error=" + utcQuery.lastError().text());
        database.close();
        return false;
    }

    Logger::info("MySQL connection opened | target=" + target +
                 " | connectionName=" + database.connectionName());
    Logger::perf("DatabaseConnection.acquireMySql", timer.elapsed(), target);
    return true;
}

}  // namespace

QString DatabaseConnection::connectionName() {
    return buildConnectionName(reinterpret_cast<quintptr>(QThread::currentThreadId()), 0);
}

DatabaseConnection::ConnectionLease::ConnectionLease(QString connectionName, QSqlDatabase database)
    : m_connectionName(std::move(connectionName)), m_database(std::move(database)) {}

DatabaseConnection::ConnectionLease::ConnectionLease(ConnectionLease&& other) noexcept
    : m_connectionName(std::move(other.m_connectionName)), m_database(std::move(other.m_database)) {}

DatabaseConnection::ConnectionLease& DatabaseConnection::ConnectionLease::operator=(ConnectionLease&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    release();
    m_connectionName = std::move(other.m_connectionName);
    m_database = std::move(other.m_database);
    return *this;
}

DatabaseConnection::ConnectionLease::~ConnectionLease() {
    release();
}

bool DatabaseConnection::ConnectionLease::isOpen() const {
    return m_database.isValid() && m_database.isOpen();
}

QSqlDatabase DatabaseConnection::ConnectionLease::database() const {
    return m_database;
}

QString DatabaseConnection::ConnectionLease::lastError() const {
    return m_database.lastError().text();
}

DatabaseConnection::ConnectionLease::operator bool() const {
    return isOpen();
}

void DatabaseConnection::ConnectionLease::release() {
    if (m_connectionName.isEmpty()) {
        return;
    }

    DatabaseConnection::releaseConnection(m_connectionName);
    m_connectionName.clear();
    m_database = QSqlDatabase();
}

DatabaseConnection::ConnectionLease DatabaseConnection::acquireMySql(const DatabaseSettings& settings) {
    QMutexLocker locker(&poolMutex());
    const quintptr threadId = reinterpret_cast<quintptr>(QThread::currentThreadId());
    auto& entries = poolEntries()[threadId];

    for (ConnectionEntry& entry : entries) {
        if (entry.inUse || !sameSettings(entry.settings, settings)) {
            continue;
        }

        QSqlDatabase database = configureDatabase(entry.connectionName, settings);
        if (!ensureOpen(database, settings)) {
            return ConnectionLease(entry.connectionName, database);
        }

        entry.inUse = true;
        return ConnectionLease(entry.connectionName, database);
    }

    const QString newConnectionName = buildConnectionName(threadId, entries.size());
    QSqlDatabase database = configureDatabase(newConnectionName, settings);
    if (!ensureOpen(database, settings)) {
        database = QSqlDatabase();
        if (QSqlDatabase::contains(newConnectionName)) {
            QSqlDatabase::removeDatabase(newConnectionName);
        }
        return ConnectionLease(newConnectionName, database);
    }

    entries.append(ConnectionEntry{newConnectionName, settings, true});
    return ConnectionLease(newConnectionName, database);
}

void DatabaseConnection::resetPool() {
    QMutexLocker locker(&poolMutex());
    auto& entriesByThread = poolEntries();
    const quintptr currentThreadId = reinterpret_cast<quintptr>(QThread::currentThreadId());
    auto it = entriesByThread.find(currentThreadId);
    if (it == entriesByThread.end()) {
        return;
    }

    for (const ConnectionEntry& entry : it.value()) {
        if (!QSqlDatabase::contains(entry.connectionName)) {
            continue;
        }

        {
            QSqlDatabase database = QSqlDatabase::database(entry.connectionName, false);
            if (database.isValid()) {
                database.close();
            }
        }
        QSqlDatabase::removeDatabase(entry.connectionName);
    }
    entriesByThread.erase(it);
}

void DatabaseConnection::releaseConnection(const QString& connectionName) {
    QMutexLocker locker(&poolMutex());
    auto& entriesByThread = poolEntries();
    for (auto it = entriesByThread.begin(); it != entriesByThread.end(); ++it) {
        for (ConnectionEntry& entry : it.value()) {
            if (entry.connectionName == connectionName) {
                entry.inUse = false;
                return;
            }
        }
    }
}

}  // namespace LaserSpc::Infrastructure

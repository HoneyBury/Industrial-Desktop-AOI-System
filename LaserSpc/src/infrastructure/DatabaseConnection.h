#pragma once

#include <QMutex>
#include <QString>
#include <QSqlDatabase>

namespace LaserSpc::Infrastructure {

struct DatabaseSettings {
    QString host = "127.0.0.1";
    int port = 3306;
    QString databaseName = "laser_spc";
    QString userName = "laserspc";
    QString password = "LaserSpc#2026";
    QString connectOptions;
    int connectTimeoutSeconds = 5;
    int readTimeoutSeconds = 15;
};

class DatabaseConnection {
public:
    class ConnectionLease {
    public:
        ConnectionLease() = default;
        ConnectionLease(const ConnectionLease&) = delete;
        ConnectionLease& operator=(const ConnectionLease&) = delete;
        ConnectionLease(ConnectionLease&& other) noexcept;
        ConnectionLease& operator=(ConnectionLease&& other) noexcept;
        ~ConnectionLease();

        bool isOpen() const;
        QSqlDatabase database() const;
        QString lastError() const;
        explicit operator bool() const;

    private:
        friend class DatabaseConnection;

        ConnectionLease(QString connectionName, QSqlDatabase database);
        void release();

        QString m_connectionName;
        QSqlDatabase m_database;
    };

    static QString connectionName();
    static ConnectionLease acquireMySql(const DatabaseSettings& settings);
    static void resetPool();

private:
    static void releaseConnection(const QString& connectionName);
};

}  // namespace LaserSpc::Infrastructure

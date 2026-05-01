#include "infrastructure/MySqlTransactionExecutor.h"

#include <QSqlError>
#include <QSqlQuery>

#include "infrastructure/Logger.h"

namespace LaserSpc::Infrastructure {

bool MySqlTransactionExecutor::begin(QSqlDatabase& database, const QString& scope, QString* errorMessage) {
    return exec(database, QStringLiteral("START TRANSACTION"), scope, QStringLiteral("begin"), errorMessage);
}

bool MySqlTransactionExecutor::commit(QSqlDatabase& database, const QString& scope, QString* errorMessage) {
    return exec(database, QStringLiteral("COMMIT"), scope, QStringLiteral("commit"), errorMessage);
}

bool MySqlTransactionExecutor::rollback(QSqlDatabase& database, const QString& scope, QString* errorMessage) {
    return exec(database, QStringLiteral("ROLLBACK"), scope, QStringLiteral("rollback"), errorMessage);
}

bool MySqlTransactionExecutor::exec(QSqlDatabase& database,
                                    const QString& sql,
                                    const QString& scope,
                                    const QString& stage,
                                    QString* errorMessage) {
    QSqlQuery query(database);
    if (query.exec(sql)) {
        Logger::info(QString("%1.%2 success | target=%3").arg(scope, stage, database.connectionName()));
        return true;
    }

    const QString error = query.lastError().text();
    if (errorMessage != nullptr) {
        *errorMessage = error;
    }
    Logger::error(QString("%1.%2 failed | target=%3 | error=%4 | sql=%5")
                      .arg(scope, stage, database.connectionName(), error, sql));
    return false;
}

}  // namespace LaserSpc::Infrastructure

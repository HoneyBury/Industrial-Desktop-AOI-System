#pragma once

#include <QSqlDatabase>
#include <QString>

namespace LaserSpc::Infrastructure {

class MySqlTransactionExecutor {
public:
    static bool begin(QSqlDatabase& database, const QString& scope, QString* errorMessage = nullptr);
    static bool commit(QSqlDatabase& database, const QString& scope, QString* errorMessage = nullptr);
    static bool rollback(QSqlDatabase& database, const QString& scope, QString* errorMessage = nullptr);

private:
    static bool exec(QSqlDatabase& database,
                     const QString& sql,
                     const QString& scope,
                     const QString& stage,
                     QString* errorMessage);
};

}  // namespace LaserSpc::Infrastructure

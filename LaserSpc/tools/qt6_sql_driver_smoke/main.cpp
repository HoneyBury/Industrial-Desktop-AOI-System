#include <QCoreApplication>
#include <QSqlDatabase>
#include <QTextStream>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    QTextStream out(stdout);
    const QStringList drivers = QSqlDatabase::drivers();
    out << drivers.join(',') << Qt::endl;
    return drivers.contains(QObject::tr("QMYSQL")) ? 0 : 1;
}

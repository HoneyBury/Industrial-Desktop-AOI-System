#include "infrastructure/PointDetailJsonService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QSaveFile>
#include <QSet>
#include <QTextStream>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

#include "infrastructure/AppConfigService.h"
#include "infrastructure/Logger.h"

namespace {

void configureUtf8(QTextStream& stream) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    stream.setCodec("UTF-8");
#else
    stream.setEncoding(QStringConverter::Utf8);
#endif
}

QString safeSegment(const QString& value, const QString& fallback) {
    QString result = value.trimmed();
    if (result.isEmpty()) {
        result = fallback;
    }

    static const QString forbidden = "\\/:*?\"<>|";
    for (const QChar ch : forbidden) {
        result.replace(ch, '_');
    }
    return result;
}

QString existingAbsolutePath(const QString& candidate) {
    const QFileInfo fileInfo(candidate);
    return fileInfo.exists() ? fileInfo.absoluteFilePath() : QString();
}

const QString& algorithmPlanKeyValue() {
    static const QString value = QStringLiteral("算法规划");
    return value;
}

const QString& algorithmPlanAliasValue() {
    static const QString value = QStringLiteral("algorithmPlan");
    return value;
}

const QString& fieldDisplayNamesKeyValue() {
    static const QString value = QStringLiteral("fieldDisplayNames");
    return value;
}

QSet<QString> reservedDataKeys() {
    return {
        QStringLiteral("templateFilePath"),
        QStringLiteral("laserTemplatePath"),
        QStringLiteral("laserContent"),
        QStringLiteral("readCodeContent"),
        QStringLiteral("success"),
        QStringLiteral("programName"),
        QStringLiteral("startTime"),
        QStringLiteral("endTime"),
        fieldDisplayNamesKeyValue(),
        algorithmPlanKeyValue(),
        algorithmPlanAliasValue()
    };
}

QJsonArray algorithmPlanFromObject(const QJsonObject& object) {
    const QJsonValue value = object.value(algorithmPlanKeyValue());
    if (value.isArray()) {
        return value.toArray();
    }
    const QJsonValue aliasValue = object.value(algorithmPlanAliasValue());
    return aliasValue.isArray() ? aliasValue.toArray() : QJsonArray{};
}

}  // namespace

namespace LaserSpc::Infrastructure {

QString PointDetailJsonService::algorithmPlanKey() {
    return algorithmPlanKeyValue();
}

QString PointDetailJsonService::algorithmPlanAlias() {
    return algorithmPlanAliasValue();
}

QString PointDetailJsonService::fieldDisplayNamesKey() {
    return fieldDisplayNamesKeyValue();
}

QString PointDetailJsonService::defaultDetailDirectory() {
    const QString overridePath = qEnvironmentVariable("LASERSPC_POINT_DETAIL_DIR");
    if (!overridePath.isEmpty()) {
        QDir().mkpath(overridePath);
        return QDir(overridePath).absolutePath();
    }

    const QString configuredPath = AppConfigService().settings().pointDetailDirectory.trimmed();
    const QString directory = configuredPath.isEmpty()
                                  ? QDir(QCoreApplication::applicationDirPath()).filePath("point_details")
                                  : configuredPath;
    QDir().mkpath(directory);
    return QDir(directory).absolutePath();
}

QString PointDetailJsonService::buildDefaultFilePath(const LaserSpc::Domain::PointRecordRow& row,
                                                     const QString& baseDirectory) {
    const QString root = baseDirectory.isEmpty() ? defaultDetailDirectory() : baseDirectory;
    QDir dir(root);
    dir.mkpath(".");
    const QString fileName = QString("%1_%2_%3.json")
                                 .arg(safeSegment(row.boardCode, "board"))
                                 .arg(safeSegment(row.pointName, "point"))
                                 .arg(row.endTime.isValid() ? row.endTime.toString("yyyyMMdd_HHmmss")
                                                            : QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    return dir.filePath(fileName);
}

QString PointDetailJsonService::resolveDetailFilePath(const QString& filePath) {
    const QString trimmed = filePath.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    const QString directHit = existingAbsolutePath(trimmed);
    if (!directHit.isEmpty()) {
        return directHit;
    }

    const QDir appDir(QCoreApplication::applicationDirPath());
    const QStringList candidates{appDir.filePath(trimmed),
                                 appDir.filePath(QStringLiteral("../") + trimmed),
                                 appDir.filePath(QStringLiteral("../../") + trimmed),
                                 appDir.filePath(QStringLiteral("../../../") + trimmed),
                                 QDir::current().filePath(trimmed)};

    for (const QString& candidate : candidates) {
        const QString hit = existingAbsolutePath(candidate);
        if (!hit.isEmpty()) {
            return hit;
        }
    }

    return QFileInfo(appDir.filePath(trimmed)).absoluteFilePath();
}

bool PointDetailJsonService::saveDetail(const LaserSpc::Domain::PointDetailInfo& detail,
                                        const QString& filePath,
                                        QString* errorMessage) {
    if (filePath.trimmed().isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("详情 JSON 路径不能为空。");
        }
        Logger::error("PointDetailJson.save failed | reason=empty_path");
        return false;
    }

    const QFileInfo fileInfo(filePath);
    if (!QDir().mkpath(fileInfo.dir().absolutePath())) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("无法创建详情 JSON 目录：%1").arg(fileInfo.dir().absolutePath());
        }
        Logger::error("PointDetailJson.save failed | reason=mkpath_failed | file=" + filePath +
                      " | directory=" + fileInfo.dir().absolutePath());
        return false;
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        Logger::error("PointDetailJson.save failed | reason=open_failed | file=" + filePath +
                      " | error=" + file.errorString());
        return false;
    }

    QTextStream stream(&file);
    configureUtf8(stream);
    stream << QJsonDocument(toJsonObject(detail)).toJson(QJsonDocument::Indented);
    if (!file.commit()) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        Logger::error("PointDetailJson.save failed | reason=commit_failed | file=" + filePath +
                      " | error=" + file.errorString());
        return false;
    }
    Logger::info("PointDetailJson.save success | file=" + filePath +
                 " | program=" + detail.programName +
                 " | start=" + detail.startTime.toString(Qt::ISODate) +
                 " | end=" + detail.endTime.toString(Qt::ISODate));
    return true;
}

bool PointDetailJsonService::loadDetail(const QString& filePath,
                                        LaserSpc::Domain::PointDetailInfo* detail,
                                        QString* errorMessage,
                                        QJsonObject* rawObject) {
    const QString resolvedPath = resolveDetailFilePath(filePath);
    QFile file(resolvedPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        Logger::error("PointDetailJson.load failed | reason=open_failed | requested=" + filePath +
                      " | resolved=" + resolvedPath +
                      " | error=" + file.errorString());
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = parseError.error == QJsonParseError::NoError
                                ? QObject::tr("详情 JSON 格式无效。")
                                : parseError.errorString();
        }
        Logger::error("PointDetailJson.load failed | reason=parse_failed | requested=" + filePath +
                      " | resolved=" + resolvedPath +
                      " | error=" + (parseError.error == QJsonParseError::NoError
                                         ? QObject::tr("invalid_json_object")
                                         : parseError.errorString()));
        return false;
    }

    const QJsonObject object = document.object();
    if (rawObject != nullptr) {
        *rawObject = object;
    }
    const bool ok = fromJsonObject(object, detail, errorMessage);
    if (!ok) {
        Logger::error("PointDetailJson.load failed | reason=invalid_payload | requested=" + filePath +
                      " | resolved=" + resolvedPath +
                      " | error=" + (errorMessage == nullptr ? QString() : *errorMessage));
        return false;
    }
    Logger::info("PointDetailJson.load success | requested=" + filePath +
                 " | resolved=" + resolvedPath);
    return true;
}

QJsonObject PointDetailJsonService::toJsonObject(const LaserSpc::Domain::PointDetailInfo& detail) {
    QJsonObject data = detail.extraFields;
    data.insert("templateFilePath", detail.laserTemplatePath);
    data.insert("laserTemplatePath", detail.laserTemplatePath);
    data.insert("laserContent", detail.laserContent);
    data.insert("readCodeContent", detail.readCodeContent);
    data.insert("success", detail.success);
    data.insert("programName", detail.programName);
    data.insert("startTime", detail.startTime.toString(Qt::ISODate));
    data.insert("endTime", detail.endTime.toString(Qt::ISODate));
    if (!detail.fieldDisplayNames.isEmpty()) {
        data.insert(fieldDisplayNamesKeyValue(), detail.fieldDisplayNames);
    }
    if (!detail.algorithmPlan.isEmpty()) {
        data.insert(algorithmPlanKeyValue(), detail.algorithmPlan);
    }

    QJsonObject root;
    root.insert("version", 2);
    root.insert("data", data);
    return root;
}

bool PointDetailJsonService::fromJsonObject(const QJsonObject& object,
                                            LaserSpc::Domain::PointDetailInfo* detail,
                                            QString* errorMessage) {
    if (detail == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QObject::tr("详情对象不能为空。");
        }
        Logger::error("PointDetailJson.fromJsonObject failed | reason=null_detail_object");
        return false;
    }

    const QJsonObject data = object.value("data").isObject() ? object.value("data").toObject() : object;
    detail->laserTemplatePath = data.value("templateFilePath").toString();
    if (detail->laserTemplatePath.trimmed().isEmpty()) {
        detail->laserTemplatePath = data.value("laserTemplatePath").toString();
    }
    detail->laserContent = data.value("laserContent").toString();
    detail->readCodeContent = data.value("readCodeContent").toString();
    detail->success = data.value("success").toBool();
    detail->programName = data.value("programName").toString();
    detail->startTime = QDateTime::fromString(data.value("startTime").toString(), Qt::ISODate);
    detail->endTime = QDateTime::fromString(data.value("endTime").toString(), Qt::ISODate);
    detail->fieldDisplayNames = data.value(fieldDisplayNamesKeyValue()).toObject();
    detail->algorithmPlan = algorithmPlanFromObject(data);
    detail->extraFields = data;
    const auto reservedKeys = reservedDataKeys();
    for (const QString& key : reservedKeys) {
        detail->extraFields.remove(key);
    }
    return true;
}

}  // namespace LaserSpc::Infrastructure

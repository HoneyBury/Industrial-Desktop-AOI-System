#include "ui/dialogs/PointDetailDialog.h"

#include <QDateTime>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QSet>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "infrastructure/PointDetailJsonService.h"
#include "ui/common/PointDetailFieldCatalog.h"

namespace LaserSpc::Ui {

namespace {

QStringList preferredObjectKeyOrder() {
    return {
        QStringLiteral("templateFilePath"),
        QStringLiteral("laserTemplatePath"),
        QStringLiteral("programName"),
        QStringLiteral("startTime"),
        QStringLiteral("endTime"),
        QStringLiteral("laserContent"),
        QStringLiteral("readCodeContent"),
        QStringLiteral("success"),
        QStringLiteral("算法规划"),
        QStringLiteral("algorithmPlan")
    };
}

}  // namespace

PointDetailDialog::PointDetailDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("点位详情"));
    resize(860, 620);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    m_pathLabel = new QLabel(this);
    m_pathLabel->setWordWrap(true);

    auto* summaryFrame = new QFrame(this);
    auto* summaryLayout = new QFormLayout(summaryFrame);
    summaryLayout->setContentsMargins(12, 12, 12, 12);
    summaryLayout->setHorizontalSpacing(16);
    summaryLayout->setVerticalSpacing(8);

    m_templatePathLabel = new QLabel(this);
    m_templatePathLabel->setWordWrap(true);
    m_programLabel = new QLabel(this);
    m_programLabel->setWordWrap(true);
    m_successLabel = new QLabel(this);
    m_successLabel->setWordWrap(true);
    m_startTimeLabel = new QLabel(this);
    m_startTimeLabel->setWordWrap(true);
    m_endTimeLabel = new QLabel(this);
    m_endTimeLabel->setWordWrap(true);
    m_laserContentLabel = new QLabel(this);
    m_laserContentLabel->setWordWrap(true);
    m_readCodeLabel = new QLabel(this);
    m_readCodeLabel->setWordWrap(true);
    m_algorithmPlanLabel = new QLabel(this);
    m_algorithmPlanLabel->setWordWrap(true);

    summaryLayout->addRow(tr("模板文件"), m_templatePathLabel);
    summaryLayout->addRow(tr("程序名"), m_programLabel);
    summaryLayout->addRow(tr("识别结果"), m_successLabel);
    summaryLayout->addRow(tr("开始时间"), m_startTimeLabel);
    summaryLayout->addRow(tr("结束时间"), m_endTimeLabel);
    summaryLayout->addRow(tr("镭射内容"), m_laserContentLabel);
    summaryLayout->addRow(tr("读码内容"), m_readCodeLabel);
    summaryLayout->addRow(tr("算法规划"), m_algorithmPlanLabel);

    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setColumnCount(2);
    m_treeWidget->setHeaderLabels({tr("字段"), tr("内容")});
    m_treeWidget->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_treeWidget->header()->resizeSection(0, 220);
    m_treeWidget->header()->setStretchLastSection(true);
    m_treeWidget->setAlternatingRowColors(true);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addWidget(m_pathLabel);
    layout->addWidget(summaryFrame);
    layout->addWidget(m_treeWidget, 1);
    layout->addWidget(buttonBox);
}

void PointDetailDialog::setDetailFilePath(const QString& filePath) {
    m_pathLabel->setText(tr("详情文件：%1").arg(filePath));
}

void PointDetailDialog::setDetailObject(const QJsonObject& object) {
    m_treeWidget->clear();
    const QJsonObject dataObject = object.value("data").isObject() ? object.value("data").toObject() : object;
    m_fieldDisplayNames = dataObject.value(Infrastructure::PointDetailJsonService::fieldDisplayNamesKey()).toObject();

    QString templatePath = dataObject.value("templateFilePath").toString();
    if (templatePath.trimmed().isEmpty()) {
        templatePath = dataObject.value("laserTemplatePath").toString();
    }

    const QJsonValue algorithmPlanValue = dataObject.contains(Infrastructure::PointDetailJsonService::algorithmPlanKey())
                                              ? dataObject.value(Infrastructure::PointDetailJsonService::algorithmPlanKey())
                                              : dataObject.value(Infrastructure::PointDetailJsonService::algorithmPlanAlias());

    setSummaryValue(m_templatePathLabel, templatePath);
    setSummaryValue(m_programLabel, formatValue(QStringLiteral("programName"), dataObject.value("programName")));
    setSummaryValue(m_successLabel, formatValue(QStringLiteral("success"), dataObject.value("success")));
    setSummaryValue(m_startTimeLabel, formatValue(QStringLiteral("startTime"), dataObject.value("startTime")));
    setSummaryValue(m_endTimeLabel, formatValue(QStringLiteral("endTime"), dataObject.value("endTime")));
    setSummaryValue(m_laserContentLabel, formatValue(QStringLiteral("laserContent"), dataObject.value("laserContent")));
    setSummaryValue(m_readCodeLabel, formatValue(QStringLiteral("readCodeContent"), dataObject.value("readCodeContent")));
    setSummaryValue(m_algorithmPlanLabel,
                    algorithmPlanValue.isArray() ? formatAlgorithmPlanSummary(algorithmPlanValue.toArray()) : tr("未提供"));

    for (auto it = object.begin(); it != object.end(); ++it) {
        appendJsonValue(nullptr, it.key(), it.value());
    }
    m_treeWidget->expandToDepth(2);
}

QString PointDetailDialog::displayNameForKey(const QString& key) const {
    const QString mapped = m_fieldDisplayNames.value(key).toString().trimmed();
    return mapped.isEmpty() ? PointDetailFieldCatalog::displayName(key) : mapped;
}

QString PointDetailDialog::formatValue(const QString& key, const QJsonValue& value) const {
    if (value.isString()) {
        const QString text = value.toString();
        if ((key == QStringLiteral("startTime") || key == QStringLiteral("endTime")) && !text.trimmed().isEmpty()) {
            const QDateTime dateTime = QDateTime::fromString(text, Qt::ISODate);
            if (dateTime.isValid()) {
                return dateTime.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            }
        }
        return text;
    }
    if (value.isBool()) {
        return value.toBool() ? tr("是") : tr("否");
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble());
    }
    if (value.isArray()) {
        return tr("%1 项").arg(value.toArray().size());
    }
    if (value.isNull() || value.isUndefined()) {
        return tr("未提供");
    }
    return QString();
}

QString PointDetailDialog::formatAlgorithmPlanSummary(const QJsonArray& value) const {
    if (value.isEmpty()) {
        return tr("0 项");
    }

    QStringList nodeNames;
    nodeNames.reserve(value.size());
    for (const QJsonValue& item : value) {
        if (!item.isObject()) {
            continue;
        }
        const QJsonObject node = item.toObject();
        const QString name = node.value(QStringLiteral("name")).toString().trimmed().isEmpty()
                                 ? node.value(QStringLiteral("nodeName")).toString().trimmed()
                                 : node.value(QStringLiteral("name")).toString().trimmed();
        if (!name.isEmpty()) {
            nodeNames.append(name);
        }
    }

    if (nodeNames.isEmpty()) {
        return tr("%1 项").arg(value.size());
    }
    return tr("%1 项：%2").arg(value.size()).arg(nodeNames.join(tr(" -> ")));
}

void PointDetailDialog::setSummaryValue(QLabel* label, const QString& text) {
    if (label == nullptr) {
        return;
    }
    label->setText(text.trimmed().isEmpty() ? tr("未提供") : text);
}

void PointDetailDialog::appendJsonObject(QTreeWidgetItem* parent, const QJsonObject& object) {
    const QStringList preferredOrder = preferredObjectKeyOrder();
    QSet<QString> appendedKeys;

    for (const QString& key : preferredOrder) {
        if (object.contains(key) && key != Infrastructure::PointDetailJsonService::fieldDisplayNamesKey()) {
            appendJsonValue(parent, key, object.value(key));
            appendedKeys.insert(key);
        }
    }

    for (auto it = object.begin(); it != object.end(); ++it) {
        if (it.key() != Infrastructure::PointDetailJsonService::fieldDisplayNamesKey() &&
            !appendedKeys.contains(it.key())) {
            appendJsonValue(parent, it.key(), it.value());
        }
    }
}

void PointDetailDialog::appendJsonValue(QTreeWidgetItem* parent, const QString& key, const QJsonValue& value) {
    auto* item = new QTreeWidgetItem({displayNameForKey(key), QString()});
    if (parent != nullptr) {
        parent->addChild(item);
    } else {
        m_treeWidget->addTopLevelItem(item);
    }

    if (value.isObject()) {
        appendJsonObject(item, value.toObject());
        return;
    }

    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        item->setText(1, formatValue(key, value));
        for (int index = 0; index < array.size(); ++index) {
            appendJsonValue(item, QString("[%1]").arg(index), array.at(index));
        }
        return;
    }

    item->setText(1, formatValue(key, value));
}

}  // namespace LaserSpc::Ui

#pragma once

#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

class QLabel;
class QFormLayout;
class QTreeWidget;
class QTreeWidgetItem;

namespace LaserSpc::Ui {

class PointDetailDialog : public QDialog {
    Q_OBJECT

public:
    explicit PointDetailDialog(QWidget* parent = nullptr);

    void setDetailFilePath(const QString& filePath);
    void setDetailObject(const QJsonObject& object);

private:
    QString displayNameForKey(const QString& key) const;
    QString formatValue(const QString& key, const QJsonValue& value) const;
    QString formatAlgorithmPlanSummary(const QJsonArray& value) const;
    void setSummaryValue(QLabel* label, const QString& text);
    void appendJsonObject(QTreeWidgetItem* parent, const QJsonObject& object);
    void appendJsonValue(QTreeWidgetItem* parent, const QString& key, const QJsonValue& value);

    QLabel* m_pathLabel = nullptr;
    QLabel* m_templatePathLabel = nullptr;
    QLabel* m_programLabel = nullptr;
    QLabel* m_successLabel = nullptr;
    QLabel* m_startTimeLabel = nullptr;
    QLabel* m_endTimeLabel = nullptr;
    QLabel* m_laserContentLabel = nullptr;
    QLabel* m_readCodeLabel = nullptr;
    QLabel* m_algorithmPlanLabel = nullptr;
    QTreeWidget* m_treeWidget = nullptr;
    QJsonObject m_fieldDisplayNames;
};

}  // namespace LaserSpc::Ui

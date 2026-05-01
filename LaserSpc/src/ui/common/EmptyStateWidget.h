#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

namespace LaserSpc::Ui {

class EmptyStateWidget : public QWidget {
    Q_OBJECT

public:
    explicit EmptyStateWidget(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void setDescription(const QString& description);
    void setActionText(const QString& text);
    void setActionVisible(bool visible);

signals:
    void actionTriggered();

private:
    QLabel* m_titleLabel = nullptr;
    QLabel* m_descriptionLabel = nullptr;
    QPushButton* m_actionButton = nullptr;
};

}  // namespace LaserSpc::Ui

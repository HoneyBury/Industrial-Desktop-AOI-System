#include "ui/common/EmptyStateWidget.h"
#include <QVariant>

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "ui/common/UiTheme.h"

namespace LaserSpc::Ui {

EmptyStateWidget::EmptyStateWidget(QWidget* parent)
    : QWidget(parent) {
    UiTheme::applyPanel(this);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 28, 28, 28);
    layout->setSpacing(10);
    layout->setAlignment(Qt::AlignCenter);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setProperty("emptyStateTitle", QVariant(true));
    m_titleLabel->setAlignment(Qt::AlignCenter);

    m_descriptionLabel = new QLabel(this);
    m_descriptionLabel->setProperty("emptyStateDesc", QVariant(true));
    m_descriptionLabel->setAlignment(Qt::AlignCenter);
    m_descriptionLabel->setWordWrap(true);

    m_actionButton = new QPushButton(this);
    UiTheme::applyPrimaryButton(m_actionButton);
    m_actionButton->setVisible(false);

    layout->addWidget(m_titleLabel);
    layout->addWidget(m_descriptionLabel);
    layout->addWidget(m_actionButton, 0, Qt::AlignCenter);

    connect(m_actionButton, &QPushButton::clicked, this, &EmptyStateWidget::actionTriggered);
}

void EmptyStateWidget::setTitle(const QString& title) {
    m_titleLabel->setText(title);
}

void EmptyStateWidget::setDescription(const QString& description) {
    m_descriptionLabel->setText(description);
}

void EmptyStateWidget::setActionText(const QString& text) {
    m_actionButton->setText(text);
}

void EmptyStateWidget::setActionVisible(bool visible) {
    m_actionButton->setVisible(visible);
}

}  // namespace LaserSpc::Ui

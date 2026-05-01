#pragma once

#include <QFrame>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace LaserSpc::Ui {

struct PageContainerParts {
    QScrollArea* scrollArea = nullptr;
    QWidget* contentWidget = nullptr;
    QVBoxLayout* contentLayout = nullptr;
};

inline PageContainerParts buildScrollablePageContainer(QWidget* host,
                                                       QVBoxLayout* rootLayout,
                                                       int spacing = 14,
                                                       const QMargins& contentMargins = QMargins(0, 0, 0, 0)) {
    PageContainerParts parts;
    host->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    parts.scrollArea = new QScrollArea(host);
    parts.scrollArea->setWidgetResizable(true);
    parts.scrollArea->setFrameShape(QFrame::NoFrame);
    parts.scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    parts.scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    parts.contentWidget = new QWidget(parts.scrollArea);
    parts.contentWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    parts.contentLayout = new QVBoxLayout(parts.contentWidget);
    parts.contentLayout->setContentsMargins(contentMargins);
    parts.contentLayout->setSpacing(spacing);

    parts.scrollArea->setWidget(parts.contentWidget);
    rootLayout->addWidget(parts.scrollArea);
    return parts;
}

}  // namespace LaserSpc::Ui

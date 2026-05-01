#pragma once

#include <QObject>

#include "domain/Models.h"

namespace LaserSpc::Ui {

class MainShellMediator : public QObject {
    Q_OBJECT

public:
    explicit MainShellMediator(QObject* parent = nullptr);

public slots:
    void onNavigationChanged(int index);

signals:
    void pageSwitchRequested(int index, LaserSpc::Domain::PageId pageId);

private:
    LaserSpc::Domain::PageId resolvePageId(int index) const;
};

}  // namespace LaserSpc::Ui

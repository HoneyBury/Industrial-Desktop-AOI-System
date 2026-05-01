#include "ui/shell/MainShellMediator.h"

namespace LaserSpc::Ui {

MainShellMediator::MainShellMediator(QObject* parent) : QObject(parent) {}

void MainShellMediator::onNavigationChanged(int index) {
    emit pageSwitchRequested(index, resolvePageId(index));
}

LaserSpc::Domain::PageId MainShellMediator::resolvePageId(int index) const {
    switch (index) {
        case 1:
            return LaserSpc::Domain::PageId::BadStat;
        case 2:
            return LaserSpc::Domain::PageId::BoardRecord;
        case 3:
            return LaserSpc::Domain::PageId::PointRecord;
        case 0:
        default:
            return LaserSpc::Domain::PageId::Summary;
    }
}

}  // namespace LaserSpc::Ui

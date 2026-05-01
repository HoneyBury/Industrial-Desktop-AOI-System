#pragma once

#include <QObject>
#include <QString>

namespace LaserSpc::Ui::TextCatalog {

enum class SummaryMetricTextKey {
    TotalBoardsTitle,
    TotalBoardsDescription,
    GoodBoardsTitle,
    GoodBoardsDescription,
    BadBoardsTitle,
    BadBoardsDescription,
    YieldRateTitle,
    YieldRateDescription
};

inline QString allSelection() {
    return QObject::tr("全部");
}

inline QString defaultReportTitle() {
    return QObject::tr("LaserSpc SPC 报告");
}

inline QString defaultReportFooter() {
    return QObject::tr("LaserSpc 自动生成");
}

inline QString summaryMetricText(SummaryMetricTextKey key) {
    switch (key) {
        case SummaryMetricTextKey::TotalBoardsTitle: return QObject::tr("总板数");
        case SummaryMetricTextKey::TotalBoardsDescription: return QObject::tr("当前筛选条件下的板级总量");
        case SummaryMetricTextKey::GoodBoardsTitle: return QObject::tr("良板数");
        case SummaryMetricTextKey::GoodBoardsDescription: return QObject::tr("板级 OK 数量");
        case SummaryMetricTextKey::BadBoardsTitle: return QObject::tr("不良板数");
        case SummaryMetricTextKey::BadBoardsDescription: return QObject::tr("板级 NG 数量");
        case SummaryMetricTextKey::YieldRateTitle: return QObject::tr("良率");
        case SummaryMetricTextKey::YieldRateDescription: return QObject::tr("按良板数 / 总板数计算");
    }
    return {};
}

}  // namespace LaserSpc::Ui::TextCatalog

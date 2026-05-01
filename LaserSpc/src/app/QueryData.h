#pragma once

#include "domain/Models.h"

namespace LaserSpc::App {

struct SummaryPageData {
    QList<LaserSpc::Domain::MetricCardData> metrics;
    LaserSpc::Domain::PageResult<LaserSpc::Domain::SummaryRow> table;
};

struct BadStatPageData {
    int totalBadPoints = 0;
    int totalGradePoints = 0;
    QList<LaserSpc::Domain::BadPointStatRow> badPoints;
    QList<LaserSpc::Domain::GradeStatRow> grades;
};

}  // namespace LaserSpc::App

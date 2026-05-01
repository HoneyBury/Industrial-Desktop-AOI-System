#pragma once

#include <QAbstractItemView>
#include <QHeaderView>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>

#include "ui/common/UiTheme.h"

namespace LaserSpc::Ui {

inline void configureDataTable(QTableWidget* table, const QStringList& headers, int sortColumn = -1) {
    if (table == nullptr) {
        return;
    }

    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->horizontalHeader()->setMinimumSectionSize(72);
    if (!headers.isEmpty()) {
        table->horizontalHeader()->setStretchLastSection(true);
    }
    for (int column = 0; column < headers.size(); ++column) {
        table->setColumnWidth(column, column >= headers.size() - 2 ? 160 : 120);
    }
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSortingEnabled(false);
    table->horizontalHeader()->setSectionsClickable(true);
    table->horizontalHeader()->setSortIndicatorShown(sortColumn >= 0);
    if (sortColumn >= 0) {
        table->horizontalHeader()->setSortIndicator(sortColumn, Qt::DescendingOrder);
    }
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setWordWrap(false);
    UiTheme::applyTable(table);
}

inline void setTextTableRow(QTableWidget* table, int row, const QStringList& values) {
    if (table == nullptr) {
        return;
    }

    for (int column = 0; column < values.size(); ++column) {
        auto* item = new QTableWidgetItem(values.at(column));
        item->setTextAlignment(Qt::AlignCenter);
        table->setItem(row, column, item);
    }
}

}  // namespace LaserSpc::Ui

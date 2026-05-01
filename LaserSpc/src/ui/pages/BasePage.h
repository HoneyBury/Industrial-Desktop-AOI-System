#pragma once

#include <QAbstractScrollArea>
#include <QHeaderView>
#include <QLayout>
#include <QMargins>
#include <QPainter>
#include <QPixmap>
#include <QScrollArea>
#include <QScrollBar>
#include <QTableWidget>
#include <QWidget>

#include "app/AppServiceFacade.h"
#include "domain/Models.h"
#include "ui/pages/PageAsyncSupport.h"

namespace LaserSpc::Ui {

struct PageInsightMetric {
    QString title;
    QString value;
};

class BasePage : public QWidget {
public:
    explicit BasePage(LaserSpc::App::AppServiceFacade* facade, QWidget* parent = nullptr)
        : QWidget(parent), m_facade(facade) {}
    ~BasePage() override = default;

    virtual LaserSpc::Domain::PageId pageId() const = 0;
    virtual QString pageTitle() const = 0;
    virtual void reload(const LaserSpc::Domain::FilterCriteria& criteria) = 0;
    virtual void refreshTexts(bool english) {
        Q_UNUSED(english);
    }
    virtual QList<PageInsightMetric> pageInsights(bool english) const {
        Q_UNUSED(english);
        return {};
    }

protected:
    QPixmap captureModuleSnapshot(QWidget* contentWidget,
                                 const QList<QTableWidget*>& expandedTables = {},
                                 const QMargins& margins = QMargins(12, 12, 12, 12)) const {
        if (contentWidget == nullptr) {
            return {};
        }

        auto renderWidget = [](QWidget* widget, int targetWidth) {
            if (widget == nullptr) {
                return QPixmap();
            }

            const QSize oldSize = widget->size();
            const QSize minimumSize = widget->minimumSize();
            const QSize maximumSize = widget->maximumSize();
            const int width = qMax(targetWidth, widget->minimumSizeHint().width());
            widget->resize(width, widget->sizeHint().height());
            if (widget->layout() != nullptr) {
                widget->layout()->activate();
            }
            widget->adjustSize();
            const QSize renderSize(qMax(width, widget->sizeHint().width()),
                                   qMax(widget->height(), widget->sizeHint().height()));
            widget->resize(renderSize);
            if (widget->layout() != nullptr) {
                widget->layout()->activate();
            }

            QPixmap pixmap(renderSize);
            pixmap.fill(Qt::transparent);
            widget->render(&pixmap, QPoint(), QRegion(), QWidget::DrawChildren);
            widget->setMinimumSize(minimumSize);
            widget->setMaximumSize(maximumSize);
            widget->resize(oldSize);
            if (widget->layout() != nullptr) {
                widget->layout()->activate();
            }
            widget->updateGeometry();
            if (QWidget* parent = widget->parentWidget()) {
                parent->updateGeometry();
            }
            return pixmap;
        };

        struct TableSnapshotState {
            QTableWidget* table = nullptr;
            int minimumHeight = 0;
            int maximumHeight = 0;
            Qt::ScrollBarPolicy verticalPolicy = Qt::ScrollBarAsNeeded;
        };

        QList<TableSnapshotState> tableStates;
        for (QTableWidget* table : expandedTables) {
            if (table == nullptr) {
                continue;
            }
            TableSnapshotState state;
            state.table = table;
            state.minimumHeight = table->minimumHeight();
            state.maximumHeight = table->maximumHeight();
            state.verticalPolicy = table->verticalScrollBarPolicy();
            tableStates.append(state);

            int totalHeight = table->horizontalHeader()->height() + table->frameWidth() * 2;
            for (int row = 0; row < table->rowCount(); ++row) {
                totalHeight += table->rowHeight(row);
            }
            if (table->horizontalScrollBar()->isVisible()) {
                totalHeight += table->horizontalScrollBar()->sizeHint().height();
            }
            table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            table->setMinimumHeight(totalHeight);
            table->setMaximumHeight(totalHeight);
        }

        QWidget* filterPanel = window() == nullptr ? nullptr : window()->findChild<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
        if (window() != nullptr) {
            const auto children = window()->findChildren<QWidget*>();
            for (QWidget* child : children) {
                if (child != nullptr && child->metaObject()->className() == QByteArray("LaserSpc::Ui::FilterPanel")) {
                    filterPanel = child;
                    break;
                }
            }
        }

        const int contentWidth = qMax(contentWidget->width(), qMax(contentWidget->sizeHint().width(), 960));
        const QPixmap filterPixmap = (filterPanel != nullptr && filterPanel->isVisible()) ? renderWidget(filterPanel, contentWidth) : QPixmap();
        const QPixmap contentPixmap = renderWidget(contentWidget, contentWidth);

        for (const TableSnapshotState& state : tableStates) {
            state.table->setMinimumHeight(state.minimumHeight);
            state.table->setMaximumHeight(state.maximumHeight);
            state.table->setVerticalScrollBarPolicy(state.verticalPolicy);
            state.table->updateGeometry();
        }

        if (contentPixmap.isNull()) {
            return {};
        }

        const int spacing = filterPixmap.isNull() ? 0 : 10;
        const int width = qMax(contentPixmap.width(), filterPixmap.width()) + margins.left() + margins.right();
        const int height = margins.top() + margins.bottom() + contentPixmap.height() +
                           (filterPixmap.isNull() ? 0 : filterPixmap.height() + spacing);
        QPixmap composed(width, height);
        composed.fill(QColor(QObject::tr("#f3f6fb")));

        QPainter painter(&composed);
        int y = margins.top();
        if (!filterPixmap.isNull()) {
            painter.drawPixmap(margins.left(), y, filterPixmap);
            y += filterPixmap.height() + spacing;
        }
        painter.drawPixmap(margins.left(), y, contentPixmap);
        return composed;
    }

    void setLoadState(PageLoadState loadState, const QString& message = QString()) {
        m_viewState.loadState = loadState;
        if (!message.isNull()) {
            m_viewState.message = message;
        }
    }

    void setExportBusy(bool exportBusy) {
        m_viewState.exportBusy = exportBusy;
    }

    PageLoadState loadState() const {
        return m_viewState.loadState;
    }

    bool isQueryBusy() const {
        return m_viewState.loadState == PageLoadState::Loading;
    }

    bool isExportBusy() const {
        return m_viewState.exportBusy;
    }

    bool isInteractionLocked() const {
        return isQueryBusy() || isExportBusy();
    }

    const PageViewState& viewState() const {
        return m_viewState;
    }

    LaserSpc::App::AppServiceFacade* m_facade = nullptr;
    bool uiEnglish() const {
        return m_facade != nullptr &&
               m_facade->settings().ui.language == LaserSpc::Infrastructure::AppLanguage::English;
    }

private:
    PageViewState m_viewState;
};

}  // namespace LaserSpc::Ui

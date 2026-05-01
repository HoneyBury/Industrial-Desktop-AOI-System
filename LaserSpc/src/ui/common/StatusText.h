#pragma once

#include <QString>

#include "ui/pages/PageAsyncSupport.h"

namespace LaserSpc::Ui::StatusText {

enum class Language {
    Chinese,
    English
};

inline Language& currentLanguageStorage() {
    static Language language = Language::Chinese;
    return language;
}

inline void setLanguage(Language language) {
    currentLanguageStorage() = language;
}

inline bool isEnglish() {
    return currentLanguageStorage() == Language::English;
}

inline QString pick(const char* chinese, const char* english) {
    return QString::fromUtf8(isEnglish() ? english : chinese);
}

inline QString pagePendingRefresh(const QString& subject) {
    return isEnglish() ? subject + QString::fromUtf8(" received updated filters and will refresh after the active query finishes.")
                       : subject + QString::fromUtf8("已收到新的筛选条件，当前查询完成后将自动刷新最新结果。");
}

inline QString pageQueryLoading(const QString& subject) {
    return isEnglish() ? subject + QString::fromUtf8(" loading...")
                       : subject + QString::fromUtf8("查询中...");
}

inline QString pageQueryFailed(const QString& subject, const QString& error) {
    return isEnglish() ? subject + QString::fromUtf8(" query failed: ") + error
                       : subject + QString::fromUtf8("查询失败：") + error;
}

inline QString pageNoData(const QString& subject) {
    return isEnglish() ? subject + QString::fromUtf8(" has no data under the current filters.")
                       : subject + QString::fromUtf8("在当前筛选条件下没有数据。");
}

inline QString pagedRefresh(const QString& subject, int page, int total, const QString& sortField) {
    return isEnglish()
               ? QString::fromUtf8("%1 refreshed: page %2, total %3 rows, sorted by %4.")
                     .arg(subject)
                     .arg(page)
                     .arg(total)
                     .arg(sortField)
               : QString::fromUtf8("%1已刷新：第 %2 页，共 %3 行，排序字段 %4。")
                     .arg(subject)
                     .arg(page)
                     .arg(total)
                     .arg(sortField);
}

inline QString statRefresh(const QString& subject, int primaryCount, int secondaryCount) {
    return isEnglish()
               ? QString::fromUtf8("%1 refreshed: top bad points %2 items, grade distribution %3 items.")
                     .arg(subject)
                     .arg(primaryCount)
                     .arg(secondaryCount)
               : QString::fromUtf8("%1已刷新：Top 不良点 %2 项，等级分布 %3 项。")
                     .arg(subject)
                     .arg(primaryCount)
                     .arg(secondaryCount);
}

inline QString exportBusy(const QString& subject) {
    return isEnglish() ? subject + QString::fromUtf8(" export is in progress, please wait.")
                       : subject + QString::fromUtf8("导出进行中，请稍候。");
}

inline QString exportStarted(const QString& subject, const QString& target) {
    return isEnglish() ? QString::fromUtf8("%1 exporting %2...").arg(subject, target)
                       : QString::fromUtf8("%1正在导出%2...").arg(subject, target);
}

inline QString exportFailed(const QString& subject, const QString& error) {
    return isEnglish() ? subject + QString::fromUtf8(" export failed: ") + error
                       : subject + QString::fromUtf8("导出失败：") + error;
}

inline QString exportCompleted(const QString& subject, const QString& outputPath) {
    return isEnglish() ? subject + QString::fromUtf8(" export completed: ") + outputPath
                       : subject + QString::fromUtf8("导出已完成：") + outputPath;
}

inline QString filterOptionsQueued() {
    return pick("筛选项刷新请求已更新，当前加载完成后将自动刷新最新筛选项。",
                "Filter option refresh was queued and will restart after the current load finishes.");
}

inline QString filterOptionsLoading() {
    return pick("筛选项加载中...", "Loading filter options...");
}

inline QString filterOptionsLoaded() {
    return pick("筛选项已加载。", "Filter options loaded.");
}

inline QString filterOptionsFailed(const QString& error) {
    return isEnglish() ? QString::fromUtf8("Failed to load filter options: ") + error
                       : QString::fromUtf8("筛选项加载失败：") + error;
}

inline QString filterOptionsStateLabel(PageLoadState state, const QString& detail = QString()) {
    switch (state) {
        case PageLoadState::Loading:
            return pick("筛选项状态：加载中...", "Filter status: loading...");
        case PageLoadState::Loaded:
            return pick("筛选项状态：最新选项已就绪。", "Filter status: latest options are ready.");
        case PageLoadState::Error:
            return detail.isEmpty() ? pick("筛选项状态：加载失败。", "Filter status: failed to load.")
                                    : (isEnglish() ? QString::fromUtf8("Filter status: failed to load - ") + detail
                                                   : QString::fromUtf8("筛选项状态：加载失败 - ") + detail);
        case PageLoadState::Empty:
            return pick("筛选项状态：当前没有可用选项。", "Filter status: no available options.");
        case PageLoadState::Idle:
        default:
            return pick("筛选项状态：等待加载。", "Filter status: waiting to load.");
    }
}

}  // namespace LaserSpc::Ui::StatusText

#pragma once

#include <QString>

#include "infrastructure/AppConfigService.h"

class QLabel;
class QPushButton;
class QTableWidget;
class QWidget;
class QMenu;

namespace LaserSpc::Ui {

enum class StatusTone {
    Neutral,
    Success,
    Warning,
    Danger
};

class UiTheme {
public:
    static QString applicationStyleSheet(
        LaserSpc::Infrastructure::ThemeStyle themeStyle = LaserSpc::Infrastructure::ThemeStyle::Ocean);
    static void applyPanel(QWidget* widget);
    static void applyTable(QTableWidget* table);
    static void applyPrimaryButton(QPushButton* button);
    static void applySecondaryButton(QPushButton* button);
    static void applyStatusLabel(QLabel* label, StatusTone tone);
    static void applyPopupMenu(QMenu* menu);
};

}  // namespace LaserSpc::Ui

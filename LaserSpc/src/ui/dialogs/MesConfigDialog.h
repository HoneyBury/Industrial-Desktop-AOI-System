#pragma once

#include <QDialog>

#include "infrastructure/AppConfigService.h"

class QCheckBox;
class QLineEdit;
class QSpinBox;

namespace LaserSpc::Ui {

class MesConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit MesConfigDialog(const LaserSpc::Infrastructure::MesSettings& settings, bool english, QWidget* parent = nullptr);

    LaserSpc::Infrastructure::MesSettings settings() const;

private:
    void setupUi(bool english);
    void loadSettings(const LaserSpc::Infrastructure::MesSettings& settings);
    void updateFieldState();

    QCheckBox* m_enabledCheckBox = nullptr;
    QLineEdit* m_endpointEdit = nullptr;
    QLineEdit* m_siteCodeEdit = nullptr;
    QLineEdit* m_stationCodeEdit = nullptr;
    QLineEdit* m_userNameEdit = nullptr;
    QSpinBox* m_timeoutSpinBox = nullptr;
};

}  // namespace LaserSpc::Ui

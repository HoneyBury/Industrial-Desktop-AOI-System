#include "ui/dialogs/MesConfigDialog.h"
#include <QVariant>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "ui/common/UiTheme.h"

namespace LaserSpc::Ui {

MesConfigDialog::MesConfigDialog(const LaserSpc::Infrastructure::MesSettings& settings, bool english, QWidget* parent)
    : QDialog(parent) {
    setupUi(english);
    loadSettings(settings);
    updateFieldState();
}

void MesConfigDialog::setupUi(bool english) {
    setWindowTitle(english ? QObject::tr("MES Configuration") : QObject::tr("MES 配置"));
    resize(520, 340);
    UiTheme::applyPanel(this);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(20, 20, 20, 16);
    rootLayout->setSpacing(14);
    auto* titleLabel = new QLabel(english ? QObject::tr("MES Configuration") : QObject::tr("MES 配置"), this);
    titleLabel->setProperty("dialogTitle", QVariant(true));
    auto* hintLabel = new QLabel(
        english ? QObject::tr("Configure MES endpoint and station identity used for downstream integration.")
                : QObject::tr("配置 MES 接口地址和站点身份，用于后续系统对接。"),
        this);
    hintLabel->setProperty("hint", QVariant(true));
    hintLabel->setWordWrap(true);

    auto* formCard = new QFrame(this);
    UiTheme::applyPanel(formCard);
    auto* cardLayout = new QVBoxLayout(formCard);
    cardLayout->setContentsMargins(16, 16, 16, 16);
    cardLayout->setSpacing(12);

    auto* formLayout = new QFormLayout();
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setSpacing(10);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setFormAlignment(Qt::AlignTop);

    m_enabledCheckBox =
        new QCheckBox(english ? QObject::tr("Enable MES integration") : QObject::tr("启用 MES 对接"), this);
    m_endpointEdit = new QLineEdit(this);
    m_endpointEdit->setPlaceholderText(english ? QObject::tr("https://mes.example.com/api/report")
                                               : QObject::tr("https://mes.example.com/api/report"));
    m_siteCodeEdit = new QLineEdit(this);
    m_stationCodeEdit = new QLineEdit(this);
    m_userNameEdit = new QLineEdit(this);
    m_timeoutSpinBox = new QSpinBox(this);
    m_timeoutSpinBox->setRange(1, 60);
    m_timeoutSpinBox->setSuffix(english ? QObject::tr(" s") : QObject::tr(" 秒"));

    auto addLabelRow = [formLayout](const QString& labelText, QWidget* field) {
        auto* label = new QLabel(labelText);
        label->setProperty("formLabel", QVariant(true));
        formLayout->addRow(label, field);
    };

    formLayout->addRow(QString(), m_enabledCheckBox);
    addLabelRow(english ? QObject::tr("Endpoint") : QObject::tr("接口地址"), m_endpointEdit);
    addLabelRow(english ? QObject::tr("Site code") : QObject::tr("厂区编码"), m_siteCodeEdit);
    addLabelRow(english ? QObject::tr("Station code") : QObject::tr("工位编码"), m_stationCodeEdit);
    addLabelRow(english ? QObject::tr("User") : QObject::tr("用户名"), m_userNameEdit);
    addLabelRow(english ? QObject::tr("Timeout") : QObject::tr("超时时间"), m_timeoutSpinBox);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    UiTheme::applyPrimaryButton(buttonBox->button(QDialogButtonBox::Save));
    UiTheme::applySecondaryButton(buttonBox->button(QDialogButtonBox::Cancel));
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_enabledCheckBox, &QCheckBox::toggled, this, &MesConfigDialog::updateFieldState);

    cardLayout->addLayout(formLayout);
    rootLayout->addWidget(titleLabel);
    rootLayout->addWidget(hintLabel);
    rootLayout->addWidget(formCard);
    rootLayout->addStretch();
    rootLayout->addWidget(buttonBox);
}

void MesConfigDialog::loadSettings(const LaserSpc::Infrastructure::MesSettings& settings) {
    m_enabledCheckBox->setChecked(settings.enabled);
    m_endpointEdit->setText(settings.endpointUrl);
    m_siteCodeEdit->setText(settings.siteCode);
    m_stationCodeEdit->setText(settings.stationCode);
    m_userNameEdit->setText(settings.userName);
    m_timeoutSpinBox->setValue(settings.timeoutSeconds);
}

LaserSpc::Infrastructure::MesSettings MesConfigDialog::settings() const {
    LaserSpc::Infrastructure::MesSettings value;
    value.enabled = m_enabledCheckBox->isChecked();
    value.endpointUrl = m_endpointEdit->text().trimmed();
    value.siteCode = m_siteCodeEdit->text().trimmed();
    value.stationCode = m_stationCodeEdit->text().trimmed();
    value.userName = m_userNameEdit->text().trimmed();
    value.timeoutSeconds = m_timeoutSpinBox->value();
    return value;
}

void MesConfigDialog::updateFieldState() {
    const bool enabled = m_enabledCheckBox->isChecked();
    m_endpointEdit->setEnabled(enabled);
    m_siteCodeEdit->setEnabled(enabled);
    m_stationCodeEdit->setEnabled(enabled);
    m_userNameEdit->setEnabled(enabled);
    m_timeoutSpinBox->setEnabled(enabled);
}

}  // namespace LaserSpc::Ui

#include "src/ui/settings_dialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QVBoxLayout>

#include "src/ai/ai_provider_config.h"
#include "src/app/app_settings.h"
#include "src/ui/lucide_icon_factory.h"

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("settingsDialog"));
    setWindowTitle(tr("Settings"));
    setWindowIcon(LucideIconFactory::icon(LucideIconFactory::IconType::Settings, QColor("#1b3b5d"), 20));
    resize(640, 450);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);

    auto *headerCard = new QFrame(this);
    headerCard->setObjectName(QStringLiteral("headerCard"));
    auto *headerLayout = new QHBoxLayout(headerCard);
    headerLayout->setContentsMargins(18, 16, 18, 16);
    headerLayout->setSpacing(12);

    auto *iconBadge = new QLabel(headerCard);
    iconBadge->setObjectName(QStringLiteral("dialogIconBadge"));
    iconBadge->setPixmap(LucideIconFactory::pixmap(LucideIconFactory::IconType::Settings, 24, QColor("#ffffff")));
    iconBadge->setFixedSize(48, 48);
    iconBadge->setAlignment(Qt::AlignCenter);

    auto *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(4);

    auto *titleLabel = new QLabel(tr("Application Settings"), headerCard);
    titleLabel->setObjectName(QStringLiteral("dialogTitle"));

    auto *hintLabel = new QLabel(tr("Saved values override environment variables. Leave a field empty to fall back to environment configuration. Language changes apply after restart."), headerCard);
    hintLabel->setObjectName(QStringLiteral("dialogHint"));
    hintLabel->setWordWrap(true);

    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(hintLabel);
    headerLayout->addWidget(iconBadge, 0, Qt::AlignTop);
    headerLayout->addLayout(titleLayout, 1);
    layout->addWidget(headerCard);

    auto *formCard = new QFrame(this);
    formCard->setObjectName(QStringLiteral("formCard"));
    auto *formCardLayout = new QVBoxLayout(formCard);
    formCardLayout->setContentsMargins(18, 18, 18, 18);
    formCardLayout->setSpacing(0);

    auto *formLayout = new QFormLayout();
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    formLayout->setHorizontalSpacing(14);
    formLayout->setVerticalSpacing(12);

    m_baseUrlEdit = new QLineEdit(this);
    m_baseUrlEdit->setPlaceholderText(tr("https://api.example.com/v1 or .../chat/completions"));
    m_apiKeyEdit = new QLineEdit(this);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    m_apiKeyEdit->setPlaceholderText(tr("sk-..."));
    m_modelEdit = new QLineEdit(this);
    m_modelEdit->setPlaceholderText(tr("gpt-4.1-mini / qwen-plus / ..."));
    m_systemPromptEdit = new QPlainTextEdit(this);
    m_systemPromptEdit->setObjectName(QStringLiteral("systemPromptEdit"));
    m_systemPromptEdit->setPlaceholderText(tr("Optional custom system prompt"));
    m_systemPromptEdit->setMinimumHeight(120);
    m_languageCombo = new QComboBox(this);
    m_languageCombo->addItem(tr("English"), QStringLiteral("en"));
    m_languageCombo->addItem(tr("简体中文"), QStringLiteral("zh_CN"));

    formLayout->addRow(tr("Language"), m_languageCombo);
    formLayout->addRow(tr("Base URL"), m_baseUrlEdit);
    formLayout->addRow(tr("API Key"), m_apiKeyEdit);
    formLayout->addRow(tr("Model"), m_modelEdit);
    formLayout->addRow(tr("System Prompt"), m_systemPromptEdit);
    formCardLayout->addLayout(formLayout);
    layout->addWidget(formCard);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    if (QPushButton *saveButton = buttonBox->button(QDialogButtonBox::Save)) {
        saveButton->setText(tr("Save"));
        saveButton->setObjectName(QStringLiteral("dialogPrimaryButton"));
    }
    if (QPushButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel)) {
        cancelButton->setText(tr("Cancel"));
        cancelButton->setObjectName(QStringLiteral("dialogSecondaryButton"));
    }
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::saveAndAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    loadValues();
}

void SettingsDialog::loadValues()
{
    const AiProviderConfig config = AiProviderConfig::load();
    m_initialLanguage = AppSettings::language();
    m_baseUrlEdit->setText(config.baseUrl);
    m_apiKeyEdit->setText(config.apiKey);
    m_modelEdit->setText(config.model);
    m_systemPromptEdit->setPlainText(config.systemPrompt);
    const int languageIndex = m_languageCombo->findData(m_initialLanguage);
    if (languageIndex >= 0) {
        m_languageCombo->setCurrentIndex(languageIndex);
    }
}

void SettingsDialog::saveAndAccept()
{
    AiProviderConfig config;
    config.baseUrl = m_baseUrlEdit->text().trimmed();
    config.apiKey = m_apiKeyEdit->text().trimmed();
    config.model = m_modelEdit->text().trimmed();
    config.systemPrompt = m_systemPromptEdit->toPlainText().trimmed();
    config.save();
    AppSettings::setLanguage(m_languageCombo->currentData().toString());
    m_restartRequired = m_languageCombo->currentData().toString() != m_initialLanguage;
    accept();
}

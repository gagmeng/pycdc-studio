#include "src/ui/settings_dialog.h"

#include <QDialogButtonBox>
#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "src/ai/ai_provider_config.h"
#include "src/ui/lucide_icon_factory.h"

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("AI Settings"));
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

    auto *titleLabel = new QLabel(tr("AI Provider Settings"), headerCard);
    titleLabel->setObjectName(QStringLiteral("dialogTitle"));

    auto *hintLabel = new QLabel(tr("Saved values override environment variables. Leave a field empty to fall back to environment configuration."), headerCard);
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
    m_systemPromptEdit->setPlaceholderText(tr("Optional custom system prompt"));
    m_systemPromptEdit->setMinimumHeight(120);

    formLayout->addRow(tr("Base URL"), m_baseUrlEdit);
    formLayout->addRow(tr("API Key"), m_apiKeyEdit);
    formLayout->addRow(tr("Model"), m_modelEdit);
    formLayout->addRow(tr("System Prompt"), m_systemPromptEdit);
    formCardLayout->addLayout(formLayout);
    layout->addWidget(formCard);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    if (QPushButton *saveButton = buttonBox->button(QDialogButtonBox::Save)) {
        saveButton->setIcon(LucideIconFactory::icon(LucideIconFactory::IconType::StatusOk, QColor("#ffffff"), 16));
    }
    if (QPushButton *cancelButton = buttonBox->button(QDialogButtonBox::Cancel)) {
        cancelButton->setIcon(LucideIconFactory::icon(LucideIconFactory::IconType::Exit, QColor("#5b6f86"), 16));
    }
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::saveAndAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    setStyleSheet(QStringLiteral(R"(
        QDialog {
            background: #eef3f8;
        }
        QFrame#headerCard, QFrame#formCard {
            background: #ffffff;
            border: 1px solid #d7e2f0;
            border-radius: 18px;
        }
        QLabel#dialogIconBadge {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #2f7cff,
                                        stop:1 #153a74);
            border: 1px solid rgba(255, 255, 255, 0.32);
            border-radius: 14px;
        }
        QLabel#dialogTitle {
            color: #152740;
            font-size: 22px;
            font-weight: 700;
        }
        QLabel#dialogHint, QLabel {
            color: #5b6f86;
        }
        QLineEdit, QPlainTextEdit {
            background: #fbfdff;
            color: #17304a;
            border: 1px solid #d7e2f0;
            border-radius: 12px;
            padding: 8px 10px;
        }
        QLineEdit:focus, QPlainTextEdit:focus {
            border: 1px solid #8db4ff;
        }
        QPushButton {
            background: #ffffff;
            color: #1c3652;
            border: 1px solid #d7e2f0;
            border-radius: 10px;
            padding: 8px 16px;
            font-weight: 600;
            min-width: 92px;
        }
        QPushButton:hover {
            background: #eef5ff;
        }
        QPushButton[text='Save'] {
            background: #1f63ff;
            color: #ffffff;
            border-color: #1f63ff;
        }
        QPushButton[text='Save']:hover {
            background: #1857e1;
        }
    )"));

    loadValues();
}

void SettingsDialog::loadValues()
{
    const AiProviderConfig config = AiProviderConfig::load();
    m_baseUrlEdit->setText(config.baseUrl);
    m_apiKeyEdit->setText(config.apiKey);
    m_modelEdit->setText(config.model);
    m_systemPromptEdit->setPlainText(config.systemPrompt);
}

void SettingsDialog::saveAndAccept()
{
    AiProviderConfig config;
    config.baseUrl = m_baseUrlEdit->text().trimmed();
    config.apiKey = m_apiKeyEdit->text().trimmed();
    config.model = m_modelEdit->text().trimmed();
    config.systemPrompt = m_systemPromptEdit->toPlainText().trimmed();
    config.save();
    accept();
}

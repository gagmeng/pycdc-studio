#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <QDialog>

class QLineEdit;
class QPlainTextEdit;
class QComboBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    bool restartRequired() const { return m_restartRequired; }

private slots:
    void saveAndAccept();

private:
    void loadValues();

    QLineEdit *m_baseUrlEdit = nullptr;
    QLineEdit *m_apiKeyEdit = nullptr;
    QLineEdit *m_modelEdit = nullptr;
    QPlainTextEdit *m_systemPromptEdit = nullptr;
    QComboBox *m_languageCombo = nullptr;
    QString m_initialLanguage;
    bool m_restartRequired = false;
};

#endif // SETTINGS_DIALOG_H

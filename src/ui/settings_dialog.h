#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <QDialog>

class QLineEdit;
class QPlainTextEdit;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void saveAndAccept();

private:
    void loadValues();

    QLineEdit *m_baseUrlEdit = nullptr;
    QLineEdit *m_apiKeyEdit = nullptr;
    QLineEdit *m_modelEdit = nullptr;
    QPlainTextEdit *m_systemPromptEdit = nullptr;
};

#endif // SETTINGS_DIALOG_H

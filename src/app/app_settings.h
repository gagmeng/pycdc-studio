#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <QString>

class QApplication;
class QTranslator;

class AppSettings
{
public:
    static QString language();
    static void setLanguage(const QString &language);

    static bool installTranslator(QApplication &app, QTranslator &translator);
    static void applyStyleSheet(QApplication &app);
    static bool restartApplication();

private:
    static QString normalizedLanguage(const QString &language);
};

#endif // APP_SETTINGS_H

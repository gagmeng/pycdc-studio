#include "src/app/app_settings.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QLocale>
#include <QProcess>
#include <QSettings>
#include <QTranslator>

namespace {
QString defaultLanguage()
{
    return QLocale::system().name().startsWith(QStringLiteral("zh"), Qt::CaseInsensitive)
        ? QStringLiteral("zh_CN")
        : QStringLiteral("en");
}
}

QString AppSettings::language()
{
    QSettings settings;
    const QString storedLanguage = settings.value(QStringLiteral("ui/language")).toString().trimmed();
    return normalizedLanguage(storedLanguage);
}

void AppSettings::setLanguage(const QString &language)
{
    QSettings settings;
    settings.setValue(QStringLiteral("ui/language"), normalizedLanguage(language));
    settings.sync();
}

bool AppSettings::installTranslator(QApplication &app, QTranslator &translator)
{
    const QString lang = language();
    if (lang == QStringLiteral("en")) {
        return true;
    }

    const QString qmPath = QStringLiteral(":/i18n/pycdc_studio_%1.qm").arg(lang);
    if (!translator.load(qmPath)) {
        return false;
    }

    app.installTranslator(&translator);
    return true;
}

void AppSettings::applyStyleSheet(QApplication &app)
{
    const QStringList candidatePaths = {
        QStringLiteral(":/styles/app.qss"),
        QStringLiteral(":/resources/styles/app.qss")
    };

    for (const QString &path : candidatePaths) {
        QFile styleFile(path);
        if (!styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
        return;
    }
}

bool AppSettings::restartApplication()
{
    return QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                   QCoreApplication::arguments());
}

QString AppSettings::normalizedLanguage(const QString &language)
{
    const QString lowered = language.trimmed().toLower();
    if (lowered.startsWith(QStringLiteral("zh"))) {
        return QStringLiteral("zh_CN");
    }
    if (lowered.startsWith(QStringLiteral("en"))) {
        return QStringLiteral("en");
    }
    return defaultLanguage();
}

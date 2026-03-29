#include "src/ai/ai_provider_config.h"

#include <QProcessEnvironment>
#include <QSettings>
#include <QStringList>

namespace {
QString firstNonEmpty(const QStringList &values)
{
    for (const QString &value : values) {
        if (!value.trimmed().isEmpty()) {
            return value;
        }
    }
    return QString();
}

QString readStoredOrEnv(QSettings &settings,
                        const QString &key,
                        const QStringList &envVars)
{
    const QString storedValue = settings.value(key).toString().trimmed();
    if (!storedValue.isEmpty()) {
        return storedValue;
    }

    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QStringList envValues;
    for (const QString &envVar : envVars) {
        envValues.append(env.value(envVar));
    }
    return firstNonEmpty(envValues);
}

void writeOrRemove(QSettings &settings, const QString &key, const QString &value)
{
    if (value.trimmed().isEmpty()) {
        settings.remove(key);
    } else {
        settings.setValue(key, value);
    }
}
}

bool AiProviderConfig::isValid() const
{
    return !baseUrl.trimmed().isEmpty()
        && !apiKey.trimmed().isEmpty()
        && !model.trimmed().isEmpty();
}

AiProviderConfig AiProviderConfig::load()
{
    QSettings settings;

    AiProviderConfig config;
    config.baseUrl = readStoredOrEnv(settings,
                                     QStringLiteral("ai/baseUrl"),
                                     { QStringLiteral("PYCDC_STUDIO_AI_BASE_URL"),
                                       QStringLiteral("OPENAI_BASE_URL"),
                                       QStringLiteral("OPENAI_API_BASE") });
    config.apiKey = readStoredOrEnv(settings,
                                    QStringLiteral("ai/apiKey"),
                                    { QStringLiteral("PYCDC_STUDIO_AI_API_KEY"),
                                      QStringLiteral("OPENAI_API_KEY") });
    config.model = readStoredOrEnv(settings,
                                   QStringLiteral("ai/model"),
                                   { QStringLiteral("PYCDC_STUDIO_AI_MODEL"),
                                     QStringLiteral("OPENAI_MODEL") });
    config.systemPrompt = readStoredOrEnv(settings,
                                          QStringLiteral("ai/systemPrompt"),
                                          { QStringLiteral("PYCDC_STUDIO_AI_SYSTEM_PROMPT") });
    if (config.systemPrompt.isEmpty()) {
        config.systemPrompt = QStringLiteral(
            "You reconstruct Python source code from bytecode metadata and disassembly. "
            "Return only Python source code.");
    }

    return config;
}

void AiProviderConfig::save() const
{
    QSettings settings;
    writeOrRemove(settings, QStringLiteral("ai/baseUrl"), baseUrl);
    writeOrRemove(settings, QStringLiteral("ai/apiKey"), apiKey);
    writeOrRemove(settings, QStringLiteral("ai/model"), model);
    writeOrRemove(settings, QStringLiteral("ai/systemPrompt"), systemPrompt);
    settings.sync();
}

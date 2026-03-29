#ifndef AI_PROVIDER_CONFIG_H
#define AI_PROVIDER_CONFIG_H

#include <QString>

struct AiProviderConfig
{
    QString baseUrl;
    QString apiKey;
    QString model;
    QString systemPrompt;

    bool isValid() const;
    static AiProviderConfig load();
    void save() const;
};

#endif // AI_PROVIDER_CONFIG_H

#ifndef OPENAI_COMPATIBLE_CLIENT_H
#define OPENAI_COMPATIBLE_CLIENT_H

#include <QObject>
#include <QJsonValue>

#include "src/ai/ai_provider_config.h"
#include "src/model/ai_reconstruction_result.h"

class OpenAiCompatibleClient : public QObject
{
    Q_OBJECT

public:
    explicit OpenAiCompatibleClient(QObject *parent = nullptr);

    void reloadFromEnvironment();
    bool isConfigured() const;
    const AiProviderConfig &config() const { return m_config; }

    AiReconstructionResult reconstruct(const QString &qualifiedName,
                                       const QString &prompt) const;

private:
    QString parseResponseText(const QByteArray &payload, QString *errorMessage) const;
    QString contentValueToText(const QJsonValue &value) const;
    QString stripMarkdownFences(const QString &text) const;

    AiProviderConfig m_config;
};

#endif // OPENAI_COMPATIBLE_CLIENT_H

#ifndef FALLBACK_SERVICE_H
#define FALLBACK_SERVICE_H

#include <QObject>
#include <QString>

#include "src/model/ai_reconstruction_result.h"

class OpenAiCompatibleClient;
class ProjectSession;
class PromptBuilder;

class FallbackService : public QObject
{
    Q_OBJECT

public:
    explicit FallbackService(ProjectSession &session,
                             PromptBuilder &promptBuilder,
                             OpenAiCompatibleClient &aiClient,
                             QObject *parent = nullptr);

    bool retryNodeWithAi(const QString &nodeId);

private:
    QString buildPlaceholderSource(const class CodeObjectNode &node) const;

    ProjectSession &m_session;
    PromptBuilder &m_promptBuilder;
    OpenAiCompatibleClient &m_aiClient;
};

#endif // FALLBACK_SERVICE_H

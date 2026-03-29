#include "src/ai/openai_compatible_client.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QStringList>
#include <QTimer>

namespace {
QString normalizedEndpointUrl(QString baseUrl)
{
    baseUrl = baseUrl.trimmed();
    if (baseUrl.endsWith(QLatin1Char('/'))) {
        baseUrl.chop(1);
    }
    if (baseUrl.endsWith(QStringLiteral("/chat/completions"))
        || baseUrl.endsWith(QStringLiteral("/responses"))) {
        return baseUrl;
    }
    return baseUrl + QStringLiteral("/chat/completions");
}
}

OpenAiCompatibleClient::OpenAiCompatibleClient(QObject *parent)
    : QObject(parent)
{
    reloadFromEnvironment();
}

void OpenAiCompatibleClient::reloadFromEnvironment()
{
    m_config = AiProviderConfig::load();
}

bool OpenAiCompatibleClient::isConfigured() const
{
    return m_config.isValid();
}

AiReconstructionResult OpenAiCompatibleClient::reconstruct(const QString &qualifiedName,
                                                           const QString &prompt) const
{
    AiReconstructionResult result;
    result.qualifiedName = qualifiedName;
    result.promptText = prompt;

    if (!m_config.isValid()) {
        result.errorMessage = tr("AI provider is not configured. Set PYCDC_STUDIO_AI_BASE_URL, PYCDC_STUDIO_AI_API_KEY, and PYCDC_STUDIO_AI_MODEL.");
        return result;
    }

    QJsonObject systemMessage;
    systemMessage[QStringLiteral("role")] = QStringLiteral("system");
    systemMessage[QStringLiteral("content")] = m_config.systemPrompt;

    QJsonObject userMessage;
    userMessage[QStringLiteral("role")] = QStringLiteral("user");
    userMessage[QStringLiteral("content")] = prompt;

    QJsonArray messages;
    messages.append(systemMessage);
    messages.append(userMessage);

    QJsonObject body;
    body[QStringLiteral("model")] = m_config.model;
    body[QStringLiteral("messages")] = messages;
    body[QStringLiteral("temperature")] = 0.1;

    QNetworkRequest request{QUrl(normalizedEndpointUrl(m_config.baseUrl))};
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_config.apiKey).toUtf8());

    QNetworkAccessManager manager;
    QPointer<QNetworkReply> reply = manager.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(90000);
    loop.exec();

    if (!reply) {
        result.errorMessage = tr("AI request was canceled unexpectedly.");
        return result;
    }

    if (timer.isActive()) {
        timer.stop();
    } else {
        reply->abort();
        reply->deleteLater();
        result.errorMessage = tr("AI request timed out.");
        return result;
    }

    const QByteArray responseData = reply->readAll();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString transportError = reply->error() == QNetworkReply::NoError
        ? QString()
        : reply->errorString();
    reply->deleteLater();

    QString parseError;
    const QString text = parseResponseText(responseData, &parseError);
    if (!transportError.isEmpty()) {
        result.errorMessage = tr("HTTP %1: %2").arg(statusCode).arg(parseError.isEmpty() ? transportError : parseError);
        return result;
    }
    if (!parseError.isEmpty()) {
        result.errorMessage = parseError;
        return result;
    }
    if (text.trimmed().isEmpty()) {
        result.errorMessage = tr("AI response did not contain usable source text.");
        return result;
    }

    result.success = true;
    result.responseText = QString::fromUtf8(responseData);
    result.finalSource = stripMarkdownFences(text);
    return result;
}

QString OpenAiCompatibleClient::parseResponseText(const QByteArray &payload, QString *errorMessage) const
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage) {
            *errorMessage = tr("Failed to parse AI response JSON.");
        }
        return QString();
    }

    const QJsonObject root = doc.object();
    if (root.contains(QStringLiteral("error"))) {
        const QJsonObject errorObject = root.value(QStringLiteral("error")).toObject();
        if (errorMessage) {
            *errorMessage = errorObject.value(QStringLiteral("message")).toString(tr("Unknown AI error."));
        }
        return QString();
    }

    const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
    if (!choices.isEmpty()) {
        const QJsonObject message = choices.first().toObject().value(QStringLiteral("message")).toObject();
        return contentValueToText(message.value(QStringLiteral("content")));
    }

    const QString outputText = root.value(QStringLiteral("output_text")).toString();
    if (!outputText.isEmpty()) {
        return outputText;
    }

    const QJsonArray output = root.value(QStringLiteral("output")).toArray();
    QStringList texts;
    for (const QJsonValue &itemValue : output) {
        const QJsonObject itemObject = itemValue.toObject();
        const QJsonArray content = itemObject.value(QStringLiteral("content")).toArray();
        for (const QJsonValue &contentValue : content) {
            const QString text = contentValue.toObject().value(QStringLiteral("text")).toString();
            if (!text.isEmpty()) {
                texts.append(text);
            }
        }
    }
    if (!texts.isEmpty()) {
        return texts.join(QStringLiteral("\n"));
    }

    if (errorMessage) {
        *errorMessage = tr("AI response format is not supported yet.");
    }
    return QString();
}

QString OpenAiCompatibleClient::contentValueToText(const QJsonValue &value) const
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isArray()) {
        QStringList texts;
        const QJsonArray array = value.toArray();
        for (const QJsonValue &itemValue : array) {
            if (itemValue.isString()) {
                texts.append(itemValue.toString());
                continue;
            }
            const QJsonObject object = itemValue.toObject();
            const QString text = object.value(QStringLiteral("text")).toString();
            if (!text.isEmpty()) {
                texts.append(text);
            }
        }
        return texts.join(QStringLiteral("\n"));
    }
    if (value.isObject()) {
        return value.toObject().value(QStringLiteral("text")).toString();
    }
    return QString();
}

QString OpenAiCompatibleClient::stripMarkdownFences(const QString &text) const
{
    QString normalized = text.trimmed();
    if (!normalized.startsWith(QStringLiteral("```"))) {
        return normalized;
    }

    const int firstNewline = normalized.indexOf('\n');
    if (firstNewline < 0) {
        return normalized;
    }

    normalized = normalized.mid(firstNewline + 1);
    const int closingFence = normalized.lastIndexOf(QStringLiteral("```"));
    if (closingFence >= 0) {
        normalized = normalized.left(closingFence);
    }
    return normalized.trimmed();
}

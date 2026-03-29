#include "src/model/project_session.h"

ProjectSession::ProjectSession(QObject *parent)
    : QObject(parent)
{
}

void ProjectSession::clear()
{
    m_openedFilePath.clear();
    m_nativeSource.clear();
    m_mergedSource.clear();
    m_disassemblyText.clear();
    m_logText.clear();
    m_statusMessage.clear();
    m_promptPreviewText.clear();
    m_codeObjectTree.clear();

    emit fileChanged();
    emit sourcesChanged();
    emit inspectChanged();
    emit treeChanged();
    emit logChanged();
    emit statusMessageChanged();
}

void ProjectSession::setOpenedFilePath(const QString &path)
{
    if (m_openedFilePath == path) {
        return;
    }
    m_openedFilePath = path;
    emit fileChanged();
}

void ProjectSession::setNativeSource(const QString &text)
{
    if (m_nativeSource == text) {
        return;
    }
    m_nativeSource = text;
    rebuildMergedSource();
    emit sourcesChanged();
}

void ProjectSession::setMergedSource(const QString &text)
{
    if (m_mergedSource == text) {
        return;
    }
    m_mergedSource = text;
    emit sourcesChanged();
}

void ProjectSession::setDisassemblyText(const QString &text)
{
    if (m_disassemblyText == text) {
        return;
    }
    m_disassemblyText = text;
    emit inspectChanged();
}

void ProjectSession::setStatusMessage(const QString &message)
{
    if (m_statusMessage == message) {
        return;
    }
    m_statusMessage = message;
    emit statusMessageChanged();
}

void ProjectSession::setPromptPreviewText(const QString &text)
{
    if (m_promptPreviewText == text) {
        return;
    }
    m_promptPreviewText = text;
    emit inspectChanged();
}

void ProjectSession::setCodeObjectTree(const QList<CodeObjectNode> &tree)
{
    m_codeObjectTree = tree;
    rebuildMergedSource();
    emit treeChanged();
    emit sourcesChanged();
}

void ProjectSession::appendLogLine(const QString &line)
{
    if (line.trimmed().isEmpty()) {
        return;
    }
    if (!m_logText.isEmpty()) {
        m_logText.append(QStringLiteral("\n"));
    }
    m_logText.append(line);
    emit logChanged();
}

bool ProjectSession::applyAiResultToNode(const QString &id,
                                         const QString &aiSource,
                                         CodeObjectNode::Status status)
{
    CodeObjectNode *node = findNodeByIdRecursive(m_codeObjectTree, id);
    if (!node) {
        return false;
    }

    node->aiSource = aiSource;
    if (status == CodeObjectNode::Status::AiReconstructed) {
        node->status = status;
    } else if (node->status == CodeObjectNode::Status::NativeFailed) {
        node->status = CodeObjectNode::Status::Partial;
    } else {
        node->status = status;
    }

    rebuildMergedSource();
    emit treeChanged();
    emit sourcesChanged();
    emit inspectChanged();
    return true;
}

const CodeObjectNode *ProjectSession::findNodeById(const QString &id) const
{
    return findNodeByIdRecursive(m_codeObjectTree, id);
}

void ProjectSession::rebuildMergedSource()
{
    m_mergedSource = buildMergedSourceDocument();
}

QString ProjectSession::buildMergedSourceDocument() const
{
    if (m_codeObjectTree.size() == 1) {
        const CodeObjectNode &root = m_codeObjectTree.first();
        if (root.objectType == QStringLiteral("module") && !root.aiSource.trimmed().isEmpty()) {
            return root.aiSource.trimmed();
        }
    }

    QStringList aiSections;
    collectAiSections(m_codeObjectTree, aiSections);
    if (aiSections.isEmpty()) {
        return m_nativeSource;
    }

    QString document = m_nativeSource.trimmed();
    if (document.isEmpty()) {
        document = QStringLiteral("# Native source is unavailable for this file.\n");
    }

    if (!document.endsWith(QLatin1Char('\n'))) {
        document.append(QLatin1Char('\n'));
    }
    document.append(QStringLiteral("\n# --- AI fallback patches ---\n"));
    document.append(QStringLiteral("# These snippets are reconstructed per code object and are not yet merged inline.\n\n"));
    document.append(aiSections.join(QStringLiteral("\n\n")));
    return document.trimmed();
}

void ProjectSession::collectAiSections(const QList<CodeObjectNode> &nodes, QStringList &sections) const
{
    for (const CodeObjectNode &node : nodes) {
        if (!node.aiSource.trimmed().isEmpty() && node.objectType != QStringLiteral("module")) {
            QString section;
            section += QStringLiteral("# [%1] %2\n").arg(node.objectType, node.qualifiedName);
            section += node.aiSource.trimmed();
            sections.append(section);
        }
        if (!node.children.isEmpty()) {
            collectAiSections(node.children, sections);
        }
    }
}

const CodeObjectNode *ProjectSession::findNodeByIdRecursive(const QList<CodeObjectNode> &nodes, const QString &id) const
{
    for (const CodeObjectNode &node : nodes) {
        if (node.id == id) {
            return &node;
        }
        if (const CodeObjectNode *child = findNodeByIdRecursive(node.children, id)) {
            return child;
        }
    }
    return nullptr;
}

CodeObjectNode *ProjectSession::findNodeByIdRecursive(QList<CodeObjectNode> &nodes, const QString &id)
{
    for (CodeObjectNode &node : nodes) {
        if (node.id == id) {
            return &node;
        }
        if (CodeObjectNode *child = findNodeByIdRecursive(node.children, id)) {
            return child;
        }
    }
    return nullptr;
}

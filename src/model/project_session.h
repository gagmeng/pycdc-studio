#ifndef PROJECT_SESSION_H
#define PROJECT_SESSION_H

#include <QObject>
#include <QString>
#include <QStringList>

#include "src/model/code_object_node.h"

class ProjectSession : public QObject
{
    Q_OBJECT

public:
    explicit ProjectSession(QObject *parent = nullptr);

    void clear();

    QString openedFilePath() const { return m_openedFilePath; }
    QString nativeSource() const { return m_nativeSource; }
    QString mergedSource() const { return m_mergedSource; }
    QString disassemblyText() const { return m_disassemblyText; }
    QString logText() const { return m_logText; }
    QString statusMessage() const { return m_statusMessage; }
    QString promptPreviewText() const { return m_promptPreviewText; }
    QList<CodeObjectNode> codeObjectTree() const { return m_codeObjectTree; }

    void setOpenedFilePath(const QString &path);
    void setNativeSource(const QString &text);
    void setMergedSource(const QString &text);
    void setDisassemblyText(const QString &text);
    void setStatusMessage(const QString &message);
    void setPromptPreviewText(const QString &text);
    void setCodeObjectTree(const QList<CodeObjectNode> &tree);
    void appendLogLine(const QString &line);
    bool applyAiResultToNode(const QString &id,
                             const QString &aiSource,
                             CodeObjectNode::Status status = CodeObjectNode::Status::AiReconstructed);

    const CodeObjectNode *findNodeById(const QString &id) const;

signals:
    void fileChanged();
    void sourcesChanged();
    void inspectChanged();
    void treeChanged();
    void logChanged();
    void statusMessageChanged();

private:
    void rebuildMergedSource();
    QString buildMergedSourceDocument() const;
    void collectAiSections(const QList<CodeObjectNode> &nodes, QStringList &sections) const;
    const CodeObjectNode *findNodeByIdRecursive(const QList<CodeObjectNode> &nodes, const QString &id) const;
    CodeObjectNode *findNodeByIdRecursive(QList<CodeObjectNode> &nodes, const QString &id);

    QString m_openedFilePath;
    QString m_nativeSource;
    QString m_mergedSource;
    QString m_disassemblyText;
    QString m_logText;
    QString m_statusMessage;
    QString m_promptPreviewText;
    QList<CodeObjectNode> m_codeObjectTree;
};

#endif // PROJECT_SESSION_H

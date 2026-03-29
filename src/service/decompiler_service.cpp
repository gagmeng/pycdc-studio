#include "src/service/decompiler_service.h"

#include <QFileInfo>

#include "src/service/pycdas_tree_parser.h"
#include "src/service/pycdc_process_runner.h"
#include "src/service/pycdas_process_runner.h"

namespace {
void applyStatusRecursive(QList<CodeObjectNode> &nodes, CodeObjectNode::Status status)
{
    for (CodeObjectNode &node : nodes) {
        node.status = status;
        if (!node.children.isEmpty()) {
            applyStatusRecursive(node.children, status);
        }
    }
}

void prefixNodeIdsRecursive(CodeObjectNode &node, const QString &prefix)
{
    node.id = prefix + QStringLiteral("|") + node.id;
    for (CodeObjectNode &child : node.children) {
        prefixNodeIdsRecursive(child, prefix);
    }
}

void prefixNodeIds(QList<CodeObjectNode> &nodes, const QString &filePath)
{
    const QString prefix = QStringLiteral("file:%1")
        .arg(QString::number(qHash(filePath), 16));
    for (CodeObjectNode &node : nodes) {
        prefixNodeIdsRecursive(node, prefix);
    }
}
}

DecompilerService::DecompilerService(ProjectSession &session,
                                     PycdcProcessRunner &pycdcRunner,
                                     PycdasProcessRunner &pycdasRunner,
                                     QObject *parent)
    : QObject(parent)
    , m_session(session)
    , m_pycdcRunner(pycdcRunner)
    , m_pycdasRunner(pycdasRunner)
{
}

bool DecompilerService::decompileFile(const QString &filePath)
{
    return decompileFiles(QStringList{filePath});
}

bool DecompilerService::decompileFiles(const QStringList &filePaths)
{
    if (filePaths.isEmpty()) {
        return false;
    }

    m_session.clear();
    m_session.setOpenedFilePath(filePaths.first());
    m_session.appendLogLine(tr("[runner] pycdas = %1").arg(m_pycdasRunner.program()));
    m_session.appendLogLine(tr("[runner] pycdc = %1").arg(m_pycdcRunner.program()));
    m_session.appendLogLine(tr("[session] loading %1 bytecode file(s)").arg(filePaths.size()));

    QList<CodeObjectNode> aggregatedRoots;
    QString firstNativeText;
    QString firstDisassemblyText;
    QString firstStatusMessage;
    bool anySuccess = false;

    for (int index = 0; index < filePaths.size(); ++index) {
        const QString &filePath = filePaths.at(index);
        m_session.setStatusMessage(tr("Running decompilers (%1/%2): %3")
                                   .arg(index + 1)
                                   .arg(filePaths.size())
                                   .arg(QFileInfo(filePath).fileName()));
        m_session.appendLogLine(tr("[session] opened %1").arg(filePath));

        const DecompileResult disassemblyResult = m_pycdasRunner.runFile(filePath);
        if (disassemblyResult.success) {
            m_session.appendLogLine(tr("[pycdas] %1 completed successfully").arg(QFileInfo(filePath).fileName()));
        } else {
            m_session.appendLogLine(tr("[pycdas] %1").arg(disassemblyResult.stderrText));
        }

        const DecompileResult nativeResult = m_pycdcRunner.runFile(filePath);
        if (nativeResult.success) {
            anySuccess = true;
            m_session.appendLogLine(tr("[pycdc] %1 completed successfully").arg(QFileInfo(filePath).fileName()));
        } else {
            m_session.appendLogLine(tr("[pycdc] %1").arg(nativeResult.stderrText));
        }

        QList<CodeObjectNode> roots = buildInitialTree(filePath,
                                                       disassemblyResult.stdoutText,
                                                       nativeResult.stdoutText,
                                                       nativeResult.stderrText,
                                                       nativeResult.success);
        prefixNodeIds(roots, filePath);
        aggregatedRoots.append(roots);

        if (index == 0) {
            firstDisassemblyText = disassemblyResult.displayText();
            firstNativeText = nativeResult.displayText();
            firstStatusMessage = nativeResult.success
                ? tr("Native decompilation finished.")
                : tr("Native decompilation failed. AI fallback can be added next.");
        }
    }

    m_session.setDisassemblyText(firstDisassemblyText);
    m_session.setNativeSource(firstNativeText);
    m_session.setMergedSource(firstNativeText);
    m_session.setCodeObjectTree(aggregatedRoots);

    if (filePaths.size() == 1) {
        m_session.setStatusMessage(firstStatusMessage);
    } else {
        m_session.setStatusMessage(tr("Loaded %1 bytecode files into the workspace.").arg(filePaths.size()));
    }

    return !aggregatedRoots.isEmpty() || anySuccess;
}

QList<CodeObjectNode> DecompilerService::buildInitialTree(const QString &filePath,
                                                          const QString &disassembly,
                                                          const QString &nativeSource,
                                                          const QString &nativeError,
                                                          bool nativeSuccess) const
{
    PycdasTreeParser parser;
    QList<CodeObjectNode> nodes = parser.parse(disassembly, filePath);
    const CodeObjectNode::Status initialStatus = nativeSuccess
        ? CodeObjectNode::Status::NativeOk
        : CodeObjectNode::Status::NativeFailed;
    if (nodes.isEmpty()) {
        QFileInfo info(filePath);

        CodeObjectNode moduleNode;
        moduleNode.id = QStringLiteral("module:%1").arg(info.completeBaseName());
        moduleNode.qualifiedName = QStringLiteral("<module>");
        moduleNode.displayName = info.fileName();
        moduleNode.objectType = QStringLiteral("module");
        moduleNode.sourceFile = filePath;
        moduleNode.disassembly = disassembly;
        moduleNode.nativeSource = nativeSource;
        moduleNode.mergedSource = nativeSource;
        moduleNode.nativeError = nativeError;
        moduleNode.status = initialStatus;
        return { moduleNode };
    }

    applyStatusRecursive(nodes, initialStatus);

    CodeObjectNode &root = nodes.first();
    root.sourceFile = filePath;
    root.disassembly = disassembly;
    root.nativeSource = nativeSource;
    root.mergedSource = nativeSource;
    root.nativeError = nativeError;
    root.status = initialStatus;
    return nodes;
}

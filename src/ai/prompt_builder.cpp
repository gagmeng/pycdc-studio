#include "src/ai/prompt_builder.h"

#include <QStringList>

#include "src/model/project_session.h"

namespace {
QString joinOrDash(const QStringList &values)
{
    return values.isEmpty() ? QStringLiteral("-") : values.join(QStringLiteral(", "));
}
}

QString PromptBuilder::buildForNode(const CodeObjectNode &node, const ProjectSession &session) const
{
    QString prompt;
    prompt += QStringLiteral("You are reconstructing Python source code from Python bytecode disassembly.\n\n");
    prompt += QStringLiteral("Requirements:\n");
    prompt += QStringLiteral("- Reconstruct the most likely valid Python source for this code object\n");
    prompt += QStringLiteral("- Prioritize semantics over formatting\n");
    prompt += QStringLiteral("- Do not invent APIs not implied by the metadata\n");
    prompt += QStringLiteral("- If uncertain, keep it simple and add a short comment\n");
    prompt += QStringLiteral("- Return only Python source for this code object\n\n");

    prompt += QStringLiteral("Context:\n");
    prompt += QStringLiteral("- Opened file: %1\n").arg(session.openedFilePath());
    prompt += QStringLiteral("- Qualified name: %1\n").arg(node.qualifiedName);
    prompt += QStringLiteral("- Object type: %1\n").arg(node.objectType);
    prompt += QStringLiteral("- First line: %1\n").arg(node.firstLine >= 0 ? QString::number(node.firstLine) : QStringLiteral("-"));
    prompt += QStringLiteral("- co_names: %1\n").arg(joinOrDash(node.coNames));
    prompt += QStringLiteral("- co_varnames: %1\n").arg(joinOrDash(node.coVarNames));
    prompt += QStringLiteral("- co_freevars: %1\n").arg(joinOrDash(node.coFreeVars));
    prompt += QStringLiteral("- co_cellvars: %1\n").arg(joinOrDash(node.coCellVars));
    prompt += QStringLiteral("- co_consts preview: %1\n\n").arg(joinOrDash(node.coConstsPreview));

    if (!node.nativeError.trimmed().isEmpty()) {
        prompt += QStringLiteral("Native decompiler error:\n%1\n\n").arg(node.nativeError.trimmed());
    }

    prompt += QStringLiteral("Disassembly:\n");
    prompt += node.disassembly.trimmed().isEmpty() ? session.disassemblyText() : node.disassembly;
    prompt += QStringLiteral("\n");
    return prompt;
}

#include "src/service/pycdas_tree_parser.h"

#include <QFileInfo>
#include <QStringList>

QList<CodeObjectNode> PycdasTreeParser::parse(const QString &text, const QString &filePath) const
{
    ParseState state;
    state.lines = text.split('\n');
    state.filePath = filePath;

    QList<CodeObjectNode> nodes;
    int index = 0;
    while (index < state.lines.size()) {
        const QString line = state.lines.at(index);
        if (line.trimmed() == QStringLiteral("[Code]")) {
            nodes.append(parseCodeObject(state, index, indentationLevel(line)));
            continue;
        }
        ++index;
    }

    for (CodeObjectNode &node : nodes) {
        finalizeNode(node, filePath, state.nextId);
    }
    return nodes;
}

CodeObjectNode PycdasTreeParser::parseCodeObject(ParseState &state, int &index, int codeIndent) const
{
    CodeObjectNode node;
    Section currentSection = Section::None;
    int sectionIndent = codeIndent;

    ++index;
    while (index < state.lines.size()) {
        const QString line = state.lines.at(index);
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            ++index;
            continue;
        }

        const int indent = indentationLevel(line);
        if (indent <= codeIndent) {
            break;
        }

        if (trimmed == QStringLiteral("[Code]")) {
            node.children.append(parseCodeObject(state, index, indent));
            continue;
        }

        if (isSectionHeader(trimmed)) {
            currentSection = classifySection(trimSectionName(trimmed));
            sectionIndent = indent;
            ++index;
            continue;
        }

        if (currentSection == Section::Disassembly) {
            if (indent <= sectionIndent) {
                currentSection = Section::None;
                continue;
            }
            if (!node.disassembly.isEmpty()) {
                node.disassembly.append('\n');
            }
            node.disassembly.append(trimmed);
            ++index;
            continue;
        }

        if (indent == codeIndent + 1 && trimmed.contains(QStringLiteral(": "))) {
            currentSection = Section::None;
            const QString fileName = extractFieldValue(trimmed, QStringLiteral("File Name"));
            if (!fileName.isEmpty()) {
                node.sourceFile = fileName;
                ++index;
                continue;
            }

            const QString objectName = extractFieldValue(trimmed, QStringLiteral("Object Name"));
            if (!objectName.isEmpty()) {
                node.displayName = objectName;
                ++index;
                continue;
            }

            const QString qualifiedName = extractFieldValue(trimmed, QStringLiteral("Qualified Name"));
            if (!qualifiedName.isEmpty()) {
                node.qualifiedName = qualifiedName;
                ++index;
                continue;
            }

            const QString firstLine = extractFieldValue(trimmed, QStringLiteral("First Line"));
            if (!firstLine.isEmpty()) {
                node.firstLine = firstLine.toInt();
                ++index;
                continue;
            }
        }

        const bool isValueSection = currentSection == Section::Names
            || currentSection == Section::VarNames
            || currentSection == Section::FreeVars
            || currentSection == Section::CellVars
            || currentSection == Section::Constants;
        if (indent > sectionIndent && isValueSection) {
            QString value = extractIndexedValue(trimmed);
            if (value.isEmpty()) {
                value = trimmed;
            }
            if (!value.isEmpty()) {
                appendListValue(currentSection, node, value);
            }
        }

        ++index;
    }

    return node;
}

void PycdasTreeParser::finalizeNode(CodeObjectNode &node, const QString &fallbackFilePath, int &nextId) const
{
    if (node.sourceFile.isEmpty()) {
        node.sourceFile = fallbackFilePath;
    }

    if (node.qualifiedName.isEmpty()) {
        if (node.displayName.isEmpty()) {
            node.qualifiedName = QStringLiteral("<module>");
        } else {
            node.qualifiedName = node.displayName;
        }
    }

    if (node.displayName.isEmpty()) {
        node.displayName = node.qualifiedName;
    }

    for (CodeObjectNode &child : node.children) {
        finalizeNode(child, fallbackFilePath, nextId);
    }

    node.objectType = deriveObjectType(node);
    if (node.objectType == QStringLiteral("module")) {
        node.displayName = QFileInfo(fallbackFilePath).fileName();
    }

    node.id = QStringLiteral("%1:%2").arg(node.objectType).arg(nextId++);
}

QString PycdasTreeParser::deriveObjectType(const CodeObjectNode &node) const
{
    const QString qualifiedName = node.qualifiedName;
    const QString displayName = node.displayName;

    if (qualifiedName == QStringLiteral("<module>") || displayName == QStringLiteral("<module>")) {
        return QStringLiteral("module");
    }
    if (qualifiedName.contains(QStringLiteral("<lambda>")) || displayName == QStringLiteral("<lambda>")) {
        return QStringLiteral("lambda");
    }
    if (qualifiedName.contains(QStringLiteral("<listcomp>"))
        || qualifiedName.contains(QStringLiteral("<dictcomp>"))
        || qualifiedName.contains(QStringLiteral("<setcomp>"))
        || qualifiedName.contains(QStringLiteral("<genexpr>"))) {
        return QStringLiteral("comprehension");
    }

    const QString nestedPrefix = qualifiedName + QStringLiteral(".<locals>.");
    const QString memberPrefix = qualifiedName + QLatin1Char('.');
    bool hasDirectMembers = false;
    for (const CodeObjectNode &child : node.children) {
        if (child.qualifiedName.startsWith(memberPrefix)
            && !child.qualifiedName.startsWith(nestedPrefix)) {
            hasDirectMembers = true;
            break;
        }
    }
    if (hasDirectMembers && !qualifiedName.contains(QStringLiteral("<locals>"))) {
        return QStringLiteral("class");
    }
    if (qualifiedName.contains(QLatin1Char('.')) && !qualifiedName.contains(QStringLiteral("<locals>"))) {
        return QStringLiteral("method");
    }
    return QStringLiteral("function");
}

void PycdasTreeParser::appendListValue(Section section, CodeObjectNode &node, const QString &value) const
{
    switch (section) {
    case Section::Names:
        node.coNames.append(value);
        break;
    case Section::VarNames:
        node.coVarNames.append(value);
        break;
    case Section::FreeVars:
        node.coFreeVars.append(value);
        break;
    case Section::CellVars:
        node.coCellVars.append(value);
        break;
    case Section::Constants:
        node.coConstsPreview.append(value);
        break;
    case Section::None:
    case Section::Disassembly:
    case Section::Other:
        break;
    }
}

int PycdasTreeParser::indentationLevel(const QString &line)
{
    int spaces = 0;
    while (spaces < line.size() && line.at(spaces) == QLatin1Char(' ')) {
        ++spaces;
    }
    return spaces / 4;
}

QString PycdasTreeParser::extractIndexedValue(const QString &line)
{
    const int separatorIndex = line.indexOf(QStringLiteral(": "));
    if (separatorIndex <= 0) {
        return QString();
    }

    bool ok = false;
    line.left(separatorIndex).toInt(&ok);
    if (!ok) {
        return QString();
    }
    return line.mid(separatorIndex + 2).trimmed();
}

QString PycdasTreeParser::extractFieldValue(const QString &line, const QString &fieldName)
{
    const QString prefix = fieldName + QStringLiteral(": ");
    if (!line.startsWith(prefix)) {
        return QString();
    }
    return line.mid(prefix.size()).trimmed();
}

QString PycdasTreeParser::trimSectionName(const QString &line)
{
    QString sectionName = line.trimmed();
    if (sectionName.startsWith(QLatin1Char('[')) && sectionName.endsWith(QLatin1Char(']'))) {
        sectionName = sectionName.mid(1, sectionName.size() - 2);
    }
    return sectionName.trimmed();
}

bool PycdasTreeParser::isSectionHeader(const QString &line)
{
    return line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'));
}

PycdasTreeParser::Section PycdasTreeParser::classifySection(const QString &sectionName)
{
    if (sectionName == QStringLiteral("Names")) {
        return Section::Names;
    }
    if (sectionName == QStringLiteral("Var Names")
        || sectionName == QStringLiteral("Locals+Names")
        || sectionName == QStringLiteral("Locals + Names")) {
        return Section::VarNames;
    }
    if (sectionName == QStringLiteral("Free Vars")) {
        return Section::FreeVars;
    }
    if (sectionName == QStringLiteral("Cell Vars")) {
        return Section::CellVars;
    }
    if (sectionName == QStringLiteral("Constants")) {
        return Section::Constants;
    }
    if (sectionName == QStringLiteral("Disassembly")) {
        return Section::Disassembly;
    }
    return Section::Other;
}

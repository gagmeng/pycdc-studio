#ifndef PYCDAS_TREE_PARSER_H
#define PYCDAS_TREE_PARSER_H

#include <QList>
#include <QString>
#include <QStringList>

#include "src/model/code_object_node.h"

class PycdasTreeParser
{
public:
    QList<CodeObjectNode> parse(const QString &text, const QString &filePath) const;

private:
    struct ParseState
    {
        QStringList lines;
        QString filePath;
        int nextId = 1;
    };

    enum class Section
    {
        None,
        Names,
        VarNames,
        FreeVars,
        CellVars,
        Constants,
        Disassembly,
        Other,
    };

    CodeObjectNode parseCodeObject(ParseState &state, int &index, int codeIndent) const;
    void finalizeNode(CodeObjectNode &node, const QString &fallbackFilePath, int &nextId) const;
    QString deriveObjectType(const CodeObjectNode &node) const;
    void appendListValue(Section section, CodeObjectNode &node, const QString &value) const;

    static int indentationLevel(const QString &line);
    static QString extractIndexedValue(const QString &line);
    static QString extractFieldValue(const QString &line, const QString &fieldName);
    static QString trimSectionName(const QString &line);
    static bool isSectionHeader(const QString &line);
    static Section classifySection(const QString &sectionName);
};

#endif // PYCDAS_TREE_PARSER_H

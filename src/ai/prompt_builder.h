#ifndef PROMPT_BUILDER_H
#define PROMPT_BUILDER_H

#include <QString>

#include "src/model/code_object_node.h"

class ProjectSession;

class PromptBuilder
{
public:
    QString buildForNode(const CodeObjectNode &node, const ProjectSession &session) const;
};

#endif // PROMPT_BUILDER_H

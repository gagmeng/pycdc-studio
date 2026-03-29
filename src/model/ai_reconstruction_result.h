#ifndef AI_RECONSTRUCTION_RESULT_H
#define AI_RECONSTRUCTION_RESULT_H

#include <QString>

struct AiReconstructionResult
{
    bool success = false;
    QString qualifiedName;
    QString promptText;
    QString responseText;
    QString finalSource;
    QString errorMessage;
};

#endif // AI_RECONSTRUCTION_RESULT_H

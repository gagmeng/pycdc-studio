#ifndef DECOMPILE_RESULT_H
#define DECOMPILE_RESULT_H

#include <QString>

struct DecompileResult
{
    bool success = false;
    int exitCode = -1;
    QString stdoutText;
    QString stderrText;
    QString errorType;
    QString failedQualifiedName;

    QString displayText() const
    {
        if (!stdoutText.trimmed().isEmpty()) {
            return stdoutText;
        }
        return stderrText;
    }
};

#endif // DECOMPILE_RESULT_H

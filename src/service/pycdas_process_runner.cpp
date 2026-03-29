#include "src/service/pycdas_process_runner.h"

#include <QProcess>

PycdasProcessRunner::PycdasProcessRunner(QObject *parent)
    : QObject(parent)
{
}

DecompileResult PycdasProcessRunner::runFile(const QString &filePath) const
{
    DecompileResult result;
    if (filePath.isEmpty()) {
        result.errorType = tr("invalid-input");
        result.stderrText = tr("Input file path is empty.");
        return result;
    }

    QProcess process;
    process.setProgram(m_program);
    process.setArguments({ filePath });
    process.start();

    if (!process.waitForStarted(3000)) {
        result.errorType = tr("process-start-failed");
        result.stderrText = tr("Failed to start pycdas from '%1'. Make sure the executable exists and is accessible.")
            .arg(m_program);
        return result;
    }

    process.waitForFinished(60000);
    result.exitCode = process.exitCode();
    result.stdoutText = QString::fromUtf8(process.readAllStandardOutput());
    result.stderrText = QString::fromUtf8(process.readAllStandardError());
    result.success = (process.exitStatus() == QProcess::NormalExit && result.exitCode == 0);

    if (!result.success && result.errorType.isEmpty()) {
        result.errorType = tr("disassemble-failed");
        if (result.stderrText.trimmed().isEmpty()) {
            result.stderrText = tr("pycdas exited with code %1.").arg(result.exitCode);
        }
    }

    return result;
}

#ifndef PYCDAS_PROCESS_RUNNER_H
#define PYCDAS_PROCESS_RUNNER_H

#include <QObject>
#include <QString>

#include "src/model/decompile_result.h"

class PycdasProcessRunner : public QObject
{
    Q_OBJECT

public:
    explicit PycdasProcessRunner(QObject *parent = nullptr);

    QString program() const { return m_program; }
    void setProgram(const QString &program) { m_program = program; }

    DecompileResult runFile(const QString &filePath) const;

private:
    QString m_program = QStringLiteral("pycdas");
};

#endif // PYCDAS_PROCESS_RUNNER_H

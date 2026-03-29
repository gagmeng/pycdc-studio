#ifndef DECOMPILER_SERVICE_H
#define DECOMPILER_SERVICE_H

#include <QObject>

#include "src/model/project_session.h"

class PycdcProcessRunner;
class PycdasProcessRunner;

class DecompilerService : public QObject
{
    Q_OBJECT

public:
    explicit DecompilerService(ProjectSession &session,
                               PycdcProcessRunner &pycdcRunner,
                               PycdasProcessRunner &pycdasRunner,
                               QObject *parent = nullptr);

    bool decompileFile(const QString &filePath);
    bool decompileFiles(const QStringList &filePaths);

private:
    QList<CodeObjectNode> buildInitialTree(const QString &filePath,
                                           const QString &disassembly,
                                           const QString &nativeSource,
                                           const QString &nativeError,
                                           bool nativeSuccess) const;

    ProjectSession &m_session;
    PycdcProcessRunner &m_pycdcRunner;
    PycdasProcessRunner &m_pycdasRunner;
};

#endif // DECOMPILER_SERVICE_H

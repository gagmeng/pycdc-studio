#include "src/app/app_context.h"

#include <QFileInfo>
#include <QProcessEnvironment>

namespace {
QString chooseProgramPath(const QString &envVarName,
                         const QString &releasePath,
                         const QString &defaultProgram)
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString envValue = env.value(envVarName).trimmed();
    if (!envValue.isEmpty()) {
        return envValue;
    }
    if (QFileInfo::exists(releasePath)) {
        return releasePath;
    }
    return defaultProgram;
}
}

AppContext::AppContext(QObject *parent)
    : QObject(parent)
    , m_session(this)
    , m_aiClient(this)
    , m_pycdcRunner(this)
    , m_pycdasRunner(this)
    , m_decompilerService(m_session, m_pycdcRunner, m_pycdasRunner, this)
    , m_fallbackService(m_session, m_promptBuilder, m_aiClient, this)
{
    m_pycdcRunner.setProgram(chooseProgramPath(QStringLiteral("PYCDC_STUDIO_PYCDC"),
                                               QStringLiteral("D:/code/pycdc/Release/pycdc.exe"),
                                               QStringLiteral("pycdc")));
    m_pycdasRunner.setProgram(chooseProgramPath(QStringLiteral("PYCDC_STUDIO_PYCDAS"),
                                                QStringLiteral("D:/code/pycdc/Release/pycdas.exe"),
                                                QStringLiteral("pycdas")));
}

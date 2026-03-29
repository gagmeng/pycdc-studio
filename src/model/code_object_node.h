#ifndef CODE_OBJECT_NODE_H
#define CODE_OBJECT_NODE_H

#include <QList>
#include <QString>
#include <QStringList>

struct CodeObjectNode
{
    enum class Status {
        Unknown,
        NativeOk,
        NativeFailed,
        AiReconstructed,
        Partial,
    };

    QString id;
    QString qualifiedName;
    QString displayName;
    QString objectType;
    QString sourceFile;
    int firstLine = -1;

    QString disassembly;
    QString nativeSource;
    QString aiSource;
    QString mergedSource;
    QString nativeError;

    QStringList coNames;
    QStringList coVarNames;
    QStringList coFreeVars;
    QStringList coCellVars;
    QStringList coConstsPreview;

    Status status = Status::Unknown;
    QList<CodeObjectNode> children;

    QString statusText() const
    {
        switch (status) {
        case Status::NativeOk: return QStringLiteral("Native");
        case Status::NativeFailed: return QStringLiteral("Failed");
        case Status::AiReconstructed: return QStringLiteral("AI");
        case Status::Partial: return QStringLiteral("Partial");
        case Status::Unknown:
        default:
            return QStringLiteral("Unknown");
        }
    }
};

#endif // CODE_OBJECT_NODE_H

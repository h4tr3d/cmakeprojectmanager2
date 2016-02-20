#ifndef CMAKEPROJECTMANAGER_CMAKEPARAMSEXT_H
#define CMAKEPROJECTMANAGER_CMAKEPARAMSEXT_H

#include <memory>

#include <QtCore>
#include <QTemporaryFile>
#include <QStringList>

namespace CMakeProjectManager {

enum class CMakeToolchainOverrideType
{
    Disabled,
    File,
    Inline
};

struct CMakeToolchainInfo
{
    CMakeToolchainOverrideType      toolchainOverride = CMakeToolchainOverrideType::Disabled;
    QString                         toolchainFile;
    QString                         toolchainInline;

    QString arguments(const QString &userArguments, const QString& buildDirectory) const;
    QStringList arguments(const QStringList &userArguments, const QString &buildDirectory) const;

    QString toolchainArgument(const QString& buildDirectory) const;

    bool operator==(const CMakeToolchainInfo& other) const;
    bool operator!=(const CMakeToolchainInfo& other) const;
};

} // namespace CMakeProjectManager

#endif // CMAKEPROJECTMANAGER_CMAKEPARAMSEXT_H

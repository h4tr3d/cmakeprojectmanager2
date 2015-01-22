#ifndef CMAKEPROJECTMANAGER_CMAKEPARAMSEXT_H
#define CMAKEPROJECTMANAGER_CMAKEPARAMSEXT_H

#include <memory>

#include <QtCore>
#include <QTemporaryFile>

namespace CMakeProjectManager {

enum class CMakeBuildType
{
    Default,
    Debug,
    Release,
    ReleaseWithDebugInfo,
    MinSizeRelease
};

enum class CMakeToolchainOverrideType
{
    Disabled,
    QtcKit,
    File,
    Inline
};

struct CMakeParamsExt
{
    CMakeBuildType                  buildType         = CMakeBuildType::Default;
    CMakeToolchainOverrideType      toolchainOverride = CMakeToolchainOverrideType::Disabled;
    QString                         toolchainFile;
    QString                         toolchainInline;

    QString arguments(const QString &userArguments, const QString& buildDirectory) const;
    bool operator==(const CMakeParamsExt& other) const;
    bool operator!=(const CMakeParamsExt& other) const;
};

} // namespace CMakeProjectManager

//Q_DECLARE_METATYPE(CMakeProjectManager::CMakeParamsExt)
Q_DECLARE_METATYPE(CMakeProjectManager::CMakeBuildType)

#endif // CMAKEPROJECTMANAGER_CMAKEPARAMSEXT_H

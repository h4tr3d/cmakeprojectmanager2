#include "cmakeparamsext.h"

#include <QMap>
#include <QDebug>

namespace CMakeProjectManager {

const QMap<CMakeBuildType, const char*> s_buildTypeMap = {
    {CMakeBuildType::Debug,               "Debug"},
    {CMakeBuildType::Release,             "Release"},
    {CMakeBuildType::ReleaseWithDebugInfo, "RelWithDebInfo"},
    {CMakeBuildType::MinSizeRelease,      "MinSizeRel"}
};

namespace {

} // ::<Empty>

QString CMakeParamsExt::arguments(const QString &userArguments, const QString &buildDirectory) const
{
    static_cast<void>(userArguments);

    QString result;

    if (buildType != CMakeBuildType::Default &&
        userArguments.toLower().contains(QLatin1String("-dcmake_build_type=")) == false) {
        result += QString(QLatin1String("-DCMAKE_BUILD_TYPE=%1 "))
                  .arg(QLatin1String(s_buildTypeMap[buildType]));

    }

    if (toolchainOverride != CMakeToolchainOverrideType::Disabled &&
        userArguments.toLower().contains(QLatin1String("-dcmake_toolchain_file=")) == false) {

        QString fileName;

        if (toolchainOverride == CMakeToolchainOverrideType::File) {
            fileName = toolchainFile;
        } else if (toolchainOverride == CMakeToolchainOverrideType::Inline) {
            QString overrideFileName = buildDirectory + QLatin1String("/QtCreator-toolchain-override.cmake");
            QFile   file(overrideFileName);

            if (file.open(QIODevice::WriteOnly)) {
                // Store content
                file.write(toolchainInline.toLocal8Bit());
                fileName = overrideFileName;
                file.close();
            } else {
                // TODO: notify error
            }

        } else if (toolchainOverride == CMakeToolchainOverrideType::QtcKit) {
            // TODO:
        }

        if (fileName.isEmpty() == false)
            result += QString(QLatin1String("-DCMAKE_TOOLCHAIN_FILE=%1 ")).arg(fileName);
    }

    result += userArguments;
    qDebug() << "Composed arguments: " << result;

    return result;
}

bool CMakeParamsExt::operator==(const CMakeParamsExt &other) const
{
    bool result = (buildType == other.buildType &&
                   toolchainOverride == other.toolchainOverride &&
                   toolchainFile == other.toolchainFile &&
                   toolchainInline == other.toolchainInline);
    return result;
}

bool CMakeParamsExt::operator!=(const CMakeParamsExt &other) const
{
    return !operator==(other);
}


} // ::CMakeProjectManager

#include "cmaketoolchaininfo.h"

#include <QMap>
#include <QDebug>

namespace CMakeProjectManager {

namespace {

} // ::<Empty>

QString CMakeToolchainInfo::arguments(const QString &userArguments, const QString &buildDirectory) const
{
    QString result;

    if (toolchainOverride != CMakeToolchainOverrideType::Disabled &&
        userArguments.toLower().contains(QLatin1String("-dcmake_toolchain_file=")) == false) {

        result += toolchainArgument(buildDirectory);
    }

    result += userArguments;
    qDebug() << "Composed arguments: " << result;

    return result;
}

QStringList CMakeToolchainInfo::arguments(const QStringList &userArguments, const QString &buildDirectory) const
{
    QStringList result;
    if (toolchainOverride != CMakeToolchainOverrideType::Disabled) {
        if (userArguments.contains(QLatin1String("cmake_toolchain_file"), Qt::CaseInsensitive) == false) {
            result << toolchainArgument(buildDirectory);
        }
    }
    qDebug() << "Composed arguments: " << result;
    return result;
}

QString CMakeToolchainInfo::toolchainArgument(const QString &buildDirectory) const
{
    QString result;
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
    }

    if (fileName.isEmpty() == false)
        result = QString(QLatin1String("-DCMAKE_TOOLCHAIN_FILE=%1 ")).arg(fileName);

    return result;
}

bool CMakeToolchainInfo::operator==(const CMakeToolchainInfo &other) const
{
    bool result = (toolchainOverride == other.toolchainOverride &&
                   toolchainFile == other.toolchainFile &&
                   toolchainInline == other.toolchainInline);
    return result;
}

bool CMakeToolchainInfo::operator!=(const CMakeToolchainInfo &other) const
{
    return !operator==(other);
}


} // ::CMakeProjectManager

/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "cmakeprojectnodes.h"

#include "cmakebuildsystem.h"
#include "cmakeprojectconstants.h"

#include <android/androidconstants.h>
#include <coreplugin/fileiconprovider.h>
#include <ios/iosconstants.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/target.h>

#include <utils/qtcassert.h>

using namespace ProjectExplorer;

namespace CMakeProjectManager {
namespace Internal {

CMakeInputsNode::CMakeInputsNode(const Utils::FilePath &cmakeLists) :
    ProjectExplorer::ProjectNode(cmakeLists)
{
    setPriority(Node::DefaultPriority - 10); // Bottom most!
    setDisplayName(QCoreApplication::translate("CMakeFilesProjectNode", "CMake Modules"));
    setIcon(DirectoryIcon(ProjectExplorer::Constants::FILEOVERLAY_MODULES));
    setListInProject(false);
}

CMakeListsNode::CMakeListsNode(const Utils::FilePath &cmakeListPath) :
    ProjectExplorer::ProjectNode(cmakeListPath)
{
    setIcon(DirectoryIcon(Constants::FILE_OVERLAY_CMAKE));
    setListInProject(false);
}

bool CMakeListsNode::showInSimpleTree() const
{
    return false;
}

Utils::optional<Utils::FilePath> CMakeListsNode::visibleAfterAddFileAction() const
{
    return filePath().pathAppended("CMakeLists.txt");
}

CMakeProjectNode::CMakeProjectNode(const Utils::FilePath &directory) :
    ProjectExplorer::ProjectNode(directory)
{
    setPriority(Node::DefaultProjectPriority + 1000);
    setIcon(DirectoryIcon(ProjectExplorer::Constants::FILEOVERLAY_PRODUCT));
    setListInProject(false);
}

QString CMakeProjectNode::tooltip() const
{
    return QString();
}

CMakeTargetNode::CMakeTargetNode(const Utils::FilePath &directory, const QString &target) :
    ProjectExplorer::ProjectNode(directory)
{
    m_target = target;
    setPriority(Node::DefaultProjectPriority + 900);
    setIcon(":/projectexplorer/images/build.png"); // TODO: Use proper icon!
    setListInProject(false);
    setProductType(ProductType::Other);
}

QString CMakeTargetNode::tooltip() const
{
    return m_tooltip;
}

QString CMakeTargetNode::buildKey() const
{
    return m_target;
}

Utils::FilePath CMakeTargetNode::buildDirectory() const
{
    return m_buildDirectory;
}

void CMakeTargetNode::setBuildDirectory(const Utils::FilePath &directory)
{
    m_buildDirectory = directory;
}

QVariant CMakeTargetNode::data(Utils::Id role) const
{
    auto value = [this](const QByteArray &key) -> QVariant {
        for (const CMakeConfigItem &configItem : m_config) {
            if (configItem.key == key)
                return configItem.value;
        }
        return {};
    };

    auto values = [this](const QByteArray &key) -> QVariant {
        for (const CMakeConfigItem &configItem : m_config) {
            if (configItem.key == key)
                return configItem.values;
        }
        return {};
    };

    if (role == Android::Constants::AndroidPackageSourceDir)
        return value(Android::Constants::ANDROID_PACKAGE_SOURCE_DIR);

    if (role == Android::Constants::AndroidDeploySettingsFile)
        return value(Android::Constants::ANDROID_DEPLOYMENT_SETTINGS_FILE);

    if (role == Android::Constants::AndroidExtraLibs)
        return value(Android::Constants::ANDROID_EXTRA_LIBS);

    if (role == Android::Constants::ANDROID_APPLICATION_ARGUMENTS)
        return value(Android::Constants::QT_ANDROID_APPLICATION_ARGUMENTS);

    if (role == Android::Constants::AndroidArch)
        return value(Android::Constants::ANDROID_ABI);

    if (role == Android::Constants::AndroidSoLibPath)
        return values(Android::Constants::ANDROID_SO_LIBS_PATHS);

    if (role == Android::Constants::AndroidTargets)
        return values("TARGETS_BUILD_PATH");

    if (role == Ios::Constants::IosTarget) {
        // For some reason the artifact is e.g. "Debug/untitled.app/untitled" which is wrong.
        // It actually is e.g. "Debug-iphonesimulator/untitled.app/untitled".
        // Anyway, the iOS plugin is only interested in the app bundle name without .app.
        return m_artifact.fileName();
    }

    if (role == Ios::Constants::IosBuildDir)
        return {}; // defaults to build configuration build directory

    QTC_ASSERT(false, qDebug() << "Unknown role" << role.toString());
    // Better guess than "not present".
    return value(role.toString().toUtf8());
}

void CMakeTargetNode::setConfig(const CMakeConfig &config)
{
    m_config = config;
}

Utils::optional<Utils::FilePath> CMakeTargetNode::visibleAfterAddFileAction() const
{
    return filePath().pathAppended("CMakeLists.txt");
}

void CMakeTargetNode::build()
{
    Project *p = getProject();
    Target *t = p ? p->activeTarget() : nullptr;
    if (t)
        static_cast<CMakeBuildSystem *>(t->buildSystem())->buildCMakeTarget(displayName());
}

void CMakeTargetNode::setTargetInformation(const QList<Utils::FilePath> &artifacts,
                                           const QString &type)
{
    m_tooltip = QCoreApplication::translate("CMakeTargetNode", "Target type: ") + type + "<br>";
    if (artifacts.isEmpty()) {
        m_tooltip += QCoreApplication::translate("CMakeTargetNode", "No build artifacts");
    } else {
        const QStringList tmp = Utils::transform(artifacts, &Utils::FilePath::toUserOutput);
        m_tooltip += QCoreApplication::translate("CMakeTargetNode", "Build artifacts:") + "<br>"
                + tmp.join("<br>");
        m_artifact = artifacts.first();
    }
}

} // Internal
} // CMakeBuildSystem

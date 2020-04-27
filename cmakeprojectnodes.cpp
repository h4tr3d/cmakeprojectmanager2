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

#include "cmakeconfigitem.h"
#include "cmakeproject.h"
#include "cmakeprojectconstants.h"
#include "cmakeprojectplugin.h"
#include "cmakespecificsettings.h"

#include <android/androidconstants.h>

#include <coreplugin/fileiconprovider.h>
#include <coreplugin/icore.h>
#include <cpptools/cpptoolsconstants.h>

#include <projectexplorer/target.h>

#include <utils/algorithm.h>
#include <utils/checkablemessagebox.h>
#include <utils/mimetypes/mimedatabase.h>
#include <utils/optional.h>
#include <utils/qtcassert.h>

#include <QClipboard>
#include <QDir>
#include <QGuiApplication>
#include <QMessageBox>

using namespace ProjectExplorer;

namespace CMakeProjectManager {
namespace Internal {

namespace {
void copySourcePathToClipboard(Utils::optional<QString> srcPath,
                               const ProjectExplorer::ProjectNode *node)
{
    QClipboard *clip = QGuiApplication::clipboard();

    QDir projDir{node->filePath().toFileInfo().absoluteFilePath()};
    clip->setText(QDir::cleanPath(projDir.relativeFilePath(srcPath.value())));
}

void noAutoAdditionNotify(const QStringList &filePaths, const ProjectExplorer::ProjectNode *node)
{
    Utils::optional<QString> srcPath{};

    for (const QString &file : filePaths) {
        if (Utils::mimeTypeForFile(file).name() == CppTools::Constants::CPP_SOURCE_MIMETYPE) {
            srcPath = file;
            break;
        }
    }

    if (srcPath) {
        CMakeSpecificSettings *settings = CMakeProjectPlugin::projectTypeSpecificSettings();
        switch (settings->afterAddFileSetting()) {
        case CMakeProjectManager::Internal::ASK_USER: {
            bool checkValue{false};
            QDialogButtonBox::StandardButton reply =
                Utils::CheckableMessageBox::question(nullptr,
                                                     QMessageBox::tr("Copy to Clipboard?"),
                                                     QMessageBox::tr("Files are not automatically added to the "
                                                                     "CMakeLists.txt file of the CMake project."
                                                                     "\nCopy the path to the source files to the clipboard?"),
                                                     "Remember My Choice", &checkValue, QDialogButtonBox::Yes | QDialogButtonBox::No,
                                                     QDialogButtonBox::Yes);
            if (checkValue) {
                if (QDialogButtonBox::Yes == reply)
                    settings->setAfterAddFileSetting(AfterAddFileAction::COPY_FILE_PATH);
                else if (QDialogButtonBox::No == reply)
                    settings->setAfterAddFileSetting(AfterAddFileAction::NEVER_COPY_FILE_PATH);

                settings->toSettings(Core::ICore::settings());
            }

            if (QDialogButtonBox::Yes == reply) {
                copySourcePathToClipboard(srcPath, node);
            }
            break;
        }

        case CMakeProjectManager::Internal::COPY_FILE_PATH: {
            copySourcePathToClipboard(srcPath, node);
            break;
        }

        case CMakeProjectManager::Internal::NEVER_COPY_FILE_PATH:
            break;
        }
    }
}

}

CMakeInputsNode::CMakeInputsNode(const Utils::FilePath &cmakeLists) :
    ProjectExplorer::ProjectNode(cmakeLists)
{
    setPriority(Node::DefaultPriority - 10); // Bottom most!
    setDisplayName(QCoreApplication::translate("CMakeFilesProjectNode", "CMake Modules"));
    setIcon(QIcon(":/projectexplorer/images/session.png")); // TODO: Use a better icon!
    setListInProject(false);
}

CMakeListsNode::CMakeListsNode(const Utils::FilePath &cmakeListPath) :
    ProjectExplorer::ProjectNode(cmakeListPath)
{
    static QIcon folderIcon = Core::FileIconProvider::directoryIcon(Constants::FILE_OVERLAY_CMAKE);
    setIcon(folderIcon);
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
    setIcon(QIcon(":/projectexplorer/images/projectexplorer.png")); // TODO: Use proper icon!
    setListInProject(false);
}

QString CMakeProjectNode::tooltip() const
{
    return QString();
}

bool CMakeBuildSystem::addFiles(Node *context, const QStringList &filePaths, QStringList *notAdded)
{
    if (auto n = dynamic_cast<CMakeProjectNode *>(context)) {
        noAutoAdditionNotify(filePaths, n);
        return true; // Return always true as autoadd is not supported!
    }

    if (auto n = dynamic_cast<CMakeTargetNode *>(context)) {
        noAutoAdditionNotify(filePaths, n);
        return true; // Return always true as autoadd is not supported!
    }

    return BuildSystem::addFiles(context, filePaths, notAdded);
}

CMakeTargetNode::CMakeTargetNode(const Utils::FilePath &directory, const QString &target) :
    ProjectExplorer::ProjectNode(directory)
{
    m_target = target;
    setPriority(Node::DefaultProjectPriority + 900);
    setIcon(QIcon(":/projectexplorer/images/build.png")); // TODO: Use proper icon!
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

QVariant CMakeTargetNode::data(Core::Id role) const
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
        return value("ANDROID_PACKAGE_SOURCE_DIR");

    if (role == Android::Constants::AndroidDeploySettingsFile)
        return value("ANDROID_DEPLOYMENT_SETTINGS_FILE");

    if (role == Android::Constants::AndroidExtraLibs)
        return value("ANDROID_EXTRA_LIBS");

    if (role == Android::Constants::AndroidArch)
        return value("ANDROID_ABI");

    if (role == Android::Constants::AndroidSoLibPath)
        return values("ANDROID_SO_LIBS_PATHS");

    if (role == Android::Constants::AndroidTargets)
        return values("TARGETS_BUILD_PATH");

    QTC_CHECK(false);
    // Better guess than "not present".
    return value(role.toString().toUtf8());
}

void CMakeTargetNode::setConfig(const CMakeConfig &config)
{
    m_config = config;
}

bool CMakeBuildSystem::supportsAction(Node *context, ProjectAction action, const Node *node) const
{
    if (dynamic_cast<CMakeTargetNode *>(context))
        return action == ProjectAction::AddNewFile;

    if (dynamic_cast<CMakeListsNode *>(context))
        return action == ProjectAction::AddNewFile;

    return BuildSystem::supportsAction(context, action, node);
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
    }
}

} // Internal
} // CMakeBuildSystem

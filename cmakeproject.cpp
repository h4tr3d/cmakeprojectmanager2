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

#include "cmakeproject.h"

#include "cmakebuildconfiguration.h"
#include "cmakekitinformation.h"
#include "cmakeprojectconstants.h"
#include "cmakeprojectnodes.h"
#include "cmakerunconfiguration.h"
#include "cmakeprojectmanager.h"
#include "compat.h"

#include <coreplugin/icore.h>
#include <coreplugin/progressmanager/progressmanager.h>
#include <cpptools/cppmodelmanager.h>
#include <cpptools/generatedcodemodelsupport.h>
#include <cpptools/projectinfo.h>
#include <cpptools/cpptoolsconstants.h>
#include <cpptools/projectpartbuilder.h>
#include <projectexplorer/buildtargetinfo.h>
#include <projectexplorer/deploymentdata.h>
#include <projectexplorer/headerpath.h>
#include <projectexplorer/kitinformation.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/target.h>
#include <projectexplorer/toolchain.h>
#include <qtsupport/baseqtversion.h>
#include <qtsupport/qtkitinformation.h>
#include <texteditor/textdocument.h>
#include <qmljs/qmljsmodelmanagerinterface.h>

#include <utils/algorithm.h>
#include <utils/qtcassert.h>
#include <utils/stringutils.h>
#include <utils/hostosinfo.h>

#include <QDir>
#include <QSet>
#include <QMessageBox>

using namespace ProjectExplorer;
using namespace Utils;

namespace CMakeProjectManager {

using namespace Internal;

// QtCreator CMake Generator wishlist:
// Which make targets we need to build to get all executables
// What is the actual compiler executable
// DEFINES

/*!
  \class CMakeProject
*/
CMakeProject::CMakeProject(CMakeManager *manager, const FileName &fileName)
{
    setId(CMakeProjectManager::Constants::CMAKEPROJECT_ID);
    setProjectManager(manager);
    setDocument(new TextEditor::TextDocument);
    document()->setFilePath(fileName);

    setRootProjectNode(new CMakeListsNode(fileName, this));
    setProjectContext(Core::Context(CMakeProjectManager::Constants::PROJECTCONTEXT));
    setProjectLanguages(Core::Context(ProjectExplorer::Constants::LANG_CXX));

    rootProjectNode()->setDisplayName(fileName.parentDir().fileName());

    connect(this, &CMakeProject::activeTargetChanged, this, &CMakeProject::handleActiveTargetChanged);

    connect(&m_treeScanner, &TreeScanner::finished, this, &CMakeProject::handleTreeScanningFinished);

    m_treeScanner.setFilter([this](const Utils::MimeType &mimeType, const Utils::FileName &fn) {
        // Mime checks requires more resources, so keep it last in check list
        auto isIgnored =
                fn.toString().startsWith(projectFilePath().toString() + ".user") ||
                TreeScanner::isWellKnownBinary(mimeType, fn);

        // Cache mime check result for speed up
        if (!isIgnored) {
            auto it = m_mimeBinaryCache.find(mimeType.name());
            if (it != m_mimeBinaryCache.end()) {
                isIgnored = *it;
            } else {
                isIgnored = TreeScanner::isMimeBinary(mimeType, fn);
                m_mimeBinaryCache[mimeType.name()] = isIgnored;
            }
        }

        return isIgnored;
    });

    m_treeScanner.setTypeFactory([](const Utils::MimeType &mimeType, const Utils::FileName &fn) {
        auto type = TreeScanner::genericFileType(mimeType, fn);
        if (type == FileType::Unknown) {
            if (mimeType.isValid()) {
                const QString mt = mimeType.name();
                if (mt == CMakeProjectManager::Constants::CMAKEPROJECTMIMETYPE
                    || mt == CMakeProjectManager::Constants::CMAKEMIMETYPE)
                    type = FileType::Project;
            }
        }
        return type;
    });

    scanProjectTree();
}

CMakeProject::~CMakeProject()
{
    setRootProjectNode(nullptr);
    m_codeModelFuture.cancel();
    qDeleteAll(m_extraCompilers);
    qDeleteAll(m_allFiles);
}

void CMakeProject::updateProjectData(CMakeBuildConfiguration *bc)
{
    QTC_ASSERT(bc, return);

    Target *t = activeTarget();
    if (!t || t->activeBuildConfiguration() != bc)
        return;

    if (!m_treeScanner.isFinished() || bc->isParsing())
        return;

    Kit *k = t->kit();

    bc->generateProjectTree(static_cast<CMakeListsNode *>(rootProjectNode()), m_allFiles);

    updateApplicationAndDeploymentTargets();
    updateTargetRunConfigurations(t);

    createGeneratedCodeModelSupport();

    ToolChain *tc = ToolChainKitInformation::toolChain(k, ToolChain::Language::Cxx);
    if (!tc) {
        emit fileListChanged();
        return;
    }

    CppTools::CppModelManager *modelmanager = CppTools::CppModelManager::instance();
    CppTools::ProjectInfo pinfo(this);
    CppTools::ProjectPartBuilder ppBuilder(pinfo);

    CppTools::ProjectPart::QtVersion activeQtVersion = CppTools::ProjectPart::NoQt;
    if (QtSupport::BaseQtVersion *qtVersion = QtSupport::QtKitInformation::qtVersion(k)) {
        if (qtVersion->qtVersion() < QtSupport::QtVersionNumber(5,0,0))
            activeQtVersion = CppTools::ProjectPart::Qt4;
        else
            activeQtVersion = CppTools::ProjectPart::Qt5;
    }

    ppBuilder.setQtVersion(activeQtVersion);

    const QSet<Core::Id> languages = bc->updateCodeModel(ppBuilder);
    for (const auto &lid : languages)
        setProjectLanguage(lid, true);

    m_codeModelFuture.cancel();
    m_codeModelFuture = modelmanager->updateProjectInfo(pinfo);

    updateQmlJSCodeModel();

    emit displayNameChanged();
    emit fileListChanged();

    emit bc->emitBuildTypeChanged();

    emit parsingFinished();
}

void CMakeProject::updateQmlJSCodeModel()
{
    QmlJS::ModelManagerInterface *modelManager = QmlJS::ModelManagerInterface::instance();
    QTC_ASSERT(modelManager, return);

    if (!activeTarget() || !activeTarget()->activeBuildConfiguration())
        return;

    QmlJS::ModelManagerInterface::ProjectInfo projectInfo =
            modelManager->defaultProjectInfoForProject(this);

    projectInfo.importPaths.clear();

    QString cmakeImports;
    CMakeBuildConfiguration *bc = qobject_cast<CMakeBuildConfiguration *>(activeTarget()->activeBuildConfiguration());
    if (!bc)
        return;

    const QList<ConfigModel::DataItem> &cm = bc->completeCMakeConfiguration();
    foreach (const ConfigModel::DataItem &di, cm) {
        if (di.key.contains(QStringLiteral("QML_IMPORT_PATH"))) {
            cmakeImports = di.value;
            break;
        }
    }

    foreach (const QString &cmakeImport, CMakeConfigItem::cmakeSplitValue(cmakeImports))
        projectInfo.importPaths.maybeInsert(FileName::fromString(cmakeImport),QmlJS::Dialect::Qml);

    modelManager->updateProjectInfo(projectInfo, this);
}

void CMakeProject::askRunCMake()
{
    QPointer<QMessageBox> box = new QMessageBox(Core::ICore::mainWindow());
    box->setIcon(QMessageBox::Question);
    box->setText(tr("Project file system tree changed."));
    box->setInformativeText(tr("File system tree changed. The change does not affect CMake project targets.\n"
                               "Edit appropriate CMakeLists.txt or run CMake now if project uses globbing expressions to collect files."));
    box->addButton(tr("Run CMake Later"), QMessageBox::RejectRole);
    auto defaultButton = box->addButton(tr("Run CMake Now"), QMessageBox::AcceptRole);
    box->setDefaultButton(defaultButton);

    int ret = box->exec();
    if (ret == QMessageBox::Accepted)
        runCMake();
}

bool CMakeProject::needsConfiguration() const
{
    return targets().isEmpty();
}

bool CMakeProject::requiresTargetPanel() const
{
    return !targets().isEmpty();
}

bool CMakeProject::knowsAllBuildExecutables() const
{
    return false;
}

bool CMakeProject::supportsKit(Kit *k, QString *errorMessage) const
{
    if (!CMakeKitInformation::cmakeTool(k)) {
        if (errorMessage)
            *errorMessage = tr("No cmake tool set.");
        return false;
    }
    return true;
}

void CMakeProject::runCMake()
{
    CMakeBuildConfiguration *bc = nullptr;
    if (activeTarget())
        bc = qobject_cast<CMakeBuildConfiguration *>(activeTarget()->activeBuildConfiguration());

    if (bc)
        bc->runCMake();
}

void CMakeProject::buildCMakeTarget(const QString &buildTarget)
{
    QTC_ASSERT(!buildTarget.isEmpty(), return);
    Target *t = activeTarget();
    auto bc = qobject_cast<CMakeBuildConfiguration *>(t ? t->activeBuildConfiguration() : nullptr);
    if (bc)
        bc->buildTarget(buildTarget);
}

bool CMakeProject::addFiles(const QStringList &filePaths)
{
    Utils::MimeDatabase mdb;
    QList<const FileNode *> nodes; // nodes to store in persistent tree
    for (auto &filePath : filePaths) {
        const Utils::MimeType mimeType = mdb.mimeTypeForFile(filePath);
        auto fn = FileName::fromString(filePath);
        auto type = TreeScanner::genericFileType(mimeType, fn);
        auto node = new FileNode(fn, type, false);
        node->setEnabled(false);
        nodes << node;

        auto parent = node->filePath().parentDir();
        auto folder = rootProjectNode()->recursiveFindOrCreateFolderNode(parent.toString(), projectDirectory());
        folder->addFileNodes({new FileNode(*node)});
    }

    // Update tree without full rescan run
    sort(nodes, Node::sortByPath);

    auto oldSize = m_allFiles.size();
    m_allFiles.append(nodes);
    std::inplace_merge(m_allFiles.begin(),
                       m_allFiles.begin() + oldSize,
                       m_allFiles.end(),
                       Node::sortByPath);

    QTC_ASSERT(isSorted(m_allFiles, Node::sortByPath), return false);

    askRunCMake();

    return true;
}

bool CMakeProject::eraseFiles(const QStringList &filePaths)
{
    QList<const FileNode *> removed;
    for (auto& filePath : filePaths) {
        auto fn = FileName::fromString(filePath);
        auto node = Compat::ProjectExplorer::recursiveFileNode(rootProjectNode(), fn, projectDirectory());
        if (!node)
            return false;
        auto folder = node->parentFolderNode();
        if (!folder)
            return false;

        // To update list
        removed << new FileNode(*node);

        folder->removeFileNodes({node});
    }

    // Update tree without full rescan run
    sort(removed, Node::sortByPath);

    // Inplace change m_allFiles in one pass

    auto it = std::lower_bound(m_allFiles.begin(), m_allFiles.end(), removed[0], Node::sortByPath);
    QTC_ASSERT(it != m_allFiles.end(), return false);

    int allIdx = static_cast<int>(std::distance(m_allFiles.begin(), it));
    int remIdx = 0;
    while (allIdx < m_allFiles.size() && remIdx < removed.size()) {
        auto &allNode = m_allFiles[allIdx];
        auto &removeNode = removed[remIdx];

        if (Node::sortByPath(allNode, removeNode)) {
            allIdx++;
        } else if (Node::sortByPath(removeNode, allNode)) {
            remIdx++;
        } else {
            delete allNode;
            m_allFiles.removeAt(allIdx);
            // We should not increment allIdx here
            remIdx++;
        }
    }

    QTC_ASSERT(isSorted(m_allFiles, Node::sortByPath), return false);

    askRunCMake();

    return true;
}

bool CMakeProject::renameFile(const QString &filePath, const QString &newFilePath)
{
    auto fn = FileName::fromString(filePath);
    auto newfn = FileName::fromString(newFilePath);
    auto node = Compat::ProjectExplorer::recursiveFileNode(rootProjectNode(), fn, projectDirectory());
    if (!node)
        return false;

    // Update tree without full rescan run
    {
        auto it = std::lower_bound(m_allFiles.begin(),
                                   m_allFiles.end(),
                                   node,
                                   Node::sortByPath);
        if (it == m_allFiles.end())
            return false;

        // Update data in scanned files and in the project tree
        delete *it;
        m_allFiles.removeAt(static_cast<int>(std::distance(m_allFiles.begin(), it)));

        // We add only one new file. Use std::lower_bound to insert item to right place
        auto toAdd = new FileNode(newfn, node->fileType(), false); // do not copy parent and other
        toAdd->setEnabled(false);

        it = std::lower_bound(m_allFiles.begin(),
                              m_allFiles.end(),
                              toAdd,
                              Node::sortByPath);

        m_allFiles.insert(it, toAdd);

        QTC_ASSERT(isSorted(m_allFiles, Node::sortByPath), return false);
    }

    auto folder = rootProjectNode()->recursiveFindOrCreateFolderNode(newfn.parentDir().toString(), projectDirectory());
    if (!folder)
        return false;

    if (folder != node->parentFolderNode()) {
        // Rename with moving
        folder->addFileNodes({ new FileNode(newfn, node->fileType(), false) });
        auto old = node->parentFolderNode();
        old->removeFileNodes({node});
    } else {
        node->setAbsoluteFilePathAndLine(newfn, -1);
    }

    askRunCMake();

    return true;
}

QList<CMakeBuildTarget> CMakeProject::buildTargets() const
{
    CMakeBuildConfiguration *bc = nullptr;
    if (activeTarget())
        bc = qobject_cast<CMakeBuildConfiguration *>(activeTarget()->activeBuildConfiguration());

    return bc ? bc->buildTargets() : QList<CMakeBuildTarget>();
}

QStringList CMakeProject::buildTargetTitles(bool runnable) const
{
    const QList<CMakeBuildTarget> targets
            = runnable ? filtered(buildTargets(),
                                  [](const CMakeBuildTarget &ct) {
                                      return !ct.executable.isEmpty() && ct.targetType == ExecutableType;
                                  })
                       : buildTargets();
    return transform(targets, [](const CMakeBuildTarget &ct) { return ct.title; });
}

bool CMakeProject::hasBuildTarget(const QString &title) const
{
    return anyOf(buildTargets(), [title](const CMakeBuildTarget &ct) { return ct.title == title; });
}

QString CMakeProject::displayName() const
{
    return rootProjectNode()->displayName();
}

QStringList CMakeProject::files(FilesMode fileMode) const
{
    // Copy and filter in one pass. Useful for big trees
    QStringList result;
    auto valid = [fileMode](const FileNode *fn) {
        // Skip unknown file types from caching
        if (fn->fileType() == FileType::Unknown)
            return false;
        const bool isGenerated = fn->isGenerated();
        switch (fileMode)
        {
        case Project::SourceFiles:
            return !isGenerated;
        case Project::GeneratedFiles:
            return isGenerated;
        case Project::AllFiles:
        default:
            return true;
        }
    };

    for (auto const& node : rootProjectNode()->recursiveFileNodes()) {
        if (valid(node))
            result.push_back(node->filePath().toString());
    }

    return result;
}

Project::RestoreResult CMakeProject::fromMap(const QVariantMap &map, QString *errorMessage)
{
    RestoreResult result = Project::fromMap(map, errorMessage);
    if (result != RestoreResult::Ok)
        return result;
    return RestoreResult::Ok;
}

bool CMakeProject::setupTarget(Target *t)
{
    t->updateDefaultBuildConfigurations();
    if (t->buildConfigurations().isEmpty())
        return false;
    t->updateDefaultDeployConfigurations();

    return true;
}

void CMakeProject::scanProjectTree()
{
    if (!m_treeScanner.isFinished())
        return;
    m_treeScanner.asyncScanForFiles(projectDirectory());
    Core::ProgressManager::addTask(m_treeScanner.future(),
                                   tr("Scan \"%1\" project tree").arg(displayName()),
                                   "CMake.Scan.Tree");
}

void CMakeProject::handleActiveTargetChanged()
{
    if (m_connectedTarget) {
        disconnect(m_connectedTarget, &Target::activeBuildConfigurationChanged,
                   this, &CMakeProject::handleActiveBuildConfigurationChanged);
        disconnect(m_connectedTarget, &Target::kitChanged,
                   this, &CMakeProject::handleActiveBuildConfigurationChanged);
    }

    m_connectedTarget = activeTarget();

    if (m_connectedTarget) {
        connect(m_connectedTarget, &Target::activeBuildConfigurationChanged,
                this, &CMakeProject::handleActiveBuildConfigurationChanged);
        connect(m_connectedTarget, &Target::kitChanged,
                this, &CMakeProject::handleActiveBuildConfigurationChanged);
    }

    handleActiveBuildConfigurationChanged();
}

void CMakeProject::handleActiveBuildConfigurationChanged()
{
    if (!activeTarget() || !activeTarget()->activeBuildConfiguration())
        return;
    auto activeBc = qobject_cast<CMakeBuildConfiguration *>(activeTarget()->activeBuildConfiguration());

    foreach (Target *t, targets()) {
        foreach (BuildConfiguration *bc, t->buildConfigurations()) {
            auto i = qobject_cast<CMakeBuildConfiguration *>(bc);
            QTC_ASSERT(i, continue);
            if (i == activeBc)
                i->maybeForceReparse();
            else
                i->resetData();
        }
    }
}

void CMakeProject::handleParsingStarted()
{
    if (activeTarget() && activeTarget()->activeBuildConfiguration() == sender())
        emit parsingStarted();
}

void CMakeProject::handleTreeScanningFinished()
{
    qDeleteAll(m_allFiles);
    m_allFiles = Utils::transform(m_treeScanner.release(), [](FileNode *fn) -> const FileNode* {
        fn->setEnabled(false);
        return fn;
    });

    auto t = activeTarget();
    if (!t)
        return;

    auto bc = qobject_cast<CMakeBuildConfiguration*>(t->activeBuildConfiguration());
    if (!bc)
        return;

    updateProjectData(bc);
}

CMakeBuildTarget CMakeProject::buildTargetForTitle(const QString &title)
{
    foreach (const CMakeBuildTarget &ct, buildTargets())
        if (ct.title == title)
            return ct;
    return CMakeBuildTarget();
}

QStringList CMakeProject::filesGeneratedFrom(const QString &sourceFile) const
{
    if (!activeTarget())
        return QStringList();
    QFileInfo fi(sourceFile);
    FileName project = projectDirectory();
    FileName baseDirectory = FileName::fromString(fi.absolutePath());

    while (baseDirectory.isChildOf(project)) {
        FileName cmakeListsTxt = baseDirectory;
        cmakeListsTxt.appendPath("CMakeLists.txt");
        if (cmakeListsTxt.exists())
            break;
        QDir dir(baseDirectory.toString());
        dir.cdUp();
        baseDirectory = FileName::fromString(dir.absolutePath());
    }

    QDir srcDirRoot = QDir(project.toString());
    QString relativePath = srcDirRoot.relativeFilePath(baseDirectory.toString());
    QDir buildDir = QDir(activeTarget()->activeBuildConfiguration()->buildDirectory().toString());
    QString generatedFilePath = buildDir.absoluteFilePath(relativePath);

    if (fi.suffix() == "ui") {
        generatedFilePath += "/ui_";
        generatedFilePath += fi.completeBaseName();
        generatedFilePath += ".h";
        return QStringList(QDir::cleanPath(generatedFilePath));
    } else if (fi.suffix() == "scxml") {
        generatedFilePath += "/";
        generatedFilePath += QDir::cleanPath(fi.completeBaseName());
        return QStringList({ generatedFilePath + ".h",
                             generatedFilePath + ".cpp" });
    } else {
        // TODO: Other types will be added when adapters for their compilers become available.
        return QStringList();
    }
}

void CMakeProject::updateTargetRunConfigurations(Target *t)
{
    // *Update* existing runconfigurations (no need to update new ones!):
    QHash<QString, const CMakeBuildTarget *> buildTargetHash;
    const QList<CMakeBuildTarget> buildTargetList = buildTargets();
    foreach (const CMakeBuildTarget &bt, buildTargetList) {
        if (bt.targetType != ExecutableType || bt.executable.isEmpty())
            continue;

        buildTargetHash.insert(bt.title, &bt);
    }

    foreach (RunConfiguration *rc, t->runConfigurations()) {
        auto cmakeRc = qobject_cast<CMakeRunConfiguration *>(rc);
        if (!cmakeRc)
            continue;

        auto btIt = buildTargetHash.constFind(cmakeRc->title());
        cmakeRc->setEnabled(btIt != buildTargetHash.constEnd());
        if (btIt != buildTargetHash.constEnd()) {
            cmakeRc->setExecutable(btIt.value()->executable.toString());
            cmakeRc->setBaseWorkingDirectory(btIt.value()->workingDirectory);
        }
    }

    // create new and remove obsolete RCs using the factories
    t->updateDefaultRunConfigurations();
}

void CMakeProject::updateApplicationAndDeploymentTargets()
{
    Target *t = activeTarget();
    if (!t)
        return;

    QFile deploymentFile;
    QTextStream deploymentStream;
    QString deploymentPrefix;

    QDir sourceDir(t->project()->projectDirectory().toString());
    QDir buildDir(t->activeBuildConfiguration()->buildDirectory().toString());

    deploymentFile.setFileName(sourceDir.filePath("QtCreatorDeployment.txt"));
    // If we don't have a global QtCreatorDeployment.txt check for one created by the active build configuration
    if (!deploymentFile.exists())
        deploymentFile.setFileName(buildDir.filePath("QtCreatorDeployment.txt"));
    if (deploymentFile.open(QFile::ReadOnly | QFile::Text)) {
        deploymentStream.setDevice(&deploymentFile);
        deploymentPrefix = deploymentStream.readLine();
        if (!deploymentPrefix.endsWith('/'))
            deploymentPrefix.append('/');
    }

    BuildTargetInfoList appTargetList;
    DeploymentData deploymentData;

    foreach (const CMakeBuildTarget &ct, buildTargets()) {
        if (ct.targetType == UtilityType)
            continue;

        if (ct.targetType == ExecutableType || ct.targetType == DynamicLibraryType)
            deploymentData.addFile(ct.executable.toString(), deploymentPrefix + buildDir.relativeFilePath(ct.executable.toFileInfo().dir().path()), DeployableFile::TypeExecutable);
        if (ct.targetType == ExecutableType) {
            // TODO: Put a path to corresponding .cbp file into projectFilePath?
            appTargetList.list << BuildTargetInfo(ct.title, ct.executable, ct.executable);
        }
    }

    QString absoluteSourcePath = sourceDir.absolutePath();
    if (!absoluteSourcePath.endsWith('/'))
        absoluteSourcePath.append('/');
    if (deploymentStream.device()) {
        while (!deploymentStream.atEnd()) {
            QString line = deploymentStream.readLine();
            if (!line.contains(':'))
                continue;
            QStringList file = line.split(':');
            deploymentData.addFile(absoluteSourcePath + file.at(0), deploymentPrefix + file.at(1));
        }
    }

    t->setApplicationTargets(appTargetList);
    t->setDeploymentData(deploymentData);
}

void CMakeProject::createGeneratedCodeModelSupport()
{
    qDeleteAll(m_extraCompilers);
    m_extraCompilers.clear();
    QList<ExtraCompilerFactory *> factories =
            ExtraCompilerFactory::extraCompilerFactories();

    // Find all files generated by any of the extra compilers, in a rather crude way.
    foreach (const QString &file, files(SourceFiles)) {
        foreach (ExtraCompilerFactory *factory, factories) {
            if (file.endsWith('.' + factory->sourceTag())) {
                QStringList generated = filesGeneratedFrom(file);
                if (!generated.isEmpty()) {
                    const FileNameList fileNames = transform(generated,
                                                             [](const QString &s) {
                        return FileName::fromString(s);
                    });
                    m_extraCompilers.append(factory->create(this, FileName::fromString(file),
                                                            fileNames));
                }
            }
        }
    }

    CppTools::GeneratedCodeModelSupport::update(m_extraCompilers);
}

void CMakeBuildTarget::clear()
{
    executable.clear();
    makeCommand.clear();
    workingDirectory.clear();
    sourceDirectory.clear();
    title.clear();
    targetType = UtilityType;
    includeFiles.clear();
    compilerOptions.clear();
    defines.clear();
    files.clear();
}

} // namespace CMakeProjectManager

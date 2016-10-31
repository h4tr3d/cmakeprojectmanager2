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

#include <coreplugin/progressmanager/progressmanager.h>
#include <cpptools/cppmodelmanager.h>
#include <cpptools/generatedcodemodelsupport.h>
#include <cpptools/projectinfo.h>
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

#include <chrono>

using namespace ProjectExplorer;
using namespace Utils;

namespace CMakeProjectManager {

const int MIN_TIME_BETWEEN_TREE_SCANS = 4500;

using namespace Internal;

// QtCreator CMake Generator wishlist:
// Which make targets we need to build to get all executables
// What is the actual compiler executable
// DEFINES

/*!
  \class CMakeProject
*/
CMakeProject::CMakeProject(CMakeManager *manager, const FileName &fileName)
    : m_treeBuilder(new TreeBuilder(this))
{
    setId(CMakeProjectManager::Constants::CMAKEPROJECT_ID);
    setProjectManager(manager);
    setDocument(new TextEditor::TextDocument);
    document()->setFilePath(fileName);

    setRootProjectNode(new CMakeProjectNode(this, FileName::fromString(fileName.toFileInfo().absolutePath())));
    setProjectContext(Core::Context(CMakeProjectManager::Constants::PROJECTCONTEXT));
    setProjectLanguages(Core::Context(ProjectExplorer::Constants::LANG_CXX));

    rootProjectNode()->setDisplayName(fileName.parentDir().fileName());

    connect(this, &CMakeProject::activeTargetChanged, this, &CMakeProject::handleActiveTargetChanged);
    connect(m_treeBuilder.get(), &TreeBuilder::scanningFinished, this, &CMakeProject::handleScanningFinished);
    connect(&m_treeWatcher, &QFileSystemWatcher::directoryChanged, this, &CMakeProject::handleDirectoryChange);
    connect(&m_treeScanTimer, &QTimer::timeout, this, &CMakeProject::scanProjectTree);

    m_treeScanTimer.setSingleShot(true);

    scanProjectTree();
}

CMakeProject::~CMakeProject()
{
    setRootProjectNode(nullptr);
    m_codeModelFuture.cancel();
    qDeleteAll(m_extraCompilers);
    qDeleteAll(m_files);
}

void CMakeProject::updateProjectData()
{
    auto cmakeBc = qobject_cast<CMakeBuildConfiguration *>(sender());
    auto treeBuilder = qobject_cast<TreeBuilder *>(sender());

    Target *t = activeTarget();
    if (!t)
        return;

    if (cmakeBc) {
        if (t->activeBuildConfiguration() != cmakeBc)
            return;
    } else if (treeBuilder) {
        cmakeBc = qobject_cast<CMakeBuildConfiguration*>(t->activeBuildConfiguration());
    } else {
        return;
    }

    QTC_ASSERT(cmakeBc, return);

    if (m_treeBuilder->isScanning() || cmakeBc->isParsing())
        return;

    Kit *k = t->kit();

    cmakeBc->generateProjectTree(static_cast<CMakeProjectNode *>(rootProjectNode()),
                                 TreeBuilder::fileNodes(m_treeFiles));

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

    const QSet<Core::Id> languages = cmakeBc->updateCodeModel(ppBuilder);
    for (const auto &lid : languages)
        setProjectLanguage(lid, true);

    m_codeModelFuture.cancel();
    pinfo.finish();
    m_codeModelFuture = modelmanager->updateProjectInfo(pinfo);

    updateQmlJSCodeModel();

    emit displayNameChanged();
    emit fileListChanged();

    emit cmakeBc->emitBuildTypeChanged();
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

QList<CMakeBuildTarget> CMakeProject::buildTargets() const
{
    CMakeBuildConfiguration *bc = nullptr;
    if (activeTarget())
        bc = qobject_cast<CMakeBuildConfiguration *>(activeTarget()->activeBuildConfiguration());

    return bc ? bc->buildTargets() : QList<CMakeBuildTarget>();
}

void CMakeProject::handleScanningFinished()
{
    m_cacheKey.clear();
    m_cachedItems.clear();

    m_lastTreeScan.start();

    m_treePaths = m_treeBuilder->paths();
    for (auto &path : m_treePaths)
        m_treeWatcher.addPath(path.toString());
    m_treeWatcher.addPath(projectDirectory().toString());

    m_treeFiles = m_treeBuilder->files();
    m_treeBuilder->clear();

    updateProjectData();
}

void CMakeProject::handleDirectoryChange(QString path)
{
    // QFileSystemWatcher so ugly to use: it can't be configure to handle only file/dir Add, Remove,
    // Rename, it alway handle FileChanged too. But we must not rescan tree on every file save.
    // Possible replaces:
    //   * https://bitbucket.org/SpartanJ/efsw/overview
    //   * http://emcrisostomo.github.io/fswatch/
    // Currently use hack:
    //  1. Request Tree Scanner nodes for changed path
    //  2. Request current directory nodes
    //  3. And compare it
    // If nodes list differ it means, that file added, removed of renemed, but not changed.
    // In future it can be optimized: replace full rescan with partial one.

    //qDebug() << "Changes [0]:" << path;

    auto cachedItems = directoryEntries(FileName::fromString(path));
    //qDebug() << "Scanner dir entries:" << cachedItems;

    auto dir = QDir(path);
    if (dir.exists()) {
        auto currentList = dir.entryList(QDir::Files |
                                         QDir::Dirs |
                                         QDir::NoDotAndDotDot |
                                         QDir::NoSymLinks);
        QSet<FileName> currentItems = transform(currentList.toSet(), [](const QString& fn) {
            return FileName::fromString(fn);
        });

        //qDebug() << "Current dir entries:" << currentItems;

        auto removed = cachedItems - currentItems;
        auto added = currentItems - cachedItems;

        if (!removed.empty() || !added.empty()) {
            //qDebug() << "Chanes [1]:" << path;
            scheduleScanProjectTree();
        }
    } else {
        // Directory removed, rescan tree
        scheduleScanProjectTree();
    }
}

FileNameList CMakeProject::directoryList(const FileNameList &paths) const
{
    QList<FileName> dirs;
    for (const auto& path : paths) {
        auto fi = path.toFileInfo();
        auto insertPath = path;

        if (!fi.isDir()) {
            insertPath = FileName::fromString(fi.absolutePath());
        }

        // Omit dups
        if (dirs.count()) {
            // Path objects already sorted
            if (dirs.last() != insertPath)
                dirs.append(insertPath);
        } else {
            dirs.append(insertPath);
        }
    }

    Q_ASSERT(isSorted(dirs, [](const FileName& lhs, const FileName& rhs) {
        return lhs < rhs;
    }));

    return dirs;
}

QSet<FileName> CMakeProject::directoryEntries(const FileName &directory) const
{
    // Speed up quick requiest
    if (m_cacheKey.count() || directory == m_cacheKey)
        return m_cachedItems;

    QSet<Utils::FileName> result;
    for (const auto &list : {m_treeFiles, m_treePaths}) {
        for (const auto& fn : list) {
            if (fn.toString().startsWith(directory.toString())) {
                auto from = directory.toString().length();
                from++; // Skip '/'

                if (from >= fn.toString().length())
                    continue;

                auto index = fn.toString().indexOf('/', from);
                int count = -1;
                if (index != -1) {
                    count = index - from;
                }

                result.insert(Utils::FileName::fromString(fn.toString().mid(from, count)));
            }
        }
    }

    // Cache last requiest
    m_cacheKey = directory;
    m_cachedItems = result;

    return result;
}

void CMakeProject::addFilesCommon(const QStringList &filePaths)
{
    auto paths = transform(filePaths, [](const QString &fn) {
        return FileName::fromString(fn);
    });

    sort(paths);
    auto oldSize = m_treeFiles.size();
    m_treeFiles.append(paths);
    std::inplace_merge(m_treeFiles.begin(),
                       m_treeFiles.begin() + oldSize,
                       m_treeFiles.end());

    auto dirs = directoryList(paths);

    // Append dirs to watcher, ignores if already present
    m_treeWatcher.addPaths(transform(dirs, [](const FileName &path) {
        return path.toString();
    }));

    oldSize = m_treePaths.size();
    std::inplace_merge(m_treePaths.begin(),
                       m_treePaths.begin() + oldSize,
                       m_treePaths.end());
    m_treePaths = filteredUnique(m_treePaths);
}

void CMakeProject::eraseFilesCommon(const QStringList &filePaths)
{
    auto paths = transform(filePaths, [](const QString &fn) {
        return FileName::fromString(fn);
    });

    sort(paths);

    FileNameList filtered;
    filtered.reserve(m_treeFiles.size());
    std::set_difference(m_treeFiles.begin(),
                        m_treeFiles.end(),
                        paths.begin(),
                        paths.end(),
                        std::back_inserter(filtered));

    m_treeFiles = filtered;

    // Process paths
    auto dirs = directoryList(paths);

    FileNameList directoryFiltered;
    directoryFiltered.reserve(m_treePaths.size());
    std::set_difference(m_treePaths.begin(),
                        m_treePaths.end(),
                        dirs.begin(),
                        dirs.end(),
                        std::back_inserter(directoryFiltered));

    m_treePaths = directoryFiltered;
}

void CMakeProject::renameFileCommon(const QString &filePath, const QString &newFilePath)
{
#define STUPID_RENAME
#ifdef STUPID_RENAME
    eraseFilesCommon(QStringList(filePath));
    addFilesCommon(QStringList(newFilePath));
#else
    // TODO
#endif
}

void CMakeProject::scheduleScanProjectTree()
{
    auto elapsedTime = m_lastTreeScan.elapsed();
    if (elapsedTime < MIN_TIME_BETWEEN_TREE_SCANS) {
        if (!m_treeScanTimer.isActive()) {
            m_treeScanTimer.setInterval(MIN_TIME_BETWEEN_TREE_SCANS - elapsedTime);
            m_treeScanTimer.start();
        }
    } else {
        scanProjectTree();
    }
}

void CMakeProject::scanProjectTree()
{
    if (m_treeBuilder->isScanning())
        return;
    m_treeBuilder->startScanning(projectDirectory());
    Core::ProgressManager::addTask(m_treeBuilder->future(),
                                   tr("Scanning tree \"%1\"").arg(displayName()),
                                   "CMake.Scanning");
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
    auto tm = std::chrono::system_clock::now();
    if (!m_files.empty()) {
        if (m_generatedFilesCache.empty() && m_sourceFilesCache.empty()) {
            for (auto &node : m_files) {
                if (node->fileType() == UnknownFileType)
                    continue;

                const bool isGenerated = node->isGenerated();
                if (isGenerated)
                    m_generatedFilesCache.push_back(node->filePath().toString());
                else
                    m_sourceFilesCache.push_back(node->filePath().toString());
            }
        }
    }

    auto delta = std::chrono::system_clock::now() - tm;
    //qDebug() << "files():" << std::chrono::duration_cast<std::chrono::milliseconds>(delta).count();

    switch (fileMode) {
        case Project::GeneratedFiles:
            return m_generatedFilesCache;
        case Project::SourceFiles:
            return m_sourceFilesCache;
        case Project::AllFiles:
            return m_sourceFilesCache + m_generatedFilesCache;
    }
    return QStringList();
}

const QList<FileNode *> &CMakeProject::files() const
{
    return m_files;
}

void CMakeProject::setFiles(QList<FileNode *> &&nodes)
{
    qDeleteAll(m_files);
    m_files = std::move(nodes);
    m_generatedFilesCache.clear();
    m_sourceFilesCache.clear();
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

bool CMakeProject::addFiles(const QStringList &filePaths)
{
    addFilesCommon(filePaths);
    // If globbing is used, watched does not know about new files, so force rebuilding
    runCMake();
    return true;
}

bool CMakeProject::eraseFiles(const QStringList &filePaths)
{
    eraseFilesCommon(filePaths);
    // FIXME force only when really needed
    runCMake();
    return true;
}

bool CMakeProject::renameFile(const QString &filePath, const QString &newFilePath)
{
    renameFileCommon(filePath, newFilePath);
    // FIXME force only when really needed
    runCMake();
    return true;
}

} // namespace CMakeProjectManager

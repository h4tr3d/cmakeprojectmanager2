/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company.  For licensing terms and
** conditions see http://www.qt.io/terms-conditions.  For further information
** use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file.  Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, The Qt Company gives you certain additional
** rights.  These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include "cmakeproject.h"

#include "cmakebuildconfiguration.h"
#include "cmakeprojectconstants.h"
#include "cmakeprojectnodes.h"
#include "cmakerunconfiguration.h"
#include "makestep.h"
#include "cmakeopenprojectwizard.h"
#include "generatorinfo.h"
#include "cmakecbpparser.h"
#include "cmakefile.h"
#include "cmakeprojectmanager.h"
#include "cmaketoolmanager.h"
#include "cmakekitinformation.h"

#include "cmakelistsparser.h"

#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/headerpath.h>
#include <projectexplorer/buildsteplist.h>
#include <projectexplorer/buildmanager.h>
#include <projectexplorer/buildtargetinfo.h>
#include <projectexplorer/kitinformation.h>
#include <projectexplorer/kitmanager.h>
#include <projectexplorer/toolchain.h>

#include <projectexplorer/deployconfiguration.h>
#include <projectexplorer/deploymentdata.h>
#include <qtsupport/customexecutablerunconfiguration.h>
#include <qtsupport/baseqtversion.h>
#include <qtsupport/qtkitinformation.h>
#include <qtsupport/uicodemodelsupport.h>
#include <cpptools/cppmodelmanager.h>
#include <extensionsystem/pluginmanager.h>
#include <utils/algorithm.h>
#include <utils/qtcassert.h>
#include <utils/stringutils.h>
#include <utils/hostosinfo.h>
#include <coreplugin/icore.h>
#include <coreplugin/infobar.h>
#include <coreplugin/editormanager/editormanager.h>

#include <utils/mimetypes/mimedatabase.h>

#include <QDebug>
#include <QDir>
#include <QFileSystemWatcher>

using namespace CMakeProjectManager;
using namespace CMakeProjectManager::Internal;
using namespace ProjectExplorer;
using namespace Utils;

// QtCreator CMake Generator wishlist:
// Which make targets we need to build to get all executables
// What is the make we need to call
// What is the actual compiler executable
// DEFINES

// Open Questions
// Who sets up the environment for cl.exe ? INCLUDEPATH and so on

// TODO This code taken from projectnodes.cpp and it marked as HACK. Wait for more clean solution.
static ProjectExplorer::FileType getFileType(const QString &file)
{
    using namespace ProjectExplorer;

    Utils::MimeDatabase mdb;
    const Utils::MimeType mt = mdb.mimeTypeForFile(file);
    if (!mt.isValid())
        return UnknownFileType;

    const QString typeName = mt.name();
    if (typeName == QLatin1String(ProjectExplorer::Constants::CPP_SOURCE_MIMETYPE)
        || typeName == QLatin1String(ProjectExplorer::Constants::C_SOURCE_MIMETYPE))
        return SourceType;
    if (typeName == QLatin1String(ProjectExplorer::Constants::CPP_HEADER_MIMETYPE)
        || typeName == QLatin1String(ProjectExplorer::Constants::C_HEADER_MIMETYPE))
        return HeaderType;
    if (typeName == QLatin1String(ProjectExplorer::Constants::RESOURCE_MIMETYPE))
        return ResourceType;
    if (typeName == QLatin1String(ProjectExplorer::Constants::FORM_MIMETYPE))
        return FormType;
    if (typeName == QLatin1String(ProjectExplorer::Constants::QML_MIMETYPE))
        return QMLType;
    return UnknownFileType;
}

// Make file node by file name
static ProjectExplorer::FileNode* fileToFileNode(const Utils::FileName &fileName)
{
    // TODO
    ProjectExplorer::FileNode *node = 0;
    bool generated = false;
    QString onlyFileName = fileName.fileName();
    if (   (onlyFileName.startsWith(QLatin1String("moc_")) && onlyFileName.endsWith(QLatin1String(".cxx")))
           || (onlyFileName.startsWith(QLatin1String("ui_")) && onlyFileName.endsWith(QLatin1String(".h")))
           || (onlyFileName.startsWith(QLatin1String("qrc_")) && onlyFileName.endsWith(QLatin1String(".cxx"))))
        generated = true;

    if (fileName.endsWith(QLatin1String("CMakeLists.txt")))
        node = new ProjectExplorer::FileNode(fileName, ProjectExplorer::ProjectFileType, false);
    else {
        ProjectExplorer::FileType fileType = getFileType(fileName.fileName());
        node = new ProjectExplorer::FileNode(fileName, fileType, generated);
    }

    return node;
}


/*!
  \class CMakeProject
*/
CMakeProject::CMakeProject(CMakeManager *manager, const FileName &fileName)
    : m_manager(manager),
      m_activeTarget(0),
      m_fileName(fileName),
      m_cbpUpdateProcess(0),
      m_watcher(new QFileSystemWatcher(this))
{
    setId(Constants::CMAKEPROJECT_ID);
    setProjectContext(Core::Context(CMakeProjectManager::Constants::PROJECTCONTEXT));
    setProjectLanguages(Core::Context(ProjectExplorer::Constants::LANG_CXX));

    m_projectName = fileName.parentDir().fileName();

    m_file = new CMakeFile(this, fileName);
    m_rootNode = new CMakeProjectNode(this, m_fileName);

    connect(this, SIGNAL(buildTargetsChanged()),
            this, SLOT(updateRunConfigurations()));

    connect(m_watcher, SIGNAL(fileChanged(QString)), this, SLOT(fileChanged(QString)));
}

CMakeProject::~CMakeProject()
{
    m_codeModelFuture.cancel();
    delete m_rootNode;
}

void CMakeProject::fileChanged(const QString &fileName)
{
    Q_UNUSED(fileName)

    parseCMakeLists();
}

void CMakeProject::changeActiveBuildConfiguration(ProjectExplorer::BuildConfiguration *bc)
{
    if (!bc)
        return;

    CMakeBuildConfiguration *cmakebc = static_cast<CMakeBuildConfiguration *>(bc);

    // Pop up a dialog asking the user to rerun cmake
    QString cbpFile = CMakeManager::findCbpFile(QDir(bc->buildDirectory().toString()));
    QFileInfo cbpFileFi(cbpFile);
    CMakeOpenProjectWizard::Mode mode = CMakeOpenProjectWizard::Nothing;
    if (!cbpFileFi.exists()) {
        mode = CMakeOpenProjectWizard::NeedToCreate;
    } else {
        foreach (const FileName &file, m_watchedFiles) {
            if (file.toFileInfo().lastModified() > cbpFileFi.lastModified()) {
                mode = CMakeOpenProjectWizard::NeedToUpdate;
                break;
            }
        }
    }

    if (mode != CMakeOpenProjectWizard::Nothing) {
        CMakeBuildInfo info(cmakebc);
        CMakeOpenProjectWizard copw(Core::ICore::mainWindow(), m_manager, mode, &info);
        if (copw.exec() == QDialog::Accepted) {
            cmakebc->setCMakeParams(copw.arguments());
            cmakebc->setUseNinja(copw.useNinja()); // NeedToCreate can change the Ninja setting
            cmakebc->setCMakeParamsExt(copw.cmakeParamsExt());
        }
    }

    // reparse
    parseCMakeLists();
}

void CMakeProject::activeTargetWasChanged(Target *target)
{
    if (m_activeTarget) {
        disconnect(m_activeTarget, SIGNAL(activeBuildConfigurationChanged(ProjectExplorer::BuildConfiguration*)),
                   this, SLOT(changeActiveBuildConfiguration(ProjectExplorer::BuildConfiguration*)));
    }

    m_activeTarget = target;

    if (!m_activeTarget)
        return;

    connect(m_activeTarget, SIGNAL(activeBuildConfigurationChanged(ProjectExplorer::BuildConfiguration*)),
            this, SLOT(changeActiveBuildConfiguration(ProjectExplorer::BuildConfiguration*)));

    changeActiveBuildConfiguration(m_activeTarget->activeBuildConfiguration());
}

void CMakeProject::changeBuildDirectory(CMakeBuildConfiguration *bc, const QString &newBuildDirectory)
{
    bc->setBuildDirectory(FileName::fromString(newBuildDirectory));
    parseCMakeLists();
}

QStringList CMakeProject::getCXXFlagsFor(const CMakeBuildTarget &buildTarget)
{
    QString makeCommand = QDir::fromNativeSeparators(buildTarget.makeCommand);
    int startIndex = makeCommand.indexOf(QLatin1Char('\"'));
    int endIndex = makeCommand.indexOf(QLatin1Char('\"'), startIndex + 1);
    if (startIndex != -1 && endIndex != -1) {
        startIndex += 1;
        QString makefile = makeCommand.mid(startIndex, endIndex - startIndex);
        int slashIndex = makefile.lastIndexOf(QLatin1Char('/'));
        makefile.truncate(slashIndex);
        makefile.append(QLatin1String("/CMakeFiles/") + buildTarget.title + QLatin1String(".dir/flags.make"));
        QFile file(makefile);
        if (file.exists()) {
            file.open(QIODevice::ReadOnly | QIODevice::Text);
            QTextStream stream(&file);
            while (!stream.atEnd()) {
                QString line = stream.readLine().trimmed();
                if (line.startsWith(QLatin1String("CXX_FLAGS ="))) {
                    // Skip past =
                    return line.mid(11).trimmed().split(QLatin1Char(' '), QString::SkipEmptyParts);
                }
            }
        }
    }

    // Attempt to find build.ninja file and obtain FLAGS (CXX_FLAGS) from there if no suitable flags.make were
    // found
    // Get "all" target's working directory
    if (!buildTargets().empty()) {
        QString buildNinjaFile = QDir::fromNativeSeparators(buildTargets().at(0).workingDirectory);
        buildNinjaFile += QLatin1String("/build.ninja");
        QFile buildNinja(buildNinjaFile);
        if (buildNinja.exists()) {
            buildNinja.open(QIODevice::ReadOnly | QIODevice::Text);
            QTextStream stream(&buildNinja);
            bool targetFound = false;
            bool cxxFound = false;
            QString targetSearchPattern = QString::fromLatin1("target %1").arg(buildTarget.title);

            while (!stream.atEnd()) {
                // 1. Look for a block that refers to the current target
                // 2. Look for a build rule which invokes CXX_COMPILER
                // 3. Return the FLAGS definition
                QString line = stream.readLine().trimmed();
                if (line.startsWith(QLatin1String("#"))) {
                    if (!line.startsWith(QLatin1String("# Object build statements for"))) continue;
                    targetFound = line.endsWith(targetSearchPattern);
                } else if (targetFound && line.startsWith(QLatin1String("build"))) {
                    cxxFound = line.indexOf(QLatin1String("CXX_COMPILER")) != -1;
                } else if (cxxFound && line.startsWith(QLatin1String("FLAGS ="))) {
                    // Skip past =
                    return line.mid(7).trimmed().split(QLatin1Char(' '), QString::SkipEmptyParts);
                }
            }
        }
    }
    return QStringList();
}

bool CMakeProject::parseCMakeLists()
{
    if (!activeTarget() ||
        !activeTarget()->activeBuildConfiguration()) {
        return false;
    }

    foreach (Core::IDocument *document, Core::DocumentModel::openedDocuments())
        if (isProjectFile(document->filePath()))
            document->infoBar()->removeInfo("CMakeEditor.RunCMake");

    CMakeListsParser parser;

    if (!parser.parseProject(projectDirectory().toString())) {
        qDebug() << parser.errorString();
        emit buildTargetsChanged();
        return false;
    }

    Kit *k = activeTarget()->kit();

    // setFolderName
    m_rootNode->setDisplayName(parser.projectName());

    foreach (const QString &file, m_watcher->files())
        m_watcher->removePath(file);

    m_projectName = parser.projectName();
    m_rootNode->setDisplayName(parser.projectName());

    QList<ProjectExplorer::FileNode *> fileList = parser.projectFiles();
    // HTRD: this files must be used to build source tree
    QList<ProjectExplorer::FileNode *> treeFileList;

    // HTRD: Take file list from file system instead cbp project file
    const QDir        dir(projectDirectory().toString());
    QStringList       sources, paths;
    getFileList(dir, projectDirectory().toString(), /*suffixes,*/ &sources, &paths);
    foreach (const QString &source, sources) {
        QFileInfo                  fileInfo(source);
        QString                    fileName = fileInfo.fileName();
        ProjectExplorer::FileNode *node     = fileToFileNode(FileName::fromString(source));

        if (fileName.endsWith(QLatin1String("CMakeLists.txt"))) {
            // [[ Nikita ]] -- new parser discards this line???
            //projectFiles.insert(FileName::fromString(source));
        } else {
            treeFileList.append(node);
        }
    }

    // HTRD: remove duplicates, if needed
    /*foreach (ProjectExplorer::FileNode *node, cbpparser.cmakeFileList()) {
            projectFiles.insert(node->path());
            if (!Utils::contains(treeFileList, [&node](const ProjectExplorer::FileNode *n) {
                    return n->path() == node->path();
                 })) {
                treeFileList.append(node);
            }
        }*/


    m_files.clear();
    foreach (ProjectExplorer::FileNode *fn, fileList)
    {
        m_files.append(fn->path().toString());
        delete fn;
    }
    m_files.sort();
    m_files.removeDuplicates();
    m_watchedFiles = m_files.toSet();

    buildTree(m_rootNode, treeFileList);

    m_buildTargets.clear();
    QStringList targets = parser.targets();
    QStringList::ConstIterator it, end = targets.cend();
    for (it = targets.cbegin(); it != end; ++it) {
        CMakeBuildTarget t;
        t.executable = *it;
        t.library = false;
        m_buildTargets.append(t);
    }

    updateApplicationAndDeploymentTargets();

    createUiCodeModelSupport();

    ToolChain *tc = ProjectExplorer::ToolChainKitInformation::toolChain(k);
    if (!tc) {
        emit buildTargetsChanged();
        emit fileListChanged();
        return true;
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

    foreach (const CMakeBuildTarget &cbt, m_buildTargets) {
        // This explicitly adds -I. to the include paths
        QStringList includePaths = cbt.includeFiles;
        includePaths += projectDirectory().toString();
        includePaths += parser.includeDirectories();
        // [[ Nikita ]] -- begin, to RESOLVE
        //QtSupport::BaseQtVersion *qtVer = QtSupport::QtKitInformation::qtVersion(activeTarget()->kit());
        //part->includePaths += qtVer->sourcePath().toString() + QLatin1String("/include");
        //part->projectDefines += parser.defines();
        // [[ Nikita ]] -- end
        ppBuilder.setIncludePaths(includePaths);
        ppBuilder.setCFlags(getCXXFlagsFor(cbt));
        ppBuilder.setCxxFlags(getCXXFlagsFor(cbt));
        ppBuilder.setDefines(cbt.defines);
        ppBuilder.setDisplayName(cbt.title);
        const QList<Core::Id> languages = ppBuilder.createProjectPartsForFiles(cbt.files);
        foreach (Core::Id language, languages)
            setProjectLanguage(language, true);
    }
    m_codeModelFuture.cancel();
    pinfo.finish();
    m_codeModelFuture = modelmanager->updateProjectInfo(pinfo);

    emit displayNameChanged();
    emit buildTargetsChanged();
    emit fileListChanged();

    emit activeBC->emitBuildTypeChanged();

    return true;
}

bool CMakeProject::isProjectFile(const FileName &fileName)
{
    return m_watchedFiles.contains(fileName);
}

QList<CMakeBuildTarget> CMakeProject::buildTargets() const
{
    return m_buildTargets;
}

QStringList CMakeProject::buildTargetTitles(bool runnable) const
{
    QStringList results;
    foreach (const CMakeBuildTarget &ct, m_buildTargets) {
        if (runnable && (ct.executable.isEmpty() || ct.targetType != ExecutableType))
            continue;
        results << ct.title;
    }
    return results;
}

bool CMakeProject::hasBuildTarget(const QString &title) const
{
    foreach (const CMakeBuildTarget &ct, m_buildTargets) {
        if (ct.title == title)
            return true;
    }
    return false;
}

void CMakeProject::gatherFileNodes(ProjectExplorer::FolderNode *parent, QList<ProjectExplorer::FileNode *> &list)
{
    foreach (ProjectExplorer::FolderNode *folder, parent->subFolderNodes())
        gatherFileNodes(folder, list);
    foreach (ProjectExplorer::FileNode *file, parent->fileNodes())
        list.append(file);
}

bool sortNodesByPath(Node *a, Node *b)
{
    return a->path() < b->path();
}

void CMakeProject::buildTree(CMakeProjectNode *rootNode, QList<ProjectExplorer::FileNode *> newList)
{
    // Gather old list
    QList<ProjectExplorer::FileNode *> oldList;
    gatherFileNodes(rootNode, oldList);
    Utils::sort(oldList, sortNodesByPath);
    Utils::sort(newList, sortNodesByPath);

    QList<ProjectExplorer::FileNode *> added;
    QList<ProjectExplorer::FileNode *> deleted;

    ProjectExplorer::compareSortedLists(oldList, newList, deleted, added, sortNodesByPath);

    qDeleteAll(ProjectExplorer::subtractSortedList(newList, added, sortNodesByPath));

    // add added nodes
    foreach (ProjectExplorer::FileNode *fn, added) {
//        qDebug()<<"added"<<fn->path();
        // Get relative path to rootNode
        QString parentDir = fn->path().toFileInfo().absolutePath();
        ProjectExplorer::FolderNode *folder = findOrCreateFolder(rootNode, parentDir);
        folder->addFileNodes(QList<ProjectExplorer::FileNode *>()<< fn);
    }

    // remove old file nodes and check whether folder nodes can be removed
    foreach (ProjectExplorer::FileNode *fn, deleted) {
        ProjectExplorer::FolderNode *parent = fn->parentFolderNode();
//        qDebug()<<"removed"<<fn->path();
        parent->removeFileNodes(QList<ProjectExplorer::FileNode *>() << fn);
        // Check for empty parent
        while (parent->subFolderNodes().isEmpty() && parent->fileNodes().isEmpty()) {
            ProjectExplorer::FolderNode *grandparent = parent->parentFolderNode();
            grandparent->removeFolderNodes(QList<ProjectExplorer::FolderNode *>() << parent);
            parent = grandparent;
            if (parent == rootNode)
                break;
        }
    }
}

ProjectExplorer::FolderNode *CMakeProject::findOrCreateFolder(CMakeProjectNode *rootNode, QString directory)
{
    FileName path = rootNode->path().parentDir();
    QDir rootParentDir(path.toString());
    QString relativePath = rootParentDir.relativeFilePath(directory);
    QStringList parts = relativePath.split(QLatin1Char('/'), QString::SkipEmptyParts);
    ProjectExplorer::FolderNode *parent = rootNode;
    foreach (const QString &part, parts) {
        path.appendPath(part);
        // Find folder in subFolders
        bool found = false;
        foreach (ProjectExplorer::FolderNode *folder, parent->subFolderNodes()) {
            if (folder->path() == path) {
                // yeah found something :)
                parent = folder;
                found = true;
                break;
            }
        }
        if (!found) {
            // No FolderNode yet, so create it
            ProjectExplorer::FolderNode *tmp = new ProjectExplorer::FolderNode(path);
            tmp->setDisplayName(part);
            parent->addFolderNodes(QList<ProjectExplorer::FolderNode *>() << tmp);
            parent = tmp;
        }
    }
    return parent;
}

QString CMakeProject::displayName() const
{
    return m_projectName;
}

Core::IDocument *CMakeProject::document() const
{
    return m_file;
}

IProjectManager *CMakeProject::projectManager() const
{
    return m_manager;
}

ProjectExplorer::ProjectNode *CMakeProject::rootProjectNode() const
{
    return m_rootNode;
}


QStringList CMakeProject::files(FilesMode fileMode) const
{
    Q_UNUSED(fileMode)
    return m_files;
}

Project::RestoreResult CMakeProject::fromMap(const QVariantMap &map, QString *errorMessage)
{
    RestoreResult result = Project::fromMap(map, errorMessage);
    if (result != RestoreResult::Ok)
        return result;

    bool hasUserFile = activeTarget();
    if (!hasUserFile) {
        CMakeOpenProjectWizard copw(Core::ICore::mainWindow(), m_manager, projectDirectory().toString(), Environment::systemEnvironment());
        if (copw.exec() != QDialog::Accepted)
            return RestoreResult::UserAbort;
        Kit *k = copw.kit();
        Target *t = new Target(this, k);
        CMakeBuildConfiguration *bc(new CMakeBuildConfiguration(t));
        bc->setDefaultDisplayName(QLatin1String("all"));
        bc->setUseNinja(copw.useNinja());
        bc->setBuildDirectory(FileName::fromString(copw.buildDirectory()));
        ProjectExplorer::BuildStepList *buildSteps = bc->stepList(ProjectExplorer::Constants::BUILDSTEPS_BUILD);
        ProjectExplorer::BuildStepList *cleanSteps = bc->stepList(ProjectExplorer::Constants::BUILDSTEPS_CLEAN);

        // Now create a standard build configuration
        buildSteps->insertStep(0, new MakeStep(buildSteps));

        MakeStep *cleanMakeStep = new MakeStep(cleanSteps);
        cleanSteps->insertStep(0, cleanMakeStep);
        cleanMakeStep->setAdditionalArguments(QLatin1String("clean"));
        cleanMakeStep->setClean(true);

        t->addBuildConfiguration(bc);

        t->updateDefaultDeployConfigurations();

        addTarget(t);
    } else {
        // We have a user file, but we could still be missing the cbp file
        // or simply run createXml with the saved settings
        CMakeBuildConfiguration *activeBC = qobject_cast<CMakeBuildConfiguration *>(activeTarget()->activeBuildConfiguration());
        if (!activeBC) {
            *errorMessage = tr("Internal Error: No build configuration found in settings file.");
            return RestoreResult::Error;
        }
        QString cbpFile = CMakeManager::findCbpFile(QDir(activeBC->buildDirectory().toString()));
        QFileInfo cbpFileFi(cbpFile);

        CMakeOpenProjectWizard::Mode mode = CMakeOpenProjectWizard::Nothing;
        if (!cbpFileFi.exists())
            mode = CMakeOpenProjectWizard::NeedToCreate;
        else if (cbpFileFi.lastModified() < m_fileName.toFileInfo().lastModified())
            mode = CMakeOpenProjectWizard::NeedToUpdate;

        if (mode != CMakeOpenProjectWizard::Nothing) {
            CMakeBuildInfo info(activeBC);
            CMakeOpenProjectWizard copw(Core::ICore::mainWindow(), m_manager, mode, &info);
            if (copw.exec() != QDialog::Accepted)
                return RestoreResult::UserAbort;
            else {
                activeBC->setUseNinja(copw.useNinja());
                activeBC->setCMakeParams(copw.arguments());
                activeBC->setCMakeParamsExt(copw.cmakeParamsExt());
            }
        }
    }

    parseCMakeLists();

    m_activeTarget = activeTarget();
    if (m_activeTarget)
        connect(m_activeTarget, SIGNAL(activeBuildConfigurationChanged(ProjectExplorer::BuildConfiguration*)),
                this, SLOT(changeActiveBuildConfiguration(ProjectExplorer::BuildConfiguration*)));

    connect(this, SIGNAL(activeTargetChanged(ProjectExplorer::Target*)),
            this, SLOT(activeTargetWasChanged(ProjectExplorer::Target*)));

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

CMakeBuildTarget CMakeProject::buildTargetForTitle(const QString &title)
{
    foreach (const CMakeBuildTarget &ct, m_buildTargets)
        if (ct.title == title)
            return ct;
    return CMakeBuildTarget();
}

QString CMakeProject::uiHeaderFile(const QString &uiFile)
{
    QFileInfo fi(uiFile);
    FileName project = projectDirectory();
    FileName baseDirectory = FileName::fromString(fi.absolutePath());

    while (baseDirectory.isChildOf(project)) {
        FileName cmakeListsTxt = baseDirectory;
        cmakeListsTxt.appendPath(QLatin1String("CMakeLists.txt"));
        if (cmakeListsTxt.exists())
            break;
        QDir dir(baseDirectory.toString());
        dir.cdUp();
        baseDirectory = FileName::fromString(dir.absolutePath());
    }

    QDir srcDirRoot = QDir(project.toString());
    QString relativePath = srcDirRoot.relativeFilePath(baseDirectory.toString());
    QDir buildDir = QDir(activeTarget()->activeBuildConfiguration()->buildDirectory().toString());
    QString uiHeaderFilePath = buildDir.absoluteFilePath(relativePath);
    uiHeaderFilePath += QLatin1String("/ui_");
    uiHeaderFilePath += fi.completeBaseName();
    uiHeaderFilePath += QLatin1String(".h");

    return QDir::cleanPath(uiHeaderFilePath);
}

void CMakeProject::updateRunConfigurations()
{
    foreach (Target *t, targets())
        updateRunConfigurations(t);
}

void CMakeProject::cbpUpdateFinished(int /*code*/)
{
    if (m_cbpUpdateProcess->exitCode() != 0) {
        cbpUpdateMessage(tr("CMake exited with error. "
                            "Please run CMake wizard manualy and check output"));
    } else {
        refresh();
    }

    m_cbpUpdateProcess->deleteLater();
    m_cbpUpdateProcess = 0;
}

// TODO Compare with updateDefaultRunConfigurations();
void CMakeProject::updateRunConfigurations(Target *t)
{
    // *Update* runconfigurations:
    QMultiMap<QString, CMakeRunConfiguration*> existingRunConfigurations;
    QList<ProjectExplorer::RunConfiguration *> toRemove;
    foreach (ProjectExplorer::RunConfiguration *rc, t->runConfigurations()) {
        if (CMakeRunConfiguration* cmakeRC = qobject_cast<CMakeRunConfiguration *>(rc))
            existingRunConfigurations.insert(cmakeRC->title(), cmakeRC);
        QtSupport::CustomExecutableRunConfiguration *ceRC =
                qobject_cast<QtSupport::CustomExecutableRunConfiguration *>(rc);
        if (ceRC && !ceRC->isConfigured())
            toRemove << rc;
    }

    foreach (const CMakeBuildTarget &ct, buildTargets()) {
        if (ct.targetType != ExecutableType)
            continue;
        if (ct.executable.isEmpty())
            continue;
        QList<CMakeRunConfiguration *> list = existingRunConfigurations.values(ct.title);
        if (!list.isEmpty()) {
            // Already exists, so override the settings...
            foreach (CMakeRunConfiguration *rc, list) {
                rc->setExecutable(ct.executable);
                rc->setBaseWorkingDirectory(ct.workingDirectory);
                rc->setEnabled(true);
            }
            existingRunConfigurations.remove(ct.title);
        } else {
            // Does not exist yet
            Core::Id id = CMakeRunConfigurationFactory::idFromBuildTarget(ct.title);
            CMakeRunConfiguration *rc = new CMakeRunConfiguration(t, id, ct.executable,
                                                                  ct.workingDirectory, ct.title);
            t->addRunConfiguration(rc);
        }
    }
    QMultiMap<QString, CMakeRunConfiguration *>::const_iterator it =
            existingRunConfigurations.constBegin();
    for ( ; it != existingRunConfigurations.constEnd(); ++it) {
        CMakeRunConfiguration *rc = it.value();
        // The executables for those runconfigurations aren't build by the current buildconfiguration
        // We just set a disable flag and show that in the display name
        rc->setEnabled(false);
        // removeRunConfiguration(rc);
    }

    foreach (ProjectExplorer::RunConfiguration *rc, toRemove)
        t->removeRunConfiguration(rc);

    if (t->runConfigurations().isEmpty()) {
        // Oh no, no run configuration,
        // create a custom executable run configuration
        t->addRunConfiguration(new QtSupport::CustomExecutableRunConfiguration(t));
    }
}

void CMakeProject::cbpUpdateMessage(const QString &message, bool show)
{
    Core::IDocument *document = Core::EditorManager::currentDocument();

    if (!document)
        return;

    Core::InfoBar *infoBar = document->infoBar();
    Core::Id id = Core::Id("CMakeProject.UpdateCbp");

    if (!infoBar->canInfoBeAdded(id))
        return;

    if (show) {
        Core::InfoBarEntry info(id, message, Core::InfoBarEntry::GlobalSuppressionEnabled);
        // TODO add custom buttor to run CMake Wizard
        //info.setCustomButtonInfo(tr("Reload QML"), this,
        //                         SLOT(reloadQml()));
        infoBar->addInfo(info);
    }
    else {
        infoBar->removeInfo(id);
    }
}

void CMakeProject::updateCbp()
{
    if (m_cbpUpdateProcess && m_cbpUpdateProcess->state() != QProcess::NotRunning)
        return;

    cbpUpdateMessage(QLatin1String(""), false);

    CMakeBuildConfiguration *bc
            = static_cast<CMakeBuildConfiguration *>(activeTarget()->activeBuildConfiguration());

    CMakeProjectManager::Internal::GeneratorInfo generatorInfo(bc->target()->kit(),
                                                               bc->useNinja());
    
    CMakeTool *cmake = CMakeKitInformation::cmakeTool(generatorInfo.kit());

    if (cmake && cmake->isValid()) {
        m_cbpUpdateProcess = new Utils::QtcProcess();
        connect(m_cbpUpdateProcess, SIGNAL(finished(int)), this, SLOT(cbpUpdateFinished(int)));

        m_manager->createXmlFile(m_cbpUpdateProcess, cmake->cmakeExecutable().toString(),
                                 bc->cmakeParamsExt().arguments(bc->cmakeParams(), bc->buildDirectory().toString()),
                                 bc->target()->project()->projectDirectory().toString(),
                                 bc->buildDirectory().toString(),
                                 bc->environment(),
                                 QString::fromLatin1(generatorInfo.generatorArgument()));
    } else {
        cbpUpdateMessage(tr("Selected Kit has no valid CMake executable specified."));
    }

}

void CMakeProject::updateApplicationAndDeploymentTargets()
{
    Target *t = activeTarget();

    QFile deploymentFile;
    QTextStream deploymentStream;
    QString deploymentPrefix;

    QDir sourceDir(t->project()->projectDirectory().toString());
    QDir buildDir(t->activeBuildConfiguration()->buildDirectory().toString());

    deploymentFile.setFileName(sourceDir.filePath(QLatin1String("QtCreatorDeployment.txt")));
    // If we don't have a global QtCreatorDeployment.txt check for one created by the active build configuration
    if (!deploymentFile.exists())
        deploymentFile.setFileName(buildDir.filePath(QLatin1String("QtCreatorDeployment.txt")));
    if (deploymentFile.open(QFile::ReadOnly | QFile::Text)) {
        deploymentStream.setDevice(&deploymentFile);
        deploymentPrefix = deploymentStream.readLine();
        if (!deploymentPrefix.endsWith(QLatin1Char('/')))
            deploymentPrefix.append(QLatin1Char('/'));
    }

    BuildTargetInfoList appTargetList;
    DeploymentData deploymentData;

    foreach (const CMakeBuildTarget &ct, m_buildTargets) {
        if (ct.executable.isEmpty())
            continue;

        if (ct.targetType == ExecutableType || ct.targetType == DynamicLibraryType)
            deploymentData.addFile(ct.executable, deploymentPrefix + buildDir.relativeFilePath(QFileInfo(ct.executable).dir().path()), DeployableFile::TypeExecutable);
        if (ct.targetType == ExecutableType) {
            // TODO: Put a path to corresponding .cbp file into projectFilePath?
            appTargetList.list << BuildTargetInfo(ct.title,
                                                  FileName::fromString(ct.executable),
                                                  FileName::fromString(ct.executable));
        }
    }

    QString absoluteSourcePath = sourceDir.absolutePath();
    if (!absoluteSourcePath.endsWith(QLatin1Char('/')))
        absoluteSourcePath.append(QLatin1Char('/'));
    if (deploymentStream.device()) {
        while (!deploymentStream.atEnd()) {
            QString line = deploymentStream.readLine();
            if (!line.contains(QLatin1Char(':')))
                continue;
            QStringList file = line.split(QLatin1Char(':'));
            deploymentData.addFile(absoluteSourcePath + file.at(0), deploymentPrefix + file.at(1));
        }
    }

    t->setApplicationTargets(appTargetList);
    t->setDeploymentData(deploymentData);
}

void CMakeProject::createUiCodeModelSupport()
{
    QHash<QString, QString> uiFileHash;

    // Find all ui files
    foreach (const QString &uiFile, m_files) {
        if (uiFile.endsWith(QLatin1String(".ui")))
            uiFileHash.insert(uiFile, uiHeaderFile(uiFile));
    }

    QtSupport::UiCodeModelManager::update(this, uiFileHash);
}



void CMakeBuildTarget::clear()
{
    executable.clear();
    makeCommand.clear();
    makeCleanCommand.clear();
    workingDirectory.clear();
    sourceDirectory.clear();
    title.clear();
    targetType = ExecutableType;
    includeFiles.clear();
    compilerOptions.clear();
    defines.clear();
}

void CMakeProject::getFileList(const QDir &dir,
                               const QString &projectRoot,
                               QStringList *files, QStringList *paths) const
{
    const QFileInfoList fileInfoList = dir.entryInfoList(QDir::Files |
                                                         QDir::Dirs |
                                                         QDir::NoDotAndDotDot |
                                                         QDir::NoSymLinks);

    foreach (const QFileInfo &fileInfo, fileInfoList) {
        QString filePath = fileInfo.absoluteFilePath();

        if (fileInfo.isDir() && isValidDir(fileInfo)) {
            getFileList(QDir(fileInfo.absoluteFilePath()), projectRoot,
                        files, paths);

            if (! paths->contains(filePath))
                paths->append(filePath);
        } else {
            files->append(filePath);
        }
    }
}

bool CMakeProject::isValidDir(const QFileInfo &fileInfo) const
{
    const QString fileName = fileInfo.fileName();
    const QString suffix = fileInfo.suffix();

    if (fileName.startsWith(QLatin1Char('.')))
        return false;

    else if (fileName == QLatin1String("CVS"))
        return false;

    // ### user include/exclude

    return true;
}

void CMakeProject::refresh()
{
    parseCMakeLists();
}

bool CMakeProject::addFiles(const QStringList &filePaths)
{
    Q_UNUSED(filePaths);
    updateCbp();
    return true;
}

bool CMakeProject::eraseFiles(const QStringList &filePaths)
{
    Q_UNUSED(filePaths);
    updateCbp();
    return true;
}

bool CMakeProject::renameFile(const QString &filePath, const QString &newFilePath)
{
    Q_UNUSED(filePath);
    Q_UNUSED(newFilePath);
    updateCbp();
    return true;
}

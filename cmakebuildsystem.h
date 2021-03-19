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

#pragma once

#include "builddirparameters.h"
#include "cmakebuildtarget.h"
#include "cmakeprojectnodes.h"
#include "fileapireader.h"
#include "simplefileapireader.h"

#include <projectexplorer/buildsystem.h>

#include <utils/fileutils.h>
#include <utils/temporarydirectory.h>

#include <QFutureSynchronizer>

namespace ProjectExplorer { class ExtraCompiler; }

namespace CppTools {
class CppProjectUpdater;
} // namespace CppTools

namespace CMakeProjectManager {

class CMakeBuildConfiguration;

namespace Internal {

// --------------------------------------------------------------------
// CMakeBuildSystem:
// --------------------------------------------------------------------

class CMakeBuildSystem final : public ProjectExplorer::BuildSystem
{
    Q_OBJECT

public:
    explicit CMakeBuildSystem(CMakeBuildConfiguration *bc);
    ~CMakeBuildSystem() final;
    
    void triggerParsing() final;

    bool supportsAction(ProjectExplorer::Node *context,
                        ProjectExplorer::ProjectAction action,
                        const ProjectExplorer::Node *node) const final;

    bool addFiles(ProjectExplorer::Node *context,
                  const QStringList &filePaths, QStringList *) final;
    
    QStringList filesGeneratedFrom(const QString &sourceFile) const final;

    // Actions:
    void runCMake();
    void runCMakeAndScanProjectTree();
    void runCMakeWithExtraArguments();

    bool deleteFiles(ProjectExplorer::Node *context,
                     const QStringList &filePaths) final;

    bool canRenameFile(ProjectExplorer::Node *context, const QString &filePath, const QString &newFilePath) final;
    bool renameFile(ProjectExplorer::Node *context, const QString &filePath, const QString &newFilePath) final;
                  

    bool persistCMakeState();
    void clearCMakeCache();

    // Context menu actions:
    void buildCMakeTarget(const QString &buildTarget);

    // Queries:
    const QList<ProjectExplorer::BuildTargetInfo> appTargets() const;
    QStringList buildTargetTitles() const;
    const QList<CMakeBuildTarget> &buildTargets() const;
    ProjectExplorer::DeploymentData deploymentData() const;

    CMakeBuildConfiguration *cmakeBuildConfiguration() const;

    QList<ProjectExplorer::TestCaseInfo> const testcasesInfo() const final;
    Utils::CommandLine commandLineForTests(const QList<QString> &tests,
                                           const QStringList &options) const final;

    // Generic CMake helper functions:
    static CMakeConfig parseCMakeCacheDotTxt(const Utils::FilePath &cacheFile,
                                             QString *errorMessage);

    bool isMultiConfig() const;
    bool usesAllCapsTargets() const;

private:
    bool addFilesPriv(const QStringList &filePaths);
    bool eraseFilesPriv(const QStringList &filePaths);
    bool renameFilePriv(const QString &filePath, const QString &newFilePath);

    // Actually ask for parsing:
    enum ReparseParameters {
        REPARSE_DEFAULT = 0, // Nothing special:-)
        REPARSE_FORCE_CMAKE_RUN
        = (1 << 0), // Force cmake to run, apply extraCMakeArguments if non-empty
        REPARSE_FORCE_INITIAL_CONFIGURATION
        = (1 << 1), // Force initial configuration arguments to cmake
        REPARSE_FORCE_EXTRA_CONFIGURATION = (1 << 2), // Force extra configuration arguments to cmake
        REPARSE_SCAN = (1 << 3),                      // Run filesystem scan
        REPARSE_URGENT = (1 << 4),                    // Do not delay the parser run by 1s
    };
    QString reparseParametersString(int reparseFlags);
    void setParametersAndRequestParse(const BuildDirParameters &parameters,
                                      const int reparseParameters);

    bool mustApplyExtraArguments(const BuildDirParameters &parameters) const;

    // State handling:
    // Parser states:
    void handleParsingSuccess();
    void handleParsingError();

    // Treescanner states:
    void handleTreeScanningFinished();

    // Combining Treescanner and Parser states:
    void combineScanAndParse();

    std::unique_ptr<CMakeProjectNode> generateProjectTree(
        const QList<const ProjectExplorer::FileNode *> &allFiles, bool includeHeadersNode);
    void checkAndReportError(QString &errorMessage);

    void updateCMakeConfiguration(QString &errorMessage);

    void updateProjectData();
    void updateFallbackProjectData();
    QList<ProjectExplorer::ExtraCompiler *> findExtraCompilers();
    void updateQmlJSCodeModel(const QStringList &extraHeaderPaths);

    void handleParsingSucceeded();
    void handleParsingFailed(const QString &msg);

    void wireUpConnections();

    Utils::FilePath workDirectory(const BuildDirParameters &parameters);
    void stopParsingAndClearState();
    void becameDirty();

    void updateReparseParameters(const int parameters);
    int takeReparseParameters();

    void runCTest();

    void writeConfigurationIntoBuildDirectory();

    ProjectExplorer::TreeScanner m_treeScanner;
    QHash<QString, bool> m_mimeBinaryCache;
    QList<const ProjectExplorer::FileNode *> m_allFiles;

    bool m_waitingForScan = false;
    bool m_waitingForParse = false;
    bool m_combinedScanAndParseResult = false;

    ParseGuard m_currentGuard;

    CppTools::CppProjectUpdater *m_cppCodeModelUpdater = nullptr;
    QList<ProjectExplorer::ExtraCompiler *> m_extraCompilers;
    QList<CMakeBuildTarget> m_buildTargets;

    // Parsing state:
    BuildDirParameters m_parameters;
    int m_reparseParameters = REPARSE_DEFAULT;
    mutable std::unordered_map<Utils::FilePath, std::unique_ptr<Utils::TemporaryDirectory>>
        m_buildDirToTempDir;
#if 0
    FileApiReader m_reader;
#else
    SimpleFileApiReader m_reader;
#endif
    mutable bool m_isHandlingError = false;

    // CTest integration
    QString m_ctestPath;
    QList<ProjectExplorer::TestCaseInfo> m_testNames;
    QFutureSynchronizer<QByteArray> m_futureSynchronizer;
};

} // namespace Internal
} // namespace CMakeProjectManager

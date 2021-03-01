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

#include "fileapireader.h"

#include "fileapidataextractor.h"
#include "fileapiparser.h"
#include "projecttreehelper.h"

#include <coreplugin/messagemanager.h>

#include <projectexplorer/projectexplorer.h>

#include <utils/algorithm.h>
#include <utils/runextensions.h>

#include <QDateTime>
#include <QLoggingCategory>

using namespace ProjectExplorer;
using namespace Utils;

namespace CMakeProjectManager {
namespace Internal {

static Q_LOGGING_CATEGORY(cmakeFileApiMode, "qtc.cmake.fileApiMode", QtWarningMsg);

using namespace FileApiDetails;

// --------------------------------------------------------------------
// FileApiReader:
// --------------------------------------------------------------------

FileApiReader::FileApiReader()
    : m_lastReplyTimestamp()
{
    QObject::connect(&m_watcher,
                     &FileSystemWatcher::directoryChanged,
                     this,
                     &FileApiReader::replyDirectoryHasChanged);
}

FileApiReader::~FileApiReader()
{
    if (isParsing())
        emit errorOccurred(tr("Parsing has been canceled."));
    stop();
    resetData();
}

void FileApiReader::setParameters(const BuildDirParameters &p)
{
    qCDebug(cmakeFileApiMode)
        << "\n\n\n\n\n=============================================================\n";

    // Update:
    m_parameters = p;
    qCDebug(cmakeFileApiMode) << "Work directory:" << m_parameters.workDirectory.toUserOutput();

    // Reset watcher:
    m_watcher.removeFiles(m_watcher.files());
    m_watcher.removeDirectories(m_watcher.directories());

    FileApiParser::setupCMakeFileApi(m_parameters.workDirectory, m_watcher);

    resetData();
}

void FileApiReader::resetData()
{
    m_cmakeFiles.clear();
    if (!m_parameters.sourceDirectory.isEmpty())
        m_cmakeFiles.insert(m_parameters.sourceDirectory.pathAppended("CMakeLists.txt"));

    m_cache.clear();
    m_buildTargets.clear();
    m_projectParts.clear();
    m_rootProjectNode.reset();
    m_knownHeaders.clear();
}

void FileApiReader::parse(bool forceCMakeRun,
                          bool forceInitialConfiguration,
                          bool forceExtraConfiguration)
{
    qCDebug(cmakeFileApiMode) << "Parse called with arguments: ForceCMakeRun:" << forceCMakeRun
                              << " - forceConfiguration:" << forceInitialConfiguration
                              << " - forceExtraConfiguration:" << forceExtraConfiguration;
    startState();

    const QStringList args = (forceInitialConfiguration ? m_parameters.initialCMakeArguments
                                                        : QStringList())
                             + (forceExtraConfiguration ? m_parameters.extraCMakeArguments
                                                        : QStringList());
    qCDebug(cmakeFileApiMode) << "Parameters request these CMake arguments:" << args;

    const QFileInfo replyFi = FileApiParser::scanForCMakeReplyFile(m_parameters.workDirectory);
    // Only need to update when one of the following conditions is met:
    //  * The user forces the cmake run,
    //  * The user provided arguments,
    //  * There is no reply file,
    //  * One of the cmakefiles is newer than the replyFile and the user asked
    //    for creator to run CMake as needed,
    //  * A query file is newer than the reply file
    const bool hasArguments = !args.isEmpty();
    const bool replyFileMissing = !replyFi.exists();
    const bool cmakeFilesChanged = m_parameters.cmakeTool() && m_parameters.cmakeTool()->isAutoRun()
                                   && anyOf(m_cmakeFiles, [&replyFi](const FilePath &f) {
                                          return f.toFileInfo().lastModified()
                                                 > replyFi.lastModified();
                                      });
    const bool queryFileChanged = anyOf(FileApiParser::cmakeQueryFilePaths(
                                            m_parameters.workDirectory),
                                        [&replyFi](const QString &qf) {
                                            return QFileInfo(qf).lastModified()
                                                   > replyFi.lastModified();
                                        });

    const bool mustUpdate = forceCMakeRun || hasArguments || replyFileMissing || cmakeFilesChanged
                            || queryFileChanged;
    qCDebug(cmakeFileApiMode) << QString("Do I need to run CMake? %1 "
                                         "(force: %2 | args: %3 | missing reply: %4 | "
                                         "cmakeFilesChanged: %5 | "
                                         "queryFileChanged: %6)")
                                     .arg(mustUpdate)
                                     .arg(forceCMakeRun)
                                     .arg(hasArguments)
                                     .arg(replyFileMissing)
                                     .arg(cmakeFilesChanged)
                                     .arg(queryFileChanged);

    if (mustUpdate) {
        qCDebug(cmakeFileApiMode) << QString("FileApiReader: Starting CMake with \"%1\".")
                                         .arg(args.join("\", \""));
        startCMakeState(args);
    } else {
        endState(replyFi);
    }
}

void FileApiReader::stop()
{
    if (m_cmakeProcess)
        disconnect(m_cmakeProcess.get(), nullptr, this, nullptr);
    m_cmakeProcess.reset();
}

bool FileApiReader::isParsing() const
{
    return m_isParsing;
}

QSet<FilePath> FileApiReader::projectFilesToWatch() const
{
    return m_cmakeFiles;
}

QList<CMakeBuildTarget> FileApiReader::takeBuildTargets(QString &errorMessage){
    Q_UNUSED(errorMessage)

    auto result = std::move(m_buildTargets);
    m_buildTargets.clear();
    return result;
}

CMakeConfig FileApiReader::takeParsedConfiguration(QString &errorMessage)
{
    if (m_lastCMakeExitCode != 0)
        errorMessage = tr("CMake returned error code: %1").arg(m_lastCMakeExitCode);

    CMakeConfig cache = m_cache;
    m_cache.clear();
    return cache;
}

QString FileApiReader::ctestPath() const
{
    // if we failed to run cmake we should not offer ctest information either
    return m_lastCMakeExitCode == 0 ? m_ctestPath : QString();
}

bool FileApiReader::isMultiConfig() const
{
    return m_isMultiConfig;
}

bool FileApiReader::usesAllCapsTargets() const
{
    return m_usesAllCapsTargets;
}

std::unique_ptr<CMakeProjectNode> FileApiReader::generateProjectTree(
    const QList<const FileNode *> &allFiles, QString &errorMessage, bool includeHeaderNodes)
{
    Q_UNUSED(errorMessage)

    if (includeHeaderNodes) {
        addHeaderNodes(m_rootProjectNode.get(), m_knownHeaders, allFiles);
    }
    addFileSystemNodes(m_rootProjectNode.get(), allFiles);
    return std::move(m_rootProjectNode);
}

RawProjectParts FileApiReader::createRawProjectParts(QString &errorMessage)
{
    Q_UNUSED(errorMessage)

    RawProjectParts result = std::move(m_projectParts);
    m_projectParts.clear();
    return result;
}

void FileApiReader::startState()
{
    qCDebug(cmakeFileApiMode) << "FileApiReader: START STATE.";
    QTC_ASSERT(!m_isParsing, return );
    QTC_ASSERT(!m_future.has_value(), return );

    m_isParsing = true;

    qCDebug(cmakeFileApiMode) << "FileApiReader: CONFIGURATION STARTED SIGNAL";
    emit configurationStarted();
}

void FileApiReader::endState(const QFileInfo &replyFi)
{
    qCDebug(cmakeFileApiMode) << "FileApiReader: END STATE.";
    QTC_ASSERT(m_isParsing, return );
    QTC_ASSERT(!m_future.has_value(), return );

    const FilePath sourceDirectory = m_parameters.sourceDirectory;
    const FilePath buildDirectory = m_parameters.workDirectory;
    const FilePath topCmakeFile = m_cmakeFiles.size() == 1 ? *m_cmakeFiles.begin() : FilePath{};
    const QString cmakeBuildType = m_parameters.cmakeBuildType;

    m_lastReplyTimestamp = replyFi.lastModified();

    m_future = runAsync(ProjectExplorerPlugin::sharedThreadPool(),
                        [replyFi, sourceDirectory, buildDirectory, topCmakeFile, cmakeBuildType]() {
                            auto result = std::make_unique<FileApiQtcData>();
                            FileApiData data = FileApiParser::parseData(replyFi, cmakeBuildType, result->errorMessage);
                            if (!result->errorMessage.isEmpty()) {
                                qWarning() << result->errorMessage;
                                *result = generateFallbackData(topCmakeFile,
                                                               sourceDirectory,
                                                               buildDirectory,
                                                               result->errorMessage);
                            } else {
                                *result = extractData(data, sourceDirectory, buildDirectory);
                            }
                            if (!result->errorMessage.isEmpty()) {
                                qWarning() << result->errorMessage;
                            }

                            return result.release();
                        });
    onFinished(m_future.value(), this, [this](const QFuture<FileApiQtcData *> &f) {
        std::unique_ptr<FileApiQtcData> value(f.result()); // Adopt the pointer again:-)

        m_future = {};
        m_isParsing = false;
        m_cache = std::move(value->cache);
        m_cmakeFiles = std::move(value->cmakeFiles);
        m_buildTargets = std::move(value->buildTargets);
        m_projectParts = std::move(value->projectParts);
        m_rootProjectNode = std::move(value->rootProjectNode);
        m_knownHeaders = std::move(value->knownHeaders);
        m_ctestPath = std::move(value->ctestPath);
        m_isMultiConfig = std::move(value->isMultiConfig);
        m_usesAllCapsTargets = std::move(value->usesAllCapsTargets);

        if (value->errorMessage.isEmpty()) {
            emit this->dataAvailable();
        } else {
            emit this->errorOccurred(value->errorMessage);
        }
    });
}

void FileApiReader::makeBackupConfiguration(bool store)
{
    FilePath reply = m_parameters.buildDirectory.pathAppended(".cmake/api/v1/reply");
    FilePath replyPrev = m_parameters.buildDirectory.pathAppended(".cmake/api/v1/reply.prev");
    if (!store)
        std::swap(reply, replyPrev);

    if (reply.exists()) {
        if (replyPrev.exists())
            FileUtils::removeRecursively(replyPrev);
        QDir dir;
        if (!dir.rename(reply.toString(), replyPrev.toString()))
            Core::MessageManager::writeFlashing(tr("Failed to rename %1 to %2.")
                                                .arg(reply.toString(), replyPrev.toString()));

    }

    FilePath cmakeCacheTxt = m_parameters.buildDirectory.pathAppended("CMakeCache.txt");
    FilePath cmakeCacheTxtPrev = m_parameters.buildDirectory.pathAppended("CMakeCache.txt.prev");
    if (!store)
        std::swap(cmakeCacheTxt, cmakeCacheTxtPrev);

    if (!FileUtils::copyIfDifferent(cmakeCacheTxt, cmakeCacheTxtPrev))
        Core::MessageManager::writeFlashing(tr("Failed to copy %1 to %2.")
                                            .arg(cmakeCacheTxt.toString(), cmakeCacheTxtPrev.toString()));

}

void FileApiReader::startCMakeState(const QStringList &configurationArguments)
{
    qCDebug(cmakeFileApiMode) << "FileApiReader: START CMAKE STATE.";
    QTC_ASSERT(!m_cmakeProcess, return );

    m_cmakeProcess = std::make_unique<CMakeProcess>();

    connect(m_cmakeProcess.get(), &CMakeProcess::finished, this, &FileApiReader::cmakeFinishedState);

    qCDebug(cmakeFileApiMode) << ">>>>>> Running cmake with arguments:" << configurationArguments;
    makeBackupConfiguration(true);
    m_cmakeProcess->run(m_parameters, configurationArguments);
}

void FileApiReader::cmakeFinishedState(int code, QProcess::ExitStatus status)
{
    qCDebug(cmakeFileApiMode) << "FileApiReader: CMAKE FINISHED STATE.";

    Q_UNUSED(code)
    Q_UNUSED(status)

    m_lastCMakeExitCode = m_cmakeProcess->lastExitCode();
    m_cmakeProcess.release()->deleteLater();

    if (m_lastCMakeExitCode != 0)
        makeBackupConfiguration(false);

    endState(FileApiParser::scanForCMakeReplyFile(m_parameters.workDirectory));
}

void FileApiReader::replyDirectoryHasChanged(const QString &directory) const
{
    if (m_isParsing)
        return; // This has been triggered by ourselves, ignore.

    const QFileInfo fi = FileApiParser::scanForCMakeReplyFile(m_parameters.workDirectory);
    const QString dir = fi.absolutePath();
    if (dir.isEmpty())
        return; // CMake started to fill the result dir, but has not written a result file yet
    QTC_ASSERT(dir == directory, return);

    if (m_lastReplyTimestamp.isValid() && fi.lastModified() > m_lastReplyTimestamp)
        emit dirty();
}

} // namespace Internal
} // namespace CMakeProjectManager

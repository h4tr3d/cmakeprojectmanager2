#include "simplefileapireader.h"

#include "cmakebuildconfiguration.h"
#include "cmakeprojectconstants.h"
#include "cmakeprojectmanager.h"
#include "fileapidataextractor.h"
#include "fileapiparser.h"
#include "projecttreehelper.h"

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/fileiconprovider.h>
#include <coreplugin/messagemanager.h>
#include <coreplugin/progressmanager/progressmanager.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/task.h>
#include <projectexplorer/taskhub.h>
#include <projectexplorer/toolchain.h>

#include <utils/algorithm.h>
#include <utils/optional.h>
#include <utils/qtcassert.h>
#include <utils/runextensions.h>

#include <QDateTime>
#include <QLoggingCategory>

using namespace ProjectExplorer;
using namespace Utils;

namespace CMakeProjectManager {
namespace Internal {

Q_DECLARE_LOGGING_CATEGORY(cmakeFileApiMode);

// --------------------------------------------------------------------
// SimpleFileApiReader:
// --------------------------------------------------------------------

void SimpleFileApiReader::endState(const QFileInfo &replyFi)
{
    qCDebug(cmakeFileApiMode) << "SimpleFileApiReader: END STATE.";
    QTC_ASSERT(m_isParsing, return );
    QTC_ASSERT(!m_future.has_value(), return );

    const FilePath sourceDirectory = m_parameters.sourceDirectory;
    const FilePath buildDirectory = m_parameters.workDirectory;
    const FilePath topCmakeFile = m_cmakeFiles.size() == 1 ? *m_cmakeFiles.begin() : FilePath{};
    const QString cmakeBuildType = m_parameters.cmakeBuildType == "Build" ? "" : m_parameters.cmakeBuildType;

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
                                *result = extractData(data, sourceDirectory, buildDirectory, true);
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

std::unique_ptr<CMakeProjectNode> SimpleFileApiReader::generateProjectTree(
    const QList<const FileNode *> &allFiles, QString &errorMessage, bool /*includeHeaderNodes*/)
{
    Q_UNUSED(errorMessage)

    QSet<Utils::FilePath> alreadyListed;

    // Cache is needed to reload tree without CMake run
    if (m_rootProjectNode) {
        m_topLevelNameCache = m_rootProjectNode->displayName();
        m_filesCache.clear();

        // Files already added:
        qCDebug(cmakeFileApiMode) << "SimpleFileApiReader: fill cache:";
        m_rootProjectNode->forEachGenericNode([&alreadyListed,this] (const Node *node) { 
            alreadyListed.insert(node->filePath());
            auto fn = dynamic_cast<const FileNode*>(node);
            // Cache only files
            if (fn) {
                m_filesCache.push_back(std::make_tuple(node->filePath(), fn->fileType(), node->isGenerated()));
                qCDebug(cmakeFileApiMode) << "  cache:" << node->filePath();
            }
        });
    } else {
        // Restore from cache
        m_rootProjectNode = std::make_unique<CMakeProjectNode>(m_parameters.sourceDirectory);
        m_rootProjectNode->setDisplayName(m_topLevelNameCache);

        std::vector<std::unique_ptr<FileNode>> files;

        files.reserve(m_filesCache.count());
        for (auto it = m_filesCache.begin(); it != m_filesCache.end();) {
            //qDebug() << "try:" << std::get<0>(*it) << std::get<0>(*it).exists();
            if (!std::get<0>(*it).exists()) {
                it = m_filesCache.erase(it);
                continue;
            }

            alreadyListed.insert(std::get<0>(*it));

            auto node = std::make_unique<FileNode>(std::get<0>(*it), std::get<1>(*it));
            node->setIsGenerated(std::get<2>(*it));
            files.emplace_back(std::move(node));
            ++it;
        }

        m_rootProjectNode->addNestedNodes(std::move(files), m_parameters.sourceDirectory);
    }

    QList<const FileNode *> added = 
        Utils::filtered(allFiles, [&alreadyListed](const FileNode *fn) -> bool {
            const int count = alreadyListed.count();
            alreadyListed.insert(fn->filePath());
            return (alreadyListed.count() != count);
        });
    
    auto addedNodes = Utils::transform<std::vector>(added, [](const FileNode *fn) {
        return std::unique_ptr<FileNode>(fn->clone());
    });

    m_rootProjectNode->addNestedNodes(std::move(addedNodes), m_parameters.sourceDirectory);

    return std::move(m_rootProjectNode);
}

RawProjectParts SimpleFileApiReader::createRawProjectParts(QString &errorMessage)
{
    Q_UNUSED(errorMessage)

    // Keep for the future calls
    return m_projectParts;
}


} // namespace Internal
} // namespace CMakeProjectManager

#include "simplefileapireader.h"

#include "cmakebuildconfiguration.h"
#include "cmakeprojectconstants.h"
#include "cmakeprojectmanager.h"
#include "fileapidataextractor.h"
#include "fileapiparser.h"
#include "projecttreehelper.h"

#include <coreplugin/editormanager/editormanager.h>
#include <utils/fsengine/fileiconprovider.h>
#include <coreplugin/messagemanager.h>
#include <coreplugin/progressmanager/progressmanager.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectexplorerconstants.h>
#include <projectexplorer/task.h>
#include <projectexplorer/taskhub.h>
#include <projectexplorer/toolchain.h>

#include <utils/algorithm.h>
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

SimpleFileApiReader::SimpleFileApiReader()
    : FileApiReader()
{
    m_isPlain = true;
}

std::unique_ptr<CMakeProjectNode> SimpleFileApiReader::rootProjectNode(
    const ProjectExplorer::TreeScanner::Result &allFiles, bool failedToParse)
{
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

    QList<FileNode *> added =
        Utils::filtered(allFiles.allFiles, [&alreadyListed](const FileNode *fn) -> bool {
            const int count = alreadyListed.count();
            alreadyListed.insert(fn->filePath());
            return (alreadyListed.count() != count);
        });
    
    auto addedNodes = Utils::transform<std::vector>(added, [](const FileNode *fn) {
        return std::unique_ptr<FileNode>(fn->clone());
    });

    // TBD: optimize here with allFiles.folderNode and use just an addNode()
    m_rootProjectNode->addNestedNodes(std::move(addedNodes), m_parameters.sourceDirectory);

    return std::exchange(m_rootProjectNode, {});
}

RawProjectParts SimpleFileApiReader::createRawProjectParts(QString &errorMessage)
{
    Q_UNUSED(errorMessage)
    
    if (!m_projectParts.empty())
        m_projectPartsCache = std::exchange(m_projectParts, {});

    return m_projectPartsCache;
}


} // namespace Internal
} // namespace CMakeProjectManager

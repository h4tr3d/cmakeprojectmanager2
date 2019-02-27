#include "simpleservermodereader.h"

#include "cmakeprojectnodes.h"

#include "utils/algorithm.h"
#include "projectexplorer/projectnodes.h"

#include <vector>

#include <QDebug>

using namespace ProjectExplorer;
using namespace Utils;

namespace CMakeProjectManager {
namespace Internal {

void SimpleServerModeReader::generateProjectTree(CMakeProjectNode *root, const QList<const ProjectExplorer::FileNode *> &allFiles)
{
    const Project *topLevel = Utils::findOrDefault(m_projects, [this](const Project *p) {
        return m_parameters.sourceDirectory == p->sourceDirectory;
    });
    if (topLevel)
        m_topLevelNameCache = topLevel->name;

    root->setDisplayName(m_topLevelNameCache);

    // Compose sources list from the CMake data
    auto files = std::move(m_cmakeInputsFileNodes);
    m_cmakeInputsFileNodes.clear(); // Clean out, they are not going to be used anymore!
    QSet<Utils::FileName> alreadyListed;
    for (auto project : m_projects) {
        for (auto target : project->targets) {
            for (auto group : target->fileGroups) {
                const QList<FileName> newSources = Utils::filtered(group->sources, [&alreadyListed](const Utils::FileName &fn) {
                    const int count = alreadyListed.count();
                    alreadyListed.insert(fn);
                    return count != alreadyListed.count();
                });
                files.reserve(files.size() + newSources.count());
                for (auto const &fn : newSources) {
                    auto node = new FileNode(fn, FileType::Source);
                    node->setIsGenerated(group->isGenerated);
                    files.emplace_back(node);
                }
            }
        }
    }

    // Update tree look after adding files without CMake parsing. Otherwise all files will be look like non-project one
    if (files.empty()) {
        files.reserve(m_filesCache.count());
        for (auto it = m_filesCache.begin(); it != m_filesCache.end();) {
            //qDebug() << "try:" << std::get<0>(*it) << std::get<0>(*it).exists();
            if (!std::get<0>(*it).exists()) {
                it = m_filesCache.erase(it);
                continue;
            }
            auto node = new FileNode(std::get<0>(*it), std::get<1>(*it));
            node->setIsGenerated(std::get<2>(*it));
            files.emplace_back(node);
            ++it;
        }
    } else {
        m_filesCache = Utils::transform<QList>(files, [](const std::unique_ptr<FileNode>& node) {
            return std::make_tuple(node->filePath(), node->fileType(), node->isGenerated());
        });
    }

    auto alreadySeen = Utils::transform<QSet>(files, &FileNode::filePath);
    QList<const FileNode *> added = 
        Utils::filtered(allFiles, [&alreadySeen](const FileNode *fn) -> bool {
            const int count = alreadySeen.count();
            alreadySeen.insert(fn->filePath());
            return (alreadySeen.count() != count);
        });

    auto addedNodes = Utils::transform<std::vector>(added, [](const FileNode *fn) {
        return std::unique_ptr<FileNode>(fn->clone());
    });

    root->addNestedNodes(std::move(files), m_parameters.sourceDirectory);
    root->addNestedNodes(std::move(addedNodes), m_parameters.sourceDirectory);
}

} // namespace Internal
} // namespace CMakeProjectManager

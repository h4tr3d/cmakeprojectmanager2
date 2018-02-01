#include "simpleservermodereader.h"

#include "cmakeprojectnodes.h"

#include "utils/algorithm.h"
#include "projectexplorer/projectnodes.h"

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
    QList<FileNode *> files = m_cmakeInputsFileNodes;
    QSet<Utils::FileName> alreadyListed;
    for (auto project : m_projects) {
        for (auto target : project->targets) {
            for (auto group : target->fileGroups) {
                const QList<FileName> newSources = Utils::filtered(group->sources, [&alreadyListed](const Utils::FileName &fn) {
                    const int count = alreadyListed.count();
                    alreadyListed.insert(fn);
                    return count != alreadyListed.count();
                });
                const QList<FileNode *> newFileNodes = Utils::transform(newSources, [group](const Utils::FileName &fn) {
                    return new FileNode(fn, FileType::Source, group->isGenerated);
                });
                files.append(newFileNodes);
            }
        }
    }

    // Update tree look after adding files without CMake parsing. Otherwise all files will be look like non-project one
    if (files.isEmpty()) {
        for (auto it = m_filesCache.begin(); it != m_filesCache.end();) {
            //qDebug() << "try:" << std::get<0>(*it) << std::get<0>(*it).exists();
            if (!std::get<0>(*it).exists()) {
                it = m_filesCache.erase(it);
                continue;
            }
            files.append(new FileNode(std::get<0>(*it), std::get<1>(*it), std::get<2>(*it)));
            ++it;
        }
    } else {
        // Keep list sorted and remove dups
        Utils::sort(files, Node::sortByPath);

        m_filesCache = Utils::transform(files, [](const FileNode *node) {
            return std::make_tuple(node->filePath(), node->fileType(), node->isGenerated());
        });
    }

    m_cmakeInputsFileNodes.clear(); // Clean out, they are not going to be used anymore!

    QList<const FileNode *> added;
    std::set_difference(
        allFiles.begin(),
        allFiles.end(),
        files.begin(),
        files.end(),
        std::back_inserter(added),
        Node::sortByPath
    );

    QList<FileNode *> fileNodes = files + Utils::transform(added, [](const FileNode *fn) {
        return fn->clone();
    });

    root->addNestedNodes(fileNodes, m_parameters.sourceDirectory);
}

} // namespace Internal
} // namespace CMakeProjectManager

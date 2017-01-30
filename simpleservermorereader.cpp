#include "simpleservermorereader.h"

#include "cmakeprojectnodes.h"

#include "utils/algorithm.h"
#include "projectexplorer/projectnodes.h"

#include <QDebug>

using namespace ProjectExplorer;
using namespace Utils;

namespace CMakeProjectManager {
namespace Internal {

void SimpleServerMoreReader::generateProjectTree(CMakeListsNode *root, const QList<const ProjectExplorer::FileNode *> &allFiles)
{
    if (m_projects.empty())
        return;

    root->setDisplayName(m_projects.at(0)->name);

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

    // Keep list sorted and remove dups
    Utils::sort(files, Node::sortByPath);

    m_cmakeInputsFileNodes.clear(); // Clean out, they are not going to be used anymore!

    QList<const FileNode *> added;
    QList<FileNode *> deleted; // Unused!
    ProjectExplorer::compareSortedLists(files, allFiles, deleted, added, Node::sortByPath);

    QList<FileNode *> fileNodes = files + Utils::transform(added, [](const FileNode *fn) {
        auto added = new FileNode(fn->filePath(), fn->fileType(), fn->isGenerated());
        added->setEnabled(fn->isEnabled());
        return added;
    });

    for (auto fn : allFiles) {
        qDebug() << "AllFiles:" << fn->filePath() << ", enabled:" << fn->isEnabled();
    }

    root->buildTree(fileNodes, m_parameters.sourceDirectory);
}

} // namespace Internal
} // namespace CMakeProjectManager

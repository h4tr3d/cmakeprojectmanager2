#include "simpleservermorereader.h"

#include "cmakeprojectnodes.h"

#include "utils/algorithm.h"
#include "projectexplorer/projectnodes.h"

#include <QDebug>

using namespace ProjectExplorer;
using namespace Utils;

namespace CMakeProjectManager {
namespace Internal {

void SimpleServerMoreReader::generateProjectTree(CMakeProjectNode *root, const QList<const ProjectExplorer::FileNode *> &allFiles)
{
    const Project *topLevel = Utils::findOrDefault(m_projects, [this](const Project *p) {
        return m_parameters.sourceDirectory == p->sourceDirectory;
    });
    if (topLevel)
        root->setDisplayName(topLevel->name);


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

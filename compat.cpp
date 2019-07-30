#include "compat.h"

#include <utils/algorithm.h>
#include <utils/hostosinfo.h>
#include <projectexplorer/projectnodes.h>

#include <QDir>

using namespace ProjectExplorer;

namespace CMakeProjectManager {
namespace Compat {
namespace ProjectExplorer {

FileNode *recursiveFileNode(ProjectNode *projectNode, const Utils::FilePath &file, const Utils::FilePath &overrideBaseDir)
{
    auto dir = file.parentDir();

    auto baseDir = overrideBaseDir.isEmpty() ? projectNode->filePath() : overrideBaseDir;

    const QDir thisDir(baseDir.toString());
    QString relativePath = thisDir.relativeFilePath(dir.toString());
    if (relativePath == ".")
        relativePath.clear();
    QStringList parts = relativePath.split('/', QString::SkipEmptyParts);
    const ::ProjectExplorer::FolderNode *parent = projectNode;
    auto probeDir = baseDir;
    foreach (const QString &part, parts) {
        probeDir.pathAppended(part);
        // Find folder in subFolders
        parent = Utils::findOrDefault(parent->folderNodes(), [&probeDir](const FolderNode *fn) {
            return fn->filePath() == probeDir;
        });
        if (!parent)
            return nullptr;
    }

    return parent->fileNode(file);
}

} // ::ProjectExplorer
} // ::Compat
} // ::CMakeProjectManager

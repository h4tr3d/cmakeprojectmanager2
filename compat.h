#pragma once

#include <utils/fileutils.h>

namespace ProjectExplorer {
class FileNode;
class FolderNode;
class ProjectNode;
}

namespace CMakeProjectManager {
namespace Compat {
namespace ProjectExplorer {

::ProjectExplorer::FileNode *recursiveFileNode(::ProjectExplorer::ProjectNode *projectNode,
                                               const Utils::FileName &file,
                                               const Utils::FileName &overrideBaseDir = Utils::FileName());

// Drops by 30bf80162936403f8eefade035cb8036b1c4f370 (ProjectExplorer: Further tree node related simplification) in upstream
void removeFileNode(::ProjectExplorer::FolderNode *folder, ::ProjectExplorer::FileNode *file);

} // ::ProjectExplorer
} // ::Compat
} // ::CMakeProjectManager
